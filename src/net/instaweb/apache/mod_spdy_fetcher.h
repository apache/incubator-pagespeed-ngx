/*
 * Copyright 2012 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// A fetcher that talks to mod_spdy for requests matching a certain
// domain (and passes the rest to fallthrough fetcher).

#ifndef NET_INSTAWEB_APACHE_MOD_SPDY_FETCHER_H_
#define NET_INSTAWEB_APACHE_MOD_SPDY_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"

#include "apr_pools.h"
#include "httpd.h"


#include "net/instaweb/apache/interface_mod_spdy.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class RequestHeaders;
class ResponseHeaders;
class RewriteDriver;
class Writer;

class ModSpdyFetcher : public UrlAsyncFetcher {
 public:
  // Initializes various filters this fetcher needs for operation.
  // This must be from within a register hooks implementation.
  static void Initialize();

  ModSpdyFetcher(request_rec* req, RewriteDriver* driver);
  virtual ~ModSpdyFetcher();

  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  // Returns true if a ModSpdyFetcher should be installed as a session fetcher
  // on a given connection.
  static bool ShouldUseOn(request_rec* req);

  // TODO(morlovich): Implement virtual void ShutDown(),
  // and give a good story on session fetchers and fetcher shutdowns in general.

 private:
  spdy_slave_connection_factory* connection_factory_;
  UrlAsyncFetcher* fallback_fetcher_;
  GoogleString own_origin_;  // empty if we couldn't figure it out.

  DISALLOW_COPY_AND_ASSIGN(ModSpdyFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_MOD_SPDY_FETCHER_H_
