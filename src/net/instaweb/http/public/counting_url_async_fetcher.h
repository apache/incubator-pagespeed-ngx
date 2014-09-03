/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)
//
// Wraps an asynchronous fetcher, but keeps track of success/failure count.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_COUNTING_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_COUNTING_URL_ASYNC_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;

class CountingUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  // TODO(hujie): We should pass in the mutex at all call-sites instead of
  //     creating a new mutex here.
  explicit CountingUrlAsyncFetcher(UrlAsyncFetcher* fetcher)
      : fetcher_(fetcher),
        thread_system_(Platform::CreateThreadSystem()),
        mutex_(thread_system_->NewMutex()) {
    Clear();
  }
  virtual ~CountingUrlAsyncFetcher();

  void set_fetcher(UrlAsyncFetcher* fetcher) { fetcher_ = fetcher; }

  virtual bool SupportsHttps() const { return fetcher_->SupportsHttps(); }

  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  // number of completed fetches.
  int fetch_count() const {
    ScopedMutex lock(mutex_.get());
    return fetch_count_;
  }

  // number of started fetches
  int fetch_start_count() const {
    ScopedMutex lock(mutex_.get());
    return fetch_start_count_;
  }
  int byte_count() const {
    ScopedMutex lock(mutex_.get());
    return byte_count_;
  }
  int failure_count() const {
    ScopedMutex lock(mutex_.get());
    return failure_count_;
  }
  GoogleString most_recent_fetched_url() const {
    ScopedMutex lock(mutex_.get());
    return most_recent_fetched_url_;
  }

  void Clear();

  class CountingFetch;
  friend class CountingFetch;

 private:
  UrlAsyncFetcher* fetcher_;
  int fetch_count_;
  int fetch_start_count_;
  int byte_count_;
  int failure_count_;
  GoogleString most_recent_fetched_url_;
  scoped_ptr<ThreadSystem> thread_system_;  // Thread system for mutex.
  scoped_ptr<AbstractMutex> mutex_;         // Mutex Protect.

  DISALLOW_COPY_AND_ASSIGN(CountingUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_COUNTING_URL_ASYNC_FETCHER_H_
