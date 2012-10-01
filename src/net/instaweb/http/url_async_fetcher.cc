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

#include "net/instaweb/http/public/url_async_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

const int64 UrlAsyncFetcher::kUnspecifiedTimeout = 0;

// Statistics group names.
const char UrlAsyncFetcher::kStatisticsGroup[] = "Statistics";

UrlAsyncFetcher::~UrlAsyncFetcher() {
}

UrlAsyncFetcher::Callback::~Callback() {
}

bool UrlAsyncFetcher::Callback::EnableThreaded() const {
  // Most fetcher callbacks are not prepared to be called from a different
  // thread.
  return false;
}

namespace {

class WriterCallbackFetch : public AsyncFetchUsingWriter {
 public:
  WriterCallbackFetch(Writer* writer, const RequestHeaders& headers,
                      UrlAsyncFetcher::Callback* callback)
      : AsyncFetchUsingWriter(writer),
        callback_(callback) {
    request_headers()->CopyFrom(headers);
  }
  virtual ~WriterCallbackFetch() {}

  virtual bool EnableThreaded() const { return callback_->EnableThreaded(); }

 protected:
  virtual void HandleHeadersComplete() {}
  virtual void HandleDone(bool success) {
    callback_->Done(success);
    delete this;
  }

 private:
  UrlAsyncFetcher::Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(WriterCallbackFetch);
};

}  // namespace

// Default implementation of StreamingFetch uses Fetch.
bool UrlAsyncFetcher::StreamingFetch(const GoogleString& url,
                                     const RequestHeaders& request_headers,
                                     ResponseHeaders* response_headers,
                                     Writer* response_writer,
                                     MessageHandler* message_handler,
                                     Callback* callback) {
  WriterCallbackFetch* fetch =
      new WriterCallbackFetch(response_writer, request_headers, callback);
  fetch->request_headers()->CopyFrom(request_headers);
  fetch->set_response_headers(response_headers);
  return Fetch(url, message_handler, fetch);
}

void UrlAsyncFetcher::ShutDown() {
}

AsyncFetch* UrlAsyncFetcher::EnableInflation(AsyncFetch* fetch) const {
  InflatingFetch* inflating_fetch = new InflatingFetch(fetch);
  if (fetch_with_gzip_) {
    inflating_fetch->EnableGzipFromBackend();
  }
  return inflating_fetch;
}

}  // namespace instaweb
