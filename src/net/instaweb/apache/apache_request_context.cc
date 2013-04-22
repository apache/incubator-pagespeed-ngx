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

#include "net/instaweb/apache/apache_request_context.h"

#include "base/logging.h"
#include "net/instaweb/apache/interface_mod_spdy.h"
#include "net/instaweb/apache/mod_spdy_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"

#include "httpd.h"  // NOLINT

namespace net_instaweb {

ApacheRequestContext::ApacheRequestContext(
    AbstractMutex* logging_mutex, Timer* timer, request_rec* req)
    : RequestContext(logging_mutex, timer),
      use_spdy_fetcher_(ModSpdyFetcher::ShouldUseOn(req)),
      local_port_(req->connection->local_addr->port),
      spdy_connection_factory_(NULL) {
  // Note that at the time we create a RequestContext we have full
  // access to the Apache request_rec.  However, due to Cloning and (I
  // believe) Detaching, we can initiate fetches after the Apache
  // request_rec* has been retired.  So deep-copy the bits we need
  // from the request_rec at the time we create our RequestContext.
  // This includes the local port (for loopback fetches) and the
  // entire connection subobject, for backdoor mod_spdy fetches.  To
  // avoid temptation we do not keep a pointer to the request_rec.

  // Determines whether we should handle request as SPDY.
  // This happens in two cases:
  // 1) It's actually a SPDY request using mod_spdy
  // 2) The header X-PSA-Optimize-For-SPDY is present, with any value.
  if (mod_spdy_get_spdy_version(req->connection) != 0) {
    set_using_spdy(true);
  } else {
    const char* value = apr_table_get(req->headers_in,
                                      HttpAttributes::kXPsaOptimizeForSpdy);
    set_using_spdy(value != NULL);
  }

  // Save our own IP as well, LoopbackRouteFetcher will need it.
  if (req->connection->local_ip != NULL) {
    local_ip_ = req->connection->local_ip;
  }

  // Independent of whether we are serving a SPDY request, we will want
  // to be able to do back door mod_spdy fetches if configured to do so.
  if (use_spdy_fetcher_) {
    // TODO(jmarantz): mdsteele indicates this is not overly expensive to
    // do per-request.  Verify this with profiling.
    spdy_connection_factory_ =
        mod_spdy_create_slave_connection_factory(req->connection);
  }
}

ApacheRequestContext::~ApacheRequestContext() {
  if (spdy_connection_factory_ != NULL) {
    mod_spdy_destroy_slave_connection_factory(spdy_connection_factory_);
  }
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
