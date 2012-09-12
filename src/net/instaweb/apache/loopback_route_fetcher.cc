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

#include "net/instaweb/apache/loopback_route_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

#include "apr_network_io.h"

namespace net_instaweb {

class MessageHandler;

LoopbackRouteFetcher::LoopbackRouteFetcher(
    const RewriteOptions* options,
    int own_port,
    UrlAsyncFetcher* backend_fetcher)
    : options_(options),
      own_port_(own_port),
      backend_fetcher_(backend_fetcher) {
}

LoopbackRouteFetcher::~LoopbackRouteFetcher() {
}

bool LoopbackRouteFetcher::Fetch(const GoogleString& original_url,
                                 MessageHandler* message_handler,
                                 AsyncFetch* fetch) {
  GoogleString url = original_url;
  GoogleUrl parsed_url(original_url);

  if (!parsed_url.is_valid()) {
    // Fail immediately in case we can't parse the URL, rather than risk
    // getting weird handling due to inconsistencies in parsing between us
    // and backend_fetcher_.
    LOG(WARNING) << "Can't parse URL:" << original_url;
    fetch->Done(false);
    return true;
  }

  RequestHeaders* request_headers = fetch->request_headers();

  // Check to see if the URL we hand to the backend has an origin we were never
  // explicitly told of, and if so just talk to loopback.
  // Note that in case of an origin mapping the parsed_url will contain the
  // fetch host, not the original host, so the domain_lawyer will know about it
  // and the if body will not run.
  if (!options_->domain_lawyer()->IsOriginKnown(parsed_url)) {
    // If there is no host header, make sure to add one, since we are about
    // to munge the URL.
    if (request_headers->Lookup1(HttpAttributes::kHost) == NULL) {
      request_headers->Replace(HttpAttributes::kHost, parsed_url.HostAndPort());
    }

    GoogleUrl base;
    if (own_port_ == 80) {
      base.Reset("http://127.0.0.1/");
    } else {
      base.Reset(StrCat("http://127.0.0.1:", IntegerToString(own_port_), "/"));
    }

    GoogleString rel;
    parsed_url.PathAndLeaf().CopyToString(&rel);

    parsed_url.Reset(base, rel);
    parsed_url.Spec().CopyToString(&url);

    // Note that we end up with host: containing the actual URL's host, but
    // the URL containing just 127.0.0.1. This is technically wrong, but the
    // Serf fetcher will interpret it in the way we want it to --- it will
    // connect to 127.0.0.1, pass only the path portion to the host, and
    // keep the host: header matching what's in the request_headers.
  }

  return backend_fetcher_->Fetch(url, message_handler, fetch);
}

bool LoopbackRouteFetcher::IsLoopbackAddr(const apr_sockaddr_t* addr) {
  if (addr->family == APR_INET) {
    // 127.0.0.0/8 is the IPv4 loopback.
    // Note: is network byte order, so we can do char-wide indexing into it
    // consistently (but not look at the whole thing).
    const char* ipbytes = reinterpret_cast<const char*>(
        &addr->sa.sin.sin_addr.s_addr);
    return (ipbytes[0] == 127);
  } else if (addr->family == APR_INET6) {
    const in6_addr& addr_v6 = addr->sa.sin6.sin6_addr;

    // There are a couple of ways we can see loopbacks in IPv6: as the
    // proper IPv6 loopback, ::1, or as "IPv4-compatible IPv6 address"
    // of the IPv4 loopback, ::FFFF:127.x.y.z.

    // Regardless, the first 10 bytes ought to be 0.
    for (int b = 0; b < 10; ++b) {
      if (addr_v6.s6_addr[b] != 0) {
        return false;
      }
    }

    // If first 10 are OK, check the last 6 bytes for the 2 options.
    return (addr_v6.s6_addr[10] == 0xFF &&
            addr_v6.s6_addr[11] == 0xFF &&
            addr_v6.s6_addr[12] == 127) ||
           (addr_v6.s6_addr[10] == 0 &&
            addr_v6.s6_addr[11] == 0 &&
            addr_v6.s6_addr[12] == 0 &&
            addr_v6.s6_addr[13] == 0 &&
            addr_v6.s6_addr[14] == 0 &&
            addr_v6.s6_addr[15] == 1);
  } else {
    return false;
  }
}

}  // namespace net_instaweb
