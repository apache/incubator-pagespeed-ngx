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

#include "net/instaweb/apache/serf_url_fetcher.h"

#include <algorithm>
#include "base/basictypes.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/html_rewriter_config.h"
#include "net/instaweb/apache/pagespeed_server_context.h"

namespace {

class AsyncCallback : public net_instaweb::UrlAsyncFetcher::Callback {
 public:
  explicit AsyncCallback(net_instaweb::MessageHandler* message_handler)
      : done_(false),
        success_(false),
        message_handler_(message_handler) {}
  virtual ~AsyncCallback() {}
  virtual void Done(bool success)  {
    done_ = true;
    success_ = success;
  }
  bool done() const { return done_; }
  bool success() const { return success_; }
 private:
  bool done_;
  bool success_;
  net_instaweb::MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(AsyncCallback);
};

}  // namespace

namespace html_rewriter {

SerfUrlFetcher::SerfUrlFetcher(PageSpeedServerContext* context,
                               SerfUrlAsyncFetcher* async_fetcher)
  : context_(context),
    async_fetcher_(async_fetcher) {
}

SerfUrlFetcher::~SerfUrlFetcher() {
}

bool SerfUrlFetcher::StreamingFetchUrl(const std::string& url,
                                       const MetaData& request_headers,
                                       MetaData* response_headers,
                                       Writer* fetched_content_writer,
                                       MessageHandler* message_handler) {
  AsyncCallback callback(message_handler);
  async_fetcher_->StreamingFetch(
      url, request_headers, response_headers,
      fetched_content_writer, message_handler, &callback);

  AprTimer timer;
  int64 max_ms = GetFetcherTimeOut(context_);  // milliseconds.
  for (int64 start_ms = timer.NowMs(), now_ms = start_ms;
       !callback.done() && now_ms - start_ms < max_ms;
       now_ms = timer.NowMs()) {
    int64 remaining_us = std::max(static_cast<int64>(0),
                                  1000 * (max_ms - now_ms));
    async_fetcher_->Poll(remaining_us);
  }
  if (!callback.done()) {
    message_handler->Error(url.c_str(), 0, "Timeout waiting response for %d ms",
                           static_cast<int>(max_ms));
  }
  return callback.success();
}

}  // namespace html_rewriter
