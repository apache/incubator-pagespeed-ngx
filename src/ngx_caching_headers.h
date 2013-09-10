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

// Author: jefftk@google.com (Jeff Kaufman)

#ifndef NGX_CACHING_HEADERS_H_
#define NGX_CACHING_HEADERS_H_

extern "C" {
  #include <ngx_http.h>
}

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/http/caching_headers.h"

namespace net_instaweb {

// Based off of ApacheCachingHeaders in net/instaweb/apache/header_util.cc
// Needed so ps_header_filter can call GenerateDisabledCacheControl().
class NgxCachingHeaders : public CachingHeaders {
 public:
  explicit NgxCachingHeaders(ngx_http_request_t* request)
      : CachingHeaders(request->headers_out.status),
        request_(request) {
  }

  virtual bool Lookup(const StringPiece& key, StringPieceVector* values);

  virtual bool IsLikelyStaticResourceType() const {
    DCHECK(false);  // not called in our use-case.
    return false;
  }

  virtual bool IsCacheableResourceStatusCode() const {
    DCHECK(false);  // not called in our use-case.
    return false;
  }

 private:
  ngx_http_request_t* request_;

  DISALLOW_COPY_AND_ASSIGN(NgxCachingHeaders);
};

}  // namespace net_instaweb

#endif  // NGX_CACHING_HEADERS_H_
