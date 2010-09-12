/**
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
// UrlFetcher is an interface for asynchronously fetching urls.  The
// caller must supply a callback to be called when the fetch is complete.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_FAKE_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_FAKE_URL_ASYNC_FETCHER_H_

#include "base/basictypes.h"
#include "net/instaweb/util/public/url_async_fetcher.h"
#include "net/instaweb/util/public/url_fetcher.h"

namespace net_instaweb {

// Constructs an async fetcher using a synchronous fetcher, blocking
// on a fetch and then the 'done' callback directly.  It's also
// possible to construct a real async interface using a synchronous
// fetcher in a thread, but this does not do that: it blocks.
//
// This is intended for functional regression tests only.
class FakeUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  explicit FakeUrlAsyncFetcher(UrlFetcher* url_fetcher)
      : url_fetcher_(url_fetcher) {
  }
  virtual ~FakeUrlAsyncFetcher();

  virtual bool StreamingFetch(const std::string& url,
                              const MetaData& request_headers,
                              MetaData* response_headers,
                              Writer* writer,
                              MessageHandler* handler,
                              Callback* callback) {
    bool ret = url_fetcher_->StreamingFetchUrl(
        url, request_headers, response_headers, writer, handler);
    callback->Done(ret);
    return true;
  }

 private:
  UrlFetcher* url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(FakeUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FAKE_URL_ASYNC_FETCHER_H_
