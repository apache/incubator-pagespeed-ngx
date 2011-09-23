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

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class GoogleUrl;
class Histogram;
class ProxyFetch;
class ProxyFetchFactory;
class RequestHeaders;
class ResourceManager;
class ResponseHeaders;
class Statistics;
class TimedVariable;
class Timer;
class Writer;

// TODO(sligocki): Rename as per style-guide.
class ProxyInterface : public UrlAsyncFetcher {
 public:
  // TODO(sligocki): Simplify this by getting timer and handler from manager
  // or even just pass in the factory for everything?
  ProxyInterface(const StringPiece& hostname, int port,
                 ResourceManager* manager, UrlAsyncFetcher* fetcher,
                 Timer* timer, MessageHandler* handler, Statistics* stats);
  virtual ~ProxyInterface();

  // All requests use this interface. We decide internally whether the
  // request is a pagespeed resource, HTML page to be rewritten or another
  // resource to be proxied directly.
  virtual bool StreamingFetch(const GoogleString& requested_url,
                              const RequestHeaders& request_headers,
                              ResponseHeaders* response_headers,
                              Writer* response_writer,
                              MessageHandler* handler,
                              Callback* callback);

  // Accessors
  Histogram* fetch_latency_histogram() { return fetch_latency_histogram_; }
  Histogram* rewrite_latency_histogram() { return rewrite_latency_histogram_; }
  TimedVariable* total_fetch_count() { return total_fetch_count_; }
  TimedVariable* total_rewrite_count() { return total_rewrite_count_; }

 private:
  // Handle requests that are being proxied.
  // * HTML requests are rewritten.
  // * Resource requests are proxied verbatim.
  void ProxyRequest(const GoogleUrl& requested_url,
                    const RequestHeaders& request_headers,
                    ResponseHeaders* response_headers,
                    Writer* response_writer,
                    MessageHandler* handler,
                    Callback* callback);

  // Is this url_string well-formed enough to proxy through?
  bool IsWellFormedUrl(const GoogleUrl& url);

  // If the URL and port are for this server, don't proxy those (to avoid
  // infinite fetching loops). This might be the favicon or something...
  bool UrlAndPortMatchThisServer(const GoogleUrl& url);

  // References to unowned objects.
  ResourceManager* resource_manager_;     // thread-safe
  UrlAsyncFetcher* fetcher_;              // thread-safe
  Timer* timer_;                          // thread-safe
  MessageHandler* handler_;               // thread-safe

  // Statistics
  Histogram* fetch_latency_histogram_;    // thread-safe
  Histogram* rewrite_latency_histogram_;  // thread-safe
  TimedVariable* total_fetch_count_;      // thread-safe
  TimedVariable* total_rewrite_count_;    // thread-safe

  // This server's hostname and port (to avoid making circular requests).
  // TODO(sligocki): This assumes we will only be called as one hostname,
  // there could be multiple DNS entries pointing at us.
  const GoogleString hostname_;
  const int port_;

  scoped_ptr<ProxyFetchFactory> proxy_fetch_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyInterface);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_PROXY_INTERFACE_H_
