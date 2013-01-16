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

#include "net/instaweb/apache/interface_mod_spdy.h"
#include "net/instaweb/http/public/meta_data.h"

#include "httpd.h"  // NOLINT

namespace {

const char* kClassName = "ApacheRequestContext";

}  // namespace

namespace net_instaweb {

ApacheRequestContext::ApacheRequestContext(AbstractMutex* logging_mutex,
                                           request_rec* req)
    : RequestContext(logging_mutex),
      apache_request_(req) {
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
}

ApacheRequestContext::~ApacheRequestContext() {
}

const char* ApacheRequestContext::class_name() const { return kClassName; }

ApacheRequestContext* ApacheRequestContext::DynamicCast(RequestContext* rc) {
  if ((rc != NULL) && (rc->class_name() == kClassName)) {
    return static_cast<ApacheRequestContext*>(rc);
  }
  return NULL;
}

}  // namespace net_instaweb
