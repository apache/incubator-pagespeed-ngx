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

#include "net/instaweb/http/public/rate_controlling_url_async_fetcher.h"

#include "net/instaweb/http/public/rate_controller.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

RateControllingUrlAsyncFetcher::RateControllingUrlAsyncFetcher(
    UrlAsyncFetcher* fetcher,
    int max_global_queue_size,
    int per_host_outgoing_request_threshold,
    int per_host_queued_request_threshold,
    ThreadSystem* thread_system,
    Statistics* statistics)
    : base_fetcher_(fetcher),
      rate_controller_(new RateController(
          max_global_queue_size,
          per_host_outgoing_request_threshold,
          per_host_queued_request_threshold,
          thread_system,
          statistics)) {
}

RateControllingUrlAsyncFetcher::~RateControllingUrlAsyncFetcher() {
}

void RateControllingUrlAsyncFetcher::Fetch(const GoogleString& url,
                                           MessageHandler* message_handler,
                                           AsyncFetch* fetch) {
  rate_controller_->Fetch(base_fetcher_, url, message_handler, fetch);
}

void RateControllingUrlAsyncFetcher::ShutDown() {
  // Note: shutting down the controller before the base fetcher serves to
  // workaround a deadlock when base_fetcher_ is SerfUrlAsyncFetcher.
  // The scenario there is that calls into RateController while holding a lock,
  // which then calls Fetch, which tries to grab an another lock and deadlocks
  // against SerfUrlAsyncFetcher::ShutDown, which grabs in the opposite order
  // (the normal convention for the class). Shutting down the rate controller
  // first means we will simply not be trying any more Serf fetches at point
  // that happens before the Serf shutdown.
  rate_controller_->ShutDown();
  base_fetcher_->ShutDown();
}

}  // namespace net_instaweb
