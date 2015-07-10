// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef PAGESPEED_APACHE_HEADER_UTIL_H_
#define PAGESPEED_APACHE_HEADER_UTIL_H_

#include <cstddef>

#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/string_util.h"

struct request_rec;

namespace net_instaweb {

class RequestHeaders;
class ResponseHeaders;

// Defines a predicate function used to select which request-headers
// to copy.  The callback sets the bool* arg (.second) to true if
// it wants to include the header.
//
// The StringPiece is the name of the header.
typedef Callback2<StringPiece, bool*> HeaderPredicateFn;

// Converts Apache header structure into RequestHeaders, selecting
// only those for which the predicate sets its bool* argument to true.
// If the predicate is NULL, then all the headers are transferred.
//
// The predicate should be created with NewPermanentCallback and stored
// in a scoped_ptr<Callback2>, so that it is deleted after this function
// completes.
void ApacheRequestToRequestHeaders(const request_rec& request,
                                   RequestHeaders* request_headers,
                                   HeaderPredicateFn* predicate);

// Fully converts apache request header structure into RequestHeaders.
inline void ApacheRequestToRequestHeaders(const request_rec& request,
                                          RequestHeaders* request_headers) {
  return ApacheRequestToRequestHeaders(request, request_headers, NULL);
}

// Converts Apache header structure (request.headers_out) into ResponseHeaders
// headers. If err_headers is not NULL then request.err_headers_out is copied
// into it. In the event that headers == err_headers, the headers from
// request.err_headers_out will be appended to the list of headers, but no
// merging occurs.
void ApacheRequestToResponseHeaders(const request_rec& request,
                                    ResponseHeaders* headers,
                                    ResponseHeaders* err_headers);

// Converts the ResponseHeaders to the output headers.  This function
// does not alter the status code or the major/minor version of the
// Apache request.
void ResponseHeadersToApacheRequest(const ResponseHeaders& response_headers,
                                    request_rec* request);

// Converts ResponseHeaders into Apache request err_headers.  This
// function does not alter the status code or the major/minor version
// of the Apache request.
void ErrorHeadersToApacheRequest(const ResponseHeaders& err_response_headers,
                                 request_rec* request);

// Remove downstream filters that might corrupt our caching headers.
void DisableDownstreamHeaderFilters(request_rec* request);

// Debug utility for printing Apache headers to stdout
void PrintHeaders(request_rec* request);

// Get request->headers_out as a string, intended for tests.
GoogleString HeadersOutToString(request_rec* request);

// Get request->subprocess_env as a string, intended for tests.
GoogleString SubprocessEnvToString(request_rec* request);

// Updates headers related to caching (but not Cache-Control).
void DisableCachingRelatedHeaders(request_rec* request);

// Updates caching headers to ensure the resulting response is not cached.
// Removes any max-age specification, and adds max-age=0, no-cache.
void DisableCacheControlHeader(request_rec* request);

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_HEADER_UTIL_H_
