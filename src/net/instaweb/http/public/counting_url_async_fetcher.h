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
//
// Wraps an asynchronous fetcher, but keeps track of success/failure count.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_COUNTING_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_COUNTING_URL_ASYNC_FETCHER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class Writer;

class CountingUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  explicit CountingUrlAsyncFetcher(UrlAsyncFetcher* fetcher)
      : fetcher_(fetcher) {
    Clear();
  }
  virtual ~CountingUrlAsyncFetcher();

  virtual bool StreamingFetch(const GoogleString& url,
                              const RequestHeaders& request_headers,
                              ResponseHeaders* response_headers,
                              Writer* fetched_content_writer,
                              MessageHandler* message_handler,
                              Callback* callback);
  int fetch_count() const { return fetch_count_; }
  int byte_count() const { return byte_count_; }
  int failure_count() const { return failure_count_; }

  void Clear();

  class Fetch;
  friend class Fetch;

 private:
  UrlAsyncFetcher* fetcher_;
  int fetch_count_;
  int byte_count_;
  int failure_count_;

  DISALLOW_COPY_AND_ASSIGN(CountingUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_COUNTING_URL_ASYNC_FETCHER_H_
