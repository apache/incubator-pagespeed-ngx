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


#ifndef NET_INSTAWEB_HTTP_PUBLIC_ASYNC_FETCH_WITH_LOCK_H_
#define NET_INSTAWEB_HTTP_PUBLIC_ASYNC_FETCH_WITH_LOCK_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

class Hasher;
class MessageHandler;
class NamedLock;
class NamedLockManager;
class UrlAsyncFetcher;

// Creates an AsyncFetch object which tries to acquire lock before fetching
// content. Start() will returns false, if it fails to acquire lock.
// Note that acquiring a lock will fail if same resource is fetching somewhere
// else.
// Caller will call the Start() which will try to acquire a lock and internally
// call StartFetch() which actually triggers a fetch. Sequence of the events:
// 1) Caller calls AsyncFetchWithLock::Start().
// 2) Start() will try to acquire lock. If lock is acquired successfully,
//    AsyncFetchWithLock::StartFetch() will be called, otherwise
//    AsyncFetchWithLock::Finalize() is called with lock_failure as true and
//    success as false and StartFetch() returns false and async_fetch_with_lock
//    object will be deleted.
//    Note: StartFetch() will be called in case of lock failure only if
//    ShouldYieldToRedundantFetchInProgress() returns false.
// 3) Subclass defines StartFetch() function which actually triggers
//    UrlAsyncFetcher::Fetch().
// 4) Subclass can override HandleHeadersComplete(), HandleWrite(),
//    HandleFlush() and HandleDone() for special handling during fetch.
//    HandleDone() also releases the lock.
//    Note: If any of these functions is overridden, then
//    AsyncFetchWithLock::HandleXXX should also be called.
// 5) Lastly AsyncFetchWithLock::Finalize() is called just before async_fetch
//    delete itself.
class AsyncFetchWithLock : public AsyncFetch {
 public:
  AsyncFetchWithLock(const Hasher* hasher,
                     const RequestContextPtr& request_context,
                     const GoogleString& url,
                     NamedLockManager* lock_manager,
                     MessageHandler* message_handler);
  virtual ~AsyncFetchWithLock();

  // This will first try to acquire lock and triggers fetch by calling
  // StartFetch() if successful.
  // Returns false, if it fails to acquire lock and deletes the
  // async_fetch_with_lock.
  static bool Start(
      UrlAsyncFetcher* fetcher,
      AsyncFetchWithLock* async_fetch_with_lock,
      MessageHandler* handler);

  // Url to be fetched.
  const GoogleString& url() const { return url_; }

 protected:
  // If someone is already fetching this resource, should we yield to them and
  // try again later?  If so, return true.  Otherwise, if we must fetch the
  // resource regardless, return false.
  virtual bool ShouldYieldToRedundantFetchInProgress() = 0;

  // Finalize is called either when we fail to acquire acquire a lock or
  // at the end of request after releasing the lock.
  virtual void Finalize(bool lock_failure, bool success);

  // StartFetch() will be called after the lock is acquired. The subclass
  // implements this function and is responsible for UrlAsyncFetcher::Fetch().
  virtual bool StartFetch(
     UrlAsyncFetcher* fetcher, MessageHandler* handler) = 0;

  // Releases the lock.
  // If subclass overrides the function, then, it should also call
  // AsyncFetchWithLock::HandleDone()
  virtual void HandleDone(bool success);

  // HandleHeadersComplete(), HandleWrite() and HandleFlush() are no-op
  // functions and any special handling can be done in subclass and must call
  // the superclass function before returning.
  virtual void HandleHeadersComplete();
  virtual bool HandleWrite(
      const StringPiece& content, MessageHandler* handler);
  virtual bool HandleFlush(MessageHandler* handler);

 private:
  // Makes a lock used for fetching.
  NamedLock* MakeInputLock(const GoogleString& url);

  // Set the lock which is acquired during the fetch.
  void set_lock(NamedLock* lock) { lock_.reset(lock); }

  NamedLockManager* lock_manager_;  // Owned by server_context.
  scoped_ptr<NamedLock> lock_;
  const Hasher* lock_hasher_;  // Used to compute named lock names.
  GoogleString url_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFetchWithLock);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_ASYNC_FETCH_WITH_LOCK_H_
