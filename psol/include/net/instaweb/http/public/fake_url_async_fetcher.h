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
// UrlFetcher is an interface for asynchronously fetching urls.  The
// caller must supply a callback to be called when the fetch is complete.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_FAKE_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_FAKE_URL_ASYNC_FETCHER_H_

#include "net/instaweb/http/public/url_pollable_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class UrlFetcher;

// Constructs an async fetcher using a synchronous fetcher, blocking
// on a fetch and then the 'done' callback directly.  It's also
// possible to construct a real async interface using a synchronous
// fetcher in a thread, but this does not do that: it blocks.
//
// This is intended for functional regression tests only.
class FakeUrlAsyncFetcher : public UrlPollableAsyncFetcher {
 public:
  explicit FakeUrlAsyncFetcher(UrlFetcher* url_fetcher)
      : url_fetcher_(url_fetcher),
        fetcher_supports_https_(true) {
  }
  virtual ~FakeUrlAsyncFetcher();

  virtual bool SupportsHttps() const { return fetcher_supports_https_; }

  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  // Since the underlying fetcher is blocking, there can never be
  // any outstanding fetches.
  virtual int Poll(int64 max_wait_ms) { return 0; }

  void set_fetcher_supports_https(bool val) { fetcher_supports_https_ = val; }

 private:
  UrlFetcher* url_fetcher_;
  bool fetcher_supports_https_;

  DISALLOW_COPY_AND_ASSIGN(FakeUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_FAKE_URL_ASYNC_FETCHER_H_
