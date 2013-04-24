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

#include "ngx_request_context.h"

extern "C" {
  #include <ngx_http.h>
}

#include "ngx_pagespeed.h"

#include "base/logging.h"
#include "net/instaweb/http/public/meta_data.h"

namespace net_instaweb {

NgxRequestContext::NgxRequestContext(AbstractMutex* logging_mutex,
                                     Timer* timer,
                                     ngx_http_request_t* r)
    : RequestContext(logging_mutex, timer),
      local_port_(-1) {
  // Note that at the time we create a RequestContext we have full
  // access to the nginx internal request structure.  However,
  // due to Cloning and (I believe) Detaching, we can initiate fetches after
  // http_http_request_t* has been retired.  So deep-copy the bits we need
  // at the time we create our RequestContext.

  // Save our own IP as well, LoopbackRouteFetcher will need it.

  // Based on ngx_http_variable_server_port.
  bool port_set = false;
#if (NGX_HAVE_INET6)
  if (r->connection->local_sockaddr->sa_family == AF_INET6) {
    local_port_ = ntohs(reinterpret_cast<struct sockaddr_in6*>(
        r->connection->local_sockaddr)->sin6_port);
    port_set = true;
  }
#endif
  if (!port_set) {
    local_port_ = ntohs(reinterpret_cast<struct sockaddr_in*>(
        r->connection->local_sockaddr)->sin_port);
  }

  ngx_str_t  s;
  u_char addr[NGX_SOCKADDR_STRLEN];
  s.len = NGX_SOCKADDR_STRLEN;
  s.data = addr;
  ngx_int_t rc = ngx_connection_local_sockaddr(r->connection, &s, 0);
  if (rc != NGX_OK) {
    s.len = 0;
  }
  local_ip_ =  ngx_psol::str_to_string_piece(s).as_string();
}

NgxRequestContext::~NgxRequestContext() {
}

NgxRequestContext* NgxRequestContext::DynamicCast(RequestContext* rc) {
  if (rc == NULL) {
    return NULL;
  }
  NgxRequestContext* out = dynamic_cast<NgxRequestContext*>(rc);
  DCHECK(out != NULL) << "Invalid request conversion. Do not rely on RTTI for "
                      << "functional behavior. Ngx handling flows must use "
                      << "NgxRequestContexts.";
  return out;
}

}  // namespace net_instaweb
