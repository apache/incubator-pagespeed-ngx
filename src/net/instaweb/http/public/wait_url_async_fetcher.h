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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_WAIT_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_WAIT_URL_ASYNC_FETCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "net/instaweb/http/public/url_async_fetcher.h"

namespace net_instaweb {

class UrlFetcher;

// Fake UrlAsyncFetcher which waits to call underlying blocking fetcher until
// you explicitly call CallCallbacks().
class WaitUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  explicit WaitUrlAsyncFetcher(UrlFetcher* url_fetcher)
      : url_fetcher_(url_fetcher) {}
  virtual ~WaitUrlAsyncFetcher();

  // Initiate fetches that will finish when CallCallbacks is called.
  virtual bool StreamingFetch(const std::string& url,
                              const RequestHeaders& request_headers,
                              ResponseHeaders* response_headers,
                              Writer* response_writer,
                              MessageHandler* message_handler,
                              Callback* callback);

  // Call all callbacks from previously initiated fetches.
  void CallCallbacks();

 private:
  class DelayedFetch;

  UrlFetcher* url_fetcher_;
  std::vector<DelayedFetch*> delayed_fetches_;

  DISALLOW_COPY_AND_ASSIGN(WaitUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_WAIT_URL_ASYNC_FETCHER_H_
