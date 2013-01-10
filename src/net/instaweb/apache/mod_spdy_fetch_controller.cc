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

// Author: morlovich@google.com (Maksim Orlovich)
//
// See the .h for an overview.

#include "net/instaweb/apache/mod_spdy_fetch_controller.h"

#include "net/instaweb/apache/mod_spdy_fetcher.h"
#include "net/instaweb/http/public/rate_controller.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Helper class that inherits off UrlAsyncFetcher that we can pass to
// RateController which will schedule calls to
// ModSpdyFetchController::BlockingFetch on our thread pool.
class ModSpdyFetchController::FetchDispatcher : public UrlAsyncFetcher {
 public:
  FetchDispatcher(ModSpdyFetcher* mod_spdy_fetcher,
                  QueuedWorkerPool* thread_pool)
      : mod_spdy_fetcher_(mod_spdy_fetcher),
        thread_pool_(thread_pool),
        sequence_(NULL) {
  }

  void Fetch(const GoogleString& url,
             MessageHandler* message_handler,
             AsyncFetch* fetch) {
    sequence_ = thread_pool_->NewSequence();
    sequence_->Add(MakeFunction(
        this, &FetchDispatcher::CallBlockingFetchAndFreeSequence,
        url, message_handler, fetch));
  }

 private:
  void CallBlockingFetchAndFreeSequence(
      GoogleString url, MessageHandler* message_handler, AsyncFetch* fetch) {
    // Mark the sequence to get cleaned up once we exit --- it's not going to be
    // deleted right now since we're actually running off it.
    thread_pool_->FreeSequence(sequence_);
    mod_spdy_fetcher_->BlockingFetch(url, message_handler, fetch);
    delete this;
  }

  ModSpdyFetcher* mod_spdy_fetcher_;
  QueuedWorkerPool* thread_pool_;
  QueuedWorkerPool::Sequence* sequence_;

  DISALLOW_COPY_AND_ASSIGN(FetchDispatcher);
};

ModSpdyFetchController::ModSpdyFetchController(
    int num_threads, ThreadSystem* thread_system, Statistics* statistics)
    : rate_controller_(500 * num_threads,  /* max queue size */
                       num_threads,  /* requests per host */
                       500 * num_threads,  /* queued per host */
                       thread_system,
                       statistics),
      thread_pool_(num_threads, "instaweb_spdy_fetch", thread_system) {
}

ModSpdyFetchController::~ModSpdyFetchController() {
}

void ModSpdyFetchController::ScheduleBlockingFetch(
    ModSpdyFetcher* fetcher, const GoogleString& url,
    MessageHandler* message_handler, AsyncFetch* fetch) {
  rate_controller_.Fetch(new FetchDispatcher(fetcher, &thread_pool_),
                         url, message_handler, fetch);
}

}  // namespace net_instaweb
