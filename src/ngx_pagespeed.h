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

#ifndef NGX_PAGESPEED_H_
#define NGX_PAGESPEED_H_

// We might be compiled with syslog.h, which #defines LOG_INFO and LOG_WARNING
// as ints.  But logging.h assumes they're usable as names, within their
// namespace, so we need to #undef them before including logging.h
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_WARNING
#undef LOG_WARNING
#endif

extern "C" {
  #include <ngx_http.h>
}

#include "base/logging.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

class GzipInflater;
class NgxBaseFetch;
class ProxyFetch;
class RewriteDriver;
class RequestHeaders;
class ResponseHeaders;
class InPlaceResourceRecorder;

// Allocate chain links and buffers from the supplied pool, and copy over the
// data from the string piece.  If the string piece is empty, return
// NGX_DECLINED immediately unless send_last_buf.
ngx_int_t string_piece_to_buffer_chain(
    ngx_pool_t* pool, StringPiece sp,
    ngx_chain_t** link_ptr, bool send_last_buf, bool send_flush);

StringPiece str_to_string_piece(ngx_str_t s);

// s1: ngx_str_t, s2: string literal
// true if they're equal, false otherwise
#define STR_EQ_LITERAL(s1, s2)          \
    ((s1).len == (sizeof(s2)-1) &&      \
     ngx_strncmp((s1).data, (s2), (sizeof(s2)-1)) == 0)

// s1: ngx_str_t, s2: string literal
// true if they're equal ignoring case, false otherwise
#define STR_CASE_EQ_LITERAL(s1, s2)     \
    ((s1).len == (sizeof(s2)-1) &&      \
     ngx_strncasecmp((s1).data, (       \
       reinterpret_cast<u_char*>(       \
         const_cast<char*>(s2))),       \
       (sizeof(s2)-1)) == 0)

// Allocate memory out of the pool for the string piece, and copy the contents
// over.  Returns NULL if we can't get memory.
char* string_piece_to_pool_string(ngx_pool_t* pool, StringPiece sp);

enum PreserveCachingHeaders {
  kPreserveAllCachingHeaders,  // Cache-Control, ETag, Last-Modified, etc
  kPreserveOnlyCacheControl,   // Only Cache-Control.
  kDontPreserveHeaders,
};

typedef struct {
  NgxBaseFetch* base_fetch;

  ngx_http_request_t* r;

  bool html_rewrite;
  bool in_place;

  PreserveCachingHeaders preserve_caching_headers;

  // for html rewrite
  ProxyFetch* proxy_fetch;
  GzipInflater* inflater_;

  // for in place resource
  RewriteDriver* driver;
  InPlaceResourceRecorder* recorder;
  ResponseHeaders* ipro_response_headers;

  // We need to remember the URL here as well since we may modify what NGX
  // gets by stripping our special query params and honoring X-Forwarded-Proto.
  GoogleString url_string;

  // We need to remember if the upstream had headers_out->location set, because
  // we should mirror that when we write it back. nginx may absolutify
  // Location: headers that start with '/' without regarding X-Forwarded-Proto.
  bool location_field_set;
  bool psol_vary_accept_only;
  bool follow_flushes;
} ps_request_ctx_t;

ps_request_ctx_t* ps_get_request_context(ngx_http_request_t* r);

void copy_request_headers_from_ngx(const ngx_http_request_t* r,
                                   RequestHeaders* headers);

void copy_response_headers_from_ngx(const ngx_http_request_t* r,
                                    ResponseHeaders* headers);

ngx_int_t copy_response_headers_to_ngx(
    ngx_http_request_t* r,
    const ResponseHeaders& pagespeed_headers,
    PreserveCachingHeaders preserve_caching_headers);

StringPiece ps_determine_host(ngx_http_request_t* r);

namespace ps_base_fetch {

ngx_int_t ps_base_fetch_handler(ngx_http_request_t* r);

}  // namespace ps_base_fetch

}  // namespace net_instaweb

#endif  // NGX_PAGESPEED_H_
