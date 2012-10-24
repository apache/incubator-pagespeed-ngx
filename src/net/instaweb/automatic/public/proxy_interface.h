/*
 * Copyright 2011 Google Inc.
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
//
// Simple interface for running Page Speed Automatic as a proxy.
//
// When implementing a Page Speed Automatic proxy, simply construct a
// ProxyInterface at start up time and call Fetch for every
// requested resource. Fetch decides how to deal with requests
// (pagespeed resources will be computed, HTML pages will be proxied
// and rewritten, and other resources will just be proxied).

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_PROXY_INTERFACE_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_PROXY_INTERFACE_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AsyncFetch;
class GoogleUrl;
class MessageHandler;
class ProxyFetchPropertyCallbackCollector;
class ProxyFetchFactory;
class ServerContext;
class RewriteOptions;
class Statistics;
class TimedVariable;
class Timer;

// TODO(sligocki): Rename as per style-guide.
class ProxyInterface : public UrlAsyncFetcher {
 public:
  ProxyInterface(const StringPiece& hostname, int port,
                 ServerContext* manager, Statistics* stats);
  virtual ~ProxyInterface();

  // Initializes statistics variables associated with this class.
  static void InitStats(Statistics* statistics);

  // All requests use this interface. We decide internally whether the
  // request is a pagespeed resource, HTML page to be rewritten or another
  // resource to be proxied directly.
  virtual void Fetch(const GoogleString& requested_url,
                     MessageHandler* handler,
                     AsyncFetch* async_fetch);

  // Callback function passed to UrlNamer to finish handling requests once we
  // have rewrite_options for requests that are being proxied.
  void ProxyRequestCallback(
      bool is_resource_fetch,
      GoogleUrl* request_url,
      AsyncFetch* async_fetch,
      RewriteOptions* domain_options,
      RewriteOptions* query_options,
      MessageHandler* handler);

  // Is this url_string well-formed enough to proxy through?
  bool IsWellFormedUrl(const GoogleUrl& url);

  static const char kBlinkRequestCount[];
  static const char kBlinkCriticalLineRequestCount[];

  // Initiates the PropertyCache look up.
  virtual ProxyFetchPropertyCallbackCollector* InitiatePropertyCacheLookup(
      bool is_resource_fetch,
      const GoogleUrl& request_url,
      RewriteOptions* options,
      AsyncFetch* async_fetch);

 private:
  friend class ProxyInterfaceTest;

  // Handle requests that are being proxied.
  // * HTML requests are rewritten.
  // * Resource requests are proxied verbatim.
  void ProxyRequest(bool is_resource_fetch,
                    const GoogleUrl& requested_url,
                    AsyncFetch* async_fetch,
                    MessageHandler* handler);

  // If the URL and port are for this server, don't proxy those (to avoid
  // infinite fetching loops). This might be the favicon or something...
  bool UrlAndPortMatchThisServer(const GoogleUrl& url);

  // References to unowned objects.
  ServerContext* server_context_;     // thread-safe
  UrlAsyncFetcher* fetcher_;              // thread-safe
  Timer* timer_;                          // thread-safe
  MessageHandler* handler_;               // thread-safe

  // This server's hostname and port (to avoid making circular requests).
  // TODO(sligocki): This assumes we will only be called as one hostname,
  // there could be multiple DNS entries pointing at us.
  const GoogleString hostname_;
  const int port_;

  // Varz variables
  // Total requests.
  TimedVariable* all_requests_;
  // Total Pagespeed requests.
  TimedVariable* pagespeed_requests_;
  // Blink requests.
  TimedVariable* blink_requests_;
  // Blink requests in the critical line flow.
  TimedVariable* blink_critical_line_requests_;
  // Rejected requests counter.
  TimedVariable* rejected_requests_;

  scoped_ptr<ProxyFetchFactory> proxy_fetch_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyInterface);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_PROXY_INTERFACE_H_
