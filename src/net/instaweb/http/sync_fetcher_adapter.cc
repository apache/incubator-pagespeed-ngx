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

#include "net/instaweb/http/public/sync_fetcher_adapter.h"

#include <algorithm>
#include "base/logging.h"
#include "net/instaweb/http/public/sync_fetcher_adapter_callback.h"
#include "net/instaweb/http/public/url_pollable_async_fetcher.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class RequestHeaders;
class ResponseHeaders;
class Writer;

SyncFetcherAdapter::SyncFetcherAdapter(Timer* timer,
                                       int64 fetcher_timeout_ms,
                                       UrlPollableAsyncFetcher* async_fetcher,
                                       ThreadSystem* thread_system)
    : timer_(timer),
      fetcher_timeout_ms_(fetcher_timeout_ms),
      async_fetcher_(async_fetcher),
      thread_system_(thread_system) {
}

SyncFetcherAdapter::~SyncFetcherAdapter() {
}

bool SyncFetcherAdapter::StreamingFetchUrl(
    const GoogleString& url, const RequestHeaders& request_headers,
    ResponseHeaders* response_headers, Writer* fetched_content_writer,
    MessageHandler* message_handler) {
  SyncFetcherAdapterCallback* callback = new SyncFetcherAdapterCallback(
      thread_system_, fetched_content_writer);
  callback->set_response_headers(response_headers);
  async_fetcher_->Fetch(url, message_handler, callback);

  // We are counting on the async fetcher having a timeout (if any)
  // that's similar to the timeout that we have in this class.
  // To avoid a race we double the timeout in the limit set here and
  // CHECK that the callback got called by the time our timeout loop exits.
  int64 start_ms = timer_->NowMs();
  int64 now_ms = start_ms;
  for (int64 end_ms = now_ms + 2 * fetcher_timeout_ms_;
       !callback->done() && (now_ms < end_ms);
       now_ms = timer_->NowMs()) {
    int64 remaining_ms =
        std::max(static_cast<int64>(0), end_ms - now_ms);
    int active = async_fetcher_->Poll(remaining_ms);
    CHECK(active > 0 || callback->done());
  }
  bool ret = false;
  if (!callback->done()) {
    message_handler->Message(
        kWarning,
        "Async fetch of %s allowed %dms to expire without calling its callback",
        url.c_str(), static_cast<int>(now_ms - start_ms));
  } else {
    ret = callback->success();
  }
  callback->Release();
  return ret;
}

}  // namespace net_instaweb
