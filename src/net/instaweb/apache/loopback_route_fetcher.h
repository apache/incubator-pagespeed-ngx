// Copyright 2012 Google Inc.
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
//
// Author: morlovich@google.com (Maksim Orlovich)
//
// This fetcher routes requests to hosts that are not explicitly mentioned in
// the DomainLawyer via the loopback.

#ifndef NET_INSTAWEB_APACHE_LOOPBACK_ROUTE_FETCHER_H_
#define NET_INSTAWEB_APACHE_LOOPBACK_ROUTE_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

struct apr_sockaddr_t;

namespace net_instaweb {

class AsyncFetch;
class RewriteOptions;
class MessageHandler;

// See file comment.
class LoopbackRouteFetcher : public UrlAsyncFetcher {
 public:
  // Does not take ownership of anything. own_port is the port the incoming
  // request came in on. If the backend_fetcher does actual fetching (and is
  // not merely simulating it for testing purposes) it should be the Serf
  // fetcher, as others may not direct requests this class produces properly.
  // (As this fetcher may produce requests that need to connect to 127.0.0.1
  //  but have a Host: and URL from somewhere else).
  LoopbackRouteFetcher(const RewriteOptions* options,
                       int own_port,
                       UrlAsyncFetcher* backend_fetcher);
  virtual ~LoopbackRouteFetcher();

  virtual bool SupportsHttps() const {
    return backend_fetcher_->SupportsHttps();
  }

  virtual bool Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  // Returns true if the given address is an IPv4 or IPv6 loopback.
  static bool IsLoopbackAddr(const apr_sockaddr_t* addr);

 private:
  const RewriteOptions* const options_;
  int own_port_;
  UrlAsyncFetcher* const backend_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(LoopbackRouteFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_LOOPBACK_ROUTE_FETCHER_H_
