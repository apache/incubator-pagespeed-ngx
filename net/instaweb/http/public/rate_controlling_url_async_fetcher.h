/*
 * Copyright 2012 Google Inc.
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

// Author: nikhilmadan@google.com (Nikhil Madan)

#ifndef NET_INSTAWEB_HTTP_PUBLIC_RATE_CONTROLLING_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_RATE_CONTROLLING_URL_ASYNC_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class RateController;
class Statistics;
class ThreadSystem;

// Fetcher that uses RateController to limit amount of background fetches
// we direct to a fetcher it wraps per domain. See RateController documentation
// for more details.
class RateControllingUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  // Does not take ownership of 'fetcher'.
  // RateController::InitStats must have been called during stats initialization
  // phase.
  RateControllingUrlAsyncFetcher(UrlAsyncFetcher* fetcher,
                                 int max_global_queue_size,
                                 int per_host_outgoing_request_threshold,
                                 int per_host_queued_request_threshold,
                                 ThreadSystem* thread_system,
                                 Statistics* statistics);

  virtual ~RateControllingUrlAsyncFetcher();

  virtual bool SupportsHttps() const {
    return base_fetcher_->SupportsHttps();
  }

  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  virtual void ShutDown();

 private:
  UrlAsyncFetcher* base_fetcher_;
  scoped_ptr<RateController> rate_controller_;

  DISALLOW_COPY_AND_ASSIGN(RateControllingUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_RATE_CONTROLLING_URL_ASYNC_FETCHER_H_
