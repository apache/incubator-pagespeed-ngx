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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/time_util.h"

namespace net_instaweb {

class Writer;

const int64 UrlAsyncFetcher::kUnspecifiedTimeout = 0;

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

// Callback for default implementation of ConditionalFetch.
// Gets called back with status from StreamingFetch which does not have
// modified bit set. Sets modified bit appropriately based upon HTTP
// response status code.
class ConditionalFetchCallback : public UrlAsyncFetcher::Callback {
 public:
  ConditionalFetchCallback(UrlAsyncFetcher::Callback* base_callback,
                           const ResponseHeaders* response_headers)
      : base_callback_(base_callback),
        response_headers_(response_headers) {}
  virtual ~ConditionalFetchCallback() {}

  virtual void Done(bool success) {
    base_callback_->set_modified(
        response_headers_->status_code() != HttpStatus::kNotModified);
    base_callback_->Done(success);
    delete this;
  }

  // TODO(sligocki): See if we can set EnableThreaded() = True

 private:
  UrlAsyncFetcher::Callback* base_callback_;
  const ResponseHeaders* response_headers_;

  DISALLOW_COPY_AND_ASSIGN(ConditionalFetchCallback);
};

}  // namespace

bool UrlAsyncFetcher::ConditionalFetch(const GoogleString& url,
                                       int64 mod_time_ms,
                                       const RequestHeaders& request_headers,
                                       ResponseHeaders* response_headers,
                                       Writer* response_writer,
                                       MessageHandler* message_handler,
                                       Callback* base_callback) {
  // Default implementation just sets the If-Modified-Since GET header.
  RequestHeaders conditional_headers;
  conditional_headers.CopyFrom(request_headers);
  GoogleString mod_time_string;
  if (ConvertTimeToString(mod_time_ms, &mod_time_string)) {
    conditional_headers.Add(HttpAttributes::kIfModifiedSince, mod_time_string);
  } else {
    message_handler->Message(kError, "Invalid time value %s",
                             Integer64ToString(mod_time_ms).c_str());
  }
  Callback* sub_callback =
      new ConditionalFetchCallback(base_callback, response_headers);
  return StreamingFetch(url, conditional_headers,
                        response_headers, response_writer,
                        message_handler, sub_callback);
}

}  // namespace instaweb
