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
// ModSpdyFetchController coordinates a threadpool and a rate controller between
// multiple ModSpdyFetcher objects. The basic usage pattern is that
// ModSpdyFetcher::Fetch calls ModSpdyFetchController::ScheduleBlockingFetch,
// which will then cause ModSpdyFetcher::BlockingFetch to be called on a
// thread in a hopefully intelligent manner.

#ifndef NET_INSTAWEB_APACHE_MOD_SPDY_FETCH_CONTROLLER_H_
#define NET_INSTAWEB_APACHE_MOD_SPDY_FETCH_CONTROLLER_H_

#include "net/instaweb/http/public/rate_controller.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class ModSpdyFetcher;
class Statistics;
class ThreadSystem;

class ModSpdyFetchController {
 public:
  // Note: RateController::InitStats must have been called before using this.
  ModSpdyFetchController(int num_threads,
                         ThreadSystem* thread_system,
                         Statistics* statistics);
  ~ModSpdyFetchController();

  // Arranges for fetcher->BlockingFetch to be called on our thread pool.
  void ScheduleBlockingFetch(
      ModSpdyFetcher* fetcher, const GoogleString& url,
      MessageHandler* message_handler, AsyncFetch* fetch);

  // TODO(morlovich): Add a ShutDown(), with semantics matching those
  // of UrlAsyncFetcher::ShutDown, and invoked similarly.

 private:
  class FetchDispatcher;

  RateController rate_controller_;
  QueuedWorkerPool thread_pool_;
  DISALLOW_COPY_AND_ASSIGN(ModSpdyFetchController);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_MOD_SPDY_FETCH_CONTROLLER_H_
