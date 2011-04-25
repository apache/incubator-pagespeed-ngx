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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/http/public/wait_url_async_fetcher.h"

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/http/public/url_fetcher.h"

namespace net_instaweb {

class WaitUrlAsyncFetcher::DelayedFetch {
 public:
  DelayedFetch(UrlFetcher* base_fetcher,
               const GoogleString& url, const RequestHeaders& request_headers,
               ResponseHeaders* response_headers, Writer* response_writer,
               MessageHandler* handler, Callback* callback)
      : base_fetcher_(base_fetcher), url_(url),
        response_headers_(response_headers), response_writer_(response_writer),
        handler_(handler), callback_(callback) {
    request_headers_.CopyFrom(request_headers);
  }

  void FetchNow() {
    bool status = base_fetcher_->StreamingFetchUrl(
      url_, request_headers_, response_headers_, response_writer_, handler_);
    callback_->Done(status);
  }

 private:
  UrlFetcher* base_fetcher_;
  GoogleString url_;
  RequestHeaders request_headers_;
  ResponseHeaders* response_headers_;
  Writer* response_writer_;
  MessageHandler* handler_;
  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(DelayedFetch);
};

WaitUrlAsyncFetcher::~WaitUrlAsyncFetcher() {}

bool WaitUrlAsyncFetcher::StreamingFetch(const GoogleString& url,
                                         const RequestHeaders& request_headers,
                                         ResponseHeaders* response_headers,
                                         Writer* response_writer,
                                         MessageHandler* handler,
                                         Callback* callback) {
  // Don't call the blocking fetcher until CallCallbacks.
  delayed_fetches_.push_back(new DelayedFetch(
      url_fetcher_, url, request_headers, response_headers, response_writer,
      handler, callback));
  return false;
}

void WaitUrlAsyncFetcher::CallCallbacks() {
  for (int i = 0, n = delayed_fetches_.size(); i < n; ++i) {
    delayed_fetches_[i]->FetchNow();
  }
  STLDeleteElements(&delayed_fetches_);
  delayed_fetches_.clear();
}

}  // namespace net_instaweb
