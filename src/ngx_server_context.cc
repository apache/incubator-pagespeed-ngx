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

// Author: jefftk@google.com (Jeff Kaufman)

#include "ngx_server_context.h"

extern "C" {
  #include <ngx_http.h>
}

#include "ngx_pagespeed.h"
#include "ngx_message_handler.h"
#include "ngx_rewrite_driver_factory.h"
#include "ngx_rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/system/public/add_headers_fetcher.h"
#include "net/instaweb/system/public/loopback_route_fetcher.h"
#include "net/instaweb/system/public/system_request_context.h"

namespace net_instaweb {

NgxServerContext::NgxServerContext(
    NgxRewriteDriverFactory* factory, StringPiece hostname, int port)
    : SystemServerContext(factory, hostname, port) {
}

NgxServerContext::~NgxServerContext() { }

NgxRewriteOptions* NgxServerContext::config() {
  return NgxRewriteOptions::DynamicCast(global_options());
}

SystemRequestContext* NgxServerContext::NewRequestContext(
    ngx_http_request_t* r) {
  // Based on ngx_http_variable_server_port.
  bool port_set = false;
  int local_port;
#if (NGX_HAVE_INET6)
  if (r->connection->local_sockaddr->sa_family == AF_INET6) {
    local_port = ntohs(reinterpret_cast<struct sockaddr_in6*>(
        r->connection->local_sockaddr)->sin6_port);
    port_set = true;
  }
#endif
  if (!port_set) {
    local_port = ntohs(reinterpret_cast<struct sockaddr_in*>(
        r->connection->local_sockaddr)->sin_port);
  }

  ngx_str_t local_ip;
  u_char addr[NGX_SOCKADDR_STRLEN];
  local_ip.len = NGX_SOCKADDR_STRLEN;
  local_ip.data = addr;
  ngx_int_t rc = ngx_connection_local_sockaddr(r->connection, &local_ip, 0);
  if (rc != NGX_OK) {
    local_ip.len = 0;
  }

  return new SystemRequestContext(thread_system()->NewMutex(),
                                  timer(),
                                  local_port,
                                  str_to_string_piece(local_ip));
}

}  // namespace net_instaweb
