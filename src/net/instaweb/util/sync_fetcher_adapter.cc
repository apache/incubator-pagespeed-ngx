// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/instaweb/util/public/sync_fetcher_adapter.h"

#include <algorithm>
#include "base/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/sync_fetcher_adapter_callback.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

SyncFetcherAdapter::SyncFetcherAdapter(Timer* timer,
                                       int64 fetcher_timeout_ms,
                                       UrlPollableAsyncFetcher* async_fetcher)
    : timer_(timer),
      fetcher_timeout_ms_(fetcher_timeout_ms),
      async_fetcher_(async_fetcher) {
}

SyncFetcherAdapter::~SyncFetcherAdapter() {
}

bool SyncFetcherAdapter::StreamingFetchUrl(
    const std::string& url, const RequestHeaders& request_headers,
    ResponseHeaders* response_headers, Writer* fetched_content_writer,
    MessageHandler* message_handler) {
  SyncFetcherAdapterCallback* callback = new SyncFetcherAdapterCallback(
      response_headers, fetched_content_writer);
  async_fetcher_->StreamingFetch(
      url, request_headers, callback->response_headers(),
      callback->writer(), message_handler, callback);

  // We are counting on the async fetcher having a timeout (if any)
  // that's similar to the timeout that we have in this class.
  // To avoid a race we double the timeout in the limit set here and
  // CHECK that the callback got called by the time our timeout loop exits.
  int64 start_ms = timer_->NowMs();
  int64 now_ms = start_ms;
  for (int64 end_ms = now_ms + 2 * fetcher_timeout_ms_;
       !callback->done() && (now_ms < end_ms);
       now_ms = timer_->NowMs()) {
    int64 remaining_us = std::max(static_cast<int64>(0),
                                  1000 * (end_ms - now_ms));
    async_fetcher_->Poll(remaining_us);
  }
  bool ret = false;
  if (!callback->done()) {
    message_handler->Message(
        kWarning,
        "Async fetcher allowed %dms to expire without calling its callback",
        static_cast<int>(now_ms - start_ms));
  } else {
    ret = callback->success();
  }
  callback->Release();
  return ret;
}

}  // namespace net_instaweb
