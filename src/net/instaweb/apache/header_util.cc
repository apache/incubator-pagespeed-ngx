// Copyright 2010 Google Inc.
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

#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/instaweb_context.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/http/public/headers.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/time_util.h"

#include "apr_strings.h"
#include "http_core.h"
#include "http_protocol.h"

namespace net_instaweb {

namespace {

int AddAttributeCallback(void *rec, const char *key, const char *value) {
  RequestHeaders* request_headers = static_cast<RequestHeaders*>(rec);
  request_headers->Add(key, value);
  return 1;
}

int AddResponseAttributeCallback(void *rec, const char *key,
                                 const char *value) {
  ResponseHeaders* response_headers = static_cast<ResponseHeaders*>(rec);
  response_headers->Add(key, value);
  return 1;
}

}  // namespace

void ApacheRequestToRequestHeaders(const request_rec& request,
                                   RequestHeaders* request_headers) {
  if (request.proto_num >= 1000) {
    // proto_num is the version number of protocol; 1.1 = 1001
    request_headers->set_major_version(request.proto_num / 1000);
    request_headers->set_minor_version(request.proto_num % 1000);
  }
  apr_table_do(AddAttributeCallback, request_headers, request.headers_in, NULL);
}

void ApacheRequestToResponseHeaders(const request_rec& request,
                                    ResponseHeaders* response_headers) {
  response_headers->set_status_code(request.status);
  if (request.proto_num >= 1000) {
    // proto_num is the version number of protocol; 1.1 = 1001
    response_headers->set_major_version(request.proto_num / 1000);
    response_headers->set_minor_version(request.proto_num % 1000);
  }
  apr_table_do(AddResponseAttributeCallback, response_headers,
               request.headers_out, NULL);
}

void ResponseHeadersToApacheRequest(const ResponseHeaders& response_headers,
                                    request_rec* request) {
  request->status = response_headers.status_code();
  // proto_num is the version number of protocol; 1.1 = 1001
  request->proto_num = response_headers.major_version() * 1000 +
                       response_headers.minor_version();
  AddResponseHeadersToRequest(response_headers, request);
}

void AddResponseHeadersToRequest(const ResponseHeaders& response_headers,
                                 request_rec* request) {
  for (int i = 0, n = response_headers.NumAttributes(); i < n; ++i) {
    const GoogleString& name = response_headers.Name(i);
    const GoogleString& value = response_headers.Value(i);
    if (StringCaseEqual(name, HttpAttributes::kContentType)) {
      // ap_set_content_type does not make a copy of the string, we need
      // to duplicate it.
      char* ptr = apr_pstrdup(request->pool, value.c_str());
      ap_set_content_type(request, ptr);
    } else {
      if (StringCaseEqual(name, HttpAttributes::kCacheControl)) {
        DisableDownstreamHeaderFilters(request);
      }
      // apr_table_add makes copies of both head key and value, so we do not
      // have to duplicate them.
      apr_table_add(request->headers_out, name.c_str(), value.c_str());
    }
  }
}

void DisableDownstreamHeaderFilters(request_rec* request) {
  // Prevent downstream filters from corrupting our headers.
  ap_filter_t* filter = request->output_filters;
  while (filter != NULL) {
    ap_filter_t* next = filter->next;
    if ((StringCaseEqual(filter->frec->name, "MOD_EXPIRES")) ||
        (StringCaseEqual(filter->frec->name, "FIXUP_HEADERS_OUT"))) {
      ap_remove_output_filter(filter);
    }
    filter = next;
  }
}

int PrintAttributeCallback(void *rec, const char *key, const char *value) {
  fprintf(stdout, "    %s: %s\n", key, value);
  return 1;
}

// This routine is intended for debugging so fprintf to stdout is the way
// to get instant feedback.
void PrintHeaders(request_rec* request) {
  fprintf(stdout, "Input headers:\n");
  apr_table_do(PrintAttributeCallback, NULL, request->headers_in, NULL);
  fprintf(stdout, "Output headers:\n");
  apr_table_do(PrintAttributeCallback, NULL, request->headers_out, NULL);
  fflush(stdout);
}

}  // namespace net_instaweb
