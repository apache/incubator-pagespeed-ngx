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

#include "net/instaweb/http/public/counting_url_async_fetcher.h"

#include "base/basictypes.h"
#include "net/instaweb/util/public/counting_writer.h"

namespace net_instaweb {

class CountingUrlAsyncFetcher::Fetch : public UrlAsyncFetcher::Callback {
 public:
  Fetch(UrlAsyncFetcher::Callback* callback, CountingUrlAsyncFetcher* counter,
        Writer* writer)
      : callback_(callback),
        counter_(counter),
        byte_counter_(writer) {
  }

  virtual void Done(bool success) {
    // TODO(jmarantz): consider whether a Mutex is needed and how to supply one.
    ++counter_->fetch_count_;
    if (success) {
      counter_->byte_count_ += byte_counter_.byte_count();
    } else {
      ++counter_->failure_count_;
    }
    callback_->Done(success);
    delete this;
  }

  Writer* writer() { return &byte_counter_; }

 private:
  UrlAsyncFetcher::Callback* callback_;
  CountingUrlAsyncFetcher* counter_;
  CountingWriter byte_counter_;

  DISALLOW_COPY_AND_ASSIGN(Fetch);
};

CountingUrlAsyncFetcher::~CountingUrlAsyncFetcher() {
}

bool CountingUrlAsyncFetcher::StreamingFetch(
    const std::string& url, const RequestHeaders& request_headers,
    ResponseHeaders* response_headers, Writer* fetched_content_writer,
    MessageHandler* message_handler, Callback* callback) {
  Fetch* fetch = new Fetch(callback, this, fetched_content_writer);
  return fetcher_->StreamingFetch(url, request_headers, response_headers,
                                  fetch->writer(), message_handler, fetch);
}

void CountingUrlAsyncFetcher::Clear() {
  fetch_count_ = 0;
  byte_count_ = 0;
  failure_count_ = 0;
}

}  // namespace instaweb
