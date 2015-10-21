/*
 * Copyright 2013 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: pulkitg@google.com (Pulkit Goyal)

#include "net/instaweb/http/public/async_fetch_with_lock.h"

#include "base/logging.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

namespace {

const int kDefaultLockTimoutMs = 2 * Timer::kMinuteMs;
const int kLockTimeoutSlackMs = 2 * Timer::kMinuteMs;

}  // namespace

AsyncFetchWithLock::AsyncFetchWithLock(
    const Hasher* hasher,
    const RequestContextPtr& request_context,
    const GoogleString& url,
    const GoogleString& cache_key,
    NamedLockManager* lock_manager,
    MessageHandler* message_handler)
    : AsyncFetch(request_context),
      lock_manager_(lock_manager),
      lock_hasher_(hasher),
      url_(url),
      cache_key_(cache_key),
      message_handler_(message_handler) {
}

AsyncFetchWithLock::~AsyncFetchWithLock() {
  DCHECK(lock_ == NULL) << "Fetch is completed without deleting the lock for "
                        << "cache key: " << cache_key_ << "url: " << url_;
}

void AsyncFetchWithLock::Start(UrlAsyncFetcher* fetcher) {
  lock_.reset(MakeInputLock(cache_key()));

  int64 lock_timeout = fetcher->timeout_ms();
  if (lock_timeout == UrlAsyncFetcher::kUnspecifiedTimeout) {
    // Even if the fetcher never explicitly times out requests, they probably
    // won't succeed after more than 2 minutes.
    lock_timeout = kDefaultLockTimoutMs;
  } else {
    // Give a little slack for polling, writing the file, freeing the lock.
    lock_timeout += kLockTimeoutSlackMs;
  }

  lock_->LockTimedWaitStealOld(0 /* wait_ms */, lock_timeout,
                               MakeFunction(this,
                                            &AsyncFetchWithLock::LockAcquired,
                                            &AsyncFetchWithLock::LockFailed,
                                            fetcher));
}

void AsyncFetchWithLock::LockFailed(UrlAsyncFetcher* fetcher) {
  // lock_name will be needed after lock is deleted.
  GoogleString lock_name(lock_->name());
  lock_.reset(NULL);
  // TODO(abliss): a per-unit-time statistic would be useful here.
  if (ShouldYieldToRedundantFetchInProgress()) {
    message_handler_->Message(
        kInfo, "%s is already being fetched (lock %s)",
        cache_key().c_str(), lock_name.c_str());
    Finalize(true /* lock_failure */, false /* success */);
    delete this;
  } else {
    message_handler_->Message(
        kInfo, "%s is being re-fetched asynchronously "
        "(lock %s held elsewhere)", cache_key().c_str(), lock_name.c_str());
    StartFetch(fetcher, message_handler_);
  }
}

void AsyncFetchWithLock::LockAcquired(UrlAsyncFetcher* fetcher) {
  StartFetch(fetcher, message_handler_);
}

void AsyncFetchWithLock::HandleDone(bool success) {
  if (lock_.get() != NULL) {
    lock_->Unlock();
    lock_.reset(NULL);
  }
  Finalize(false /* lock_failure */, success /* success */);
  delete this;
}

void AsyncFetchWithLock::HandleHeadersComplete() {
}

bool AsyncFetchWithLock::HandleWrite(
    const StringPiece& content, MessageHandler* handler) {
  return true;
}

bool AsyncFetchWithLock::HandleFlush(MessageHandler* handler) {
  return true;
}

void AsyncFetchWithLock::Finalize(bool lock_failure, bool success) {
}

NamedLock* AsyncFetchWithLock::MakeInputLock(const GoogleString& url) {
  return MakeInputLock(url, lock_hasher_, lock_manager_);
}

NamedLock* AsyncFetchWithLock::MakeInputLock(const GoogleString& url,
                                             const Hasher* hasher,
                                             NamedLockManager* lock_manager) {
  const char kLockSuffix[] = ".lock";

  GoogleString lock_name = StrCat(hasher->Hash(url), kLockSuffix);
  return lock_manager->CreateNamedLock(lock_name);
}

}  // namespace net_instaweb
