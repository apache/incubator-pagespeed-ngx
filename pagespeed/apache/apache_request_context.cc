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

#include "pagespeed/apache/apache_request_context.h"

#include "base/logging.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/apache/apache_httpd_includes.h"

namespace net_instaweb {

ApacheRequestContext::ApacheRequestContext(
    AbstractMutex* logging_mutex, Timer* timer, request_rec* req)
    : SystemRequestContext(
          logging_mutex,
          timer,
          req->hostname,
          req->connection->local_addr->port,
          req->connection->local_ip) {
  // Note that at the time we create a RequestContext we have full
  // access to the Apache request_rec.  However, due to Cloning and (I
  // believe) Detaching, we can initiate fetches after the Apache
  // request_rec* has been retired.  So deep-copy the bits we need
  // from the request_rec at the time we create our RequestContext.
  // This includes the local port (for loopback fetches) and whether H2 is on.
  if (req->proto_num == 2000) {
    set_using_http2(true);
  }

  const char* via_header = apr_table_get(req->headers_in, "Via");
  if (via_header != nullptr) {
    SetHttp2SupportFromViaHeader(via_header);
  }
}

ApacheRequestContext::~ApacheRequestContext() {
}

ApacheRequestContext* ApacheRequestContext::DynamicCast(RequestContext* rc) {
  if (rc == NULL) {
    return NULL;
  }
  ApacheRequestContext* out = dynamic_cast<ApacheRequestContext*>(rc);
  DCHECK(out != NULL) << "Invalid request conversion. Do not rely on RTTI for "
                      << "functional behavior. Apache handling flows must use "
                      << "ApacheRequestContexts.";
  return out;
}

}  // namespace net_instaweb
