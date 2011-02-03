//  Copyright 2010 Google Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

// Author: morlovich@google.com (Maksim Orlovich)
//
// UrlPollableAsyncFetchers allow a client to block on asynchronous resource
// fetches.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_URL_POLLABLE_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_URL_POLLABLE_ASYNC_FETCHER_H_

#include "base/basictypes.h"
#include "net/instaweb/http/public/url_async_fetcher.h"

namespace net_instaweb {

class UrlPollableAsyncFetcher : public UrlAsyncFetcher {
 public:
  virtual ~UrlPollableAsyncFetcher();

  // Poll the active fetches, returning the number of fetches
  // still outstanding.
  virtual int Poll(int64 max_wait_ms) = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_URL_POLLABLE_ASYNC_FETCHER_H_
