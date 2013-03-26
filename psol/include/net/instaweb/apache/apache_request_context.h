/*
 * Copyright 2013 Google Inc.
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
// Captures the Apache request details in our request context, including
// the port (used for loopback fetches) and (if enabled & serving spdy)
// a factory for generating SPDY fetches.

#ifndef NET_INSTAWEB_APACHE_APACHE_REQUEST_CONTEXT_H_
#define NET_INSTAWEB_APACHE_APACHE_REQUEST_CONTEXT_H_

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

struct request_rec;
struct spdy_slave_connection_factory;

namespace net_instaweb {

class AbstractMutex;

class ApacheRequestContext : public RequestContext {
 public:
  ApacheRequestContext(AbstractMutex* logging_mutex, request_rec* req);

  // Captures the original URL of the request, which is used to help
  // authorize domains for fetches we do on behalf of that request.
  void set_url(StringPiece url) { url.CopyToString(&url_); }

  // Returns rc as an ApacheRequestContext* if it is one and CHECK
  // fails if it is not. Returns NULL if rc is NULL.
  static ApacheRequestContext* DynamicCast(RequestContext* rc);

  bool use_spdy_fetcher() const { return use_spdy_fetcher_; }
  int local_port() const { return local_port_; }
  StringPiece url() const { return url_; }
  spdy_slave_connection_factory* spdy_connection_factory() {
    return spdy_connection_factory_;
  }

 protected:
  virtual ~ApacheRequestContext();

 private:
  bool use_spdy_fetcher_;
  int local_port_;
  GoogleString url_;
  spdy_slave_connection_factory* spdy_connection_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApacheRequestContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_REQUEST_CONTEXT_H_
