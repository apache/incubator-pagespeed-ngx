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
// NOTE: This interface is actively under development and may be
// changed extensively. Contact us at mod-pagespeed-discuss@googlegroups.com
// if you are interested in using it.
//
// Simple interface for running Page Speed Automatic as a proxy.
//
// When implementing a Page Speed Automatic proxy, simply construct a
// ProxyInterface at start up time and call StreamingFetch for every
// requested resource. StreamingFetch decides how to deal with requests
// (pagespeed resources will be computed, HTML pages will be proxied
// and rewritten, and other resources will just be proxied).
//
// Page Speed Automatic currently does not deal with some requests
// such as POSTs.

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_PROXY_INTERFACE_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_PROXY_INTERFACE_H_

#include <utility>

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AsyncFetch;
class GoogleUrl;
class Layout;
class MessageHandler;
class ProxyFetchFactory;
class RequestHeaders;
class ResourceManager;
class RewriteOptions;
class Statistics;
class TimedVariable;
class Timer;

// TODO(sligocki): Rename as per style-guide.
class ProxyInterface : public UrlAsyncFetcher {
 public:
  typedef std::pair<RewriteOptions*, bool> OptionsBoolPair;

  ProxyInterface(const StringPiece& hostname, int port,
                 ResourceManager* manager, Statistics* stats);
  virtual ~ProxyInterface();

  // Initializes statistics variables associated with this class.
  static void Initialize(Statistics* statistics);

  // All requests use this interface. We decide internally whether the
  // request is a pagespeed resource, HTML page to be rewritten or another
  // resource to be proxied directly.
  virtual bool Fetch(const GoogleString& requested_url,
                     MessageHandler* handler,
                     AsyncFetch* async_fetch);

  // Returns any custom options required for this request, incorporating
  // any domain-specific options from the UrlNamer, options set in query-params,
  // and options set in request headers.  Possible return-value scenarios for
  // the pair are:
  //
  // first==*, .second==false:  query-params or req-headers failed in parse.
  // .first!=NULL, .second=true:  Custom options created, now owned by caller.
  // .first==NULL, .second=true:  Use the global options for resource_manager.
  OptionsBoolPair GetCustomOptions(const GoogleUrl& request_url,
                                   const RequestHeaders& request_headers,
                                   RewriteOptions* domain_options,
                                   MessageHandler* handler);

  void set_server_version(const StringPiece& server_version);

  // Callback function passed to UrlNamer to finish handling requests once we
  // have rewrite_options for requests that are being proxied.
  void ProxyRequestCallback(bool is_resource_fetch,
                            GoogleUrl* request_url,
                            AsyncFetch* async_fetch,
                            RewriteOptions* domain_options,
                            MessageHandler* handler);

 private:
  // Handle requests that are being proxied.
  // * HTML requests are rewritten.
  // * Resource requests are proxied verbatim.
  void ProxyRequest(bool is_resource_fetch,
                    const GoogleUrl& requested_url,
                    AsyncFetch* async_fetch,
                    MessageHandler* handler);

  // Is this url_string well-formed enough to proxy through?
  bool IsWellFormedUrl(const GoogleUrl& url);

  // If the URL and port are for this server, don't proxy those (to avoid
  // infinite fetching loops). This might be the favicon or something...
  bool UrlAndPortMatchThisServer(const GoogleUrl& url);

  // Checks whether the request is a valid blink request. Returns a pointer
  // to the Layout if it is a blink request and NULL otherwise.
  const Layout* ExtractBlinkLayout(const GoogleUrl& url,
                                   AsyncFetch* async_fetch,
                                   RewriteOptions* options);

  // References to unowned objects.
  ResourceManager* resource_manager_;     // thread-safe
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

  scoped_ptr<ProxyFetchFactory> proxy_fetch_factory_;
  UserAgentMatcher user_agent_matcher_;

  DISALLOW_COPY_AND_ASSIGN(ProxyInterface);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_PROXY_INTERFACE_H_
