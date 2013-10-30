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

#include <cstdio>
#include <memory>

#include "base/logging.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/http/caching_headers.h"

#include "apr_strings.h"
#include "http_core.h"
#include "http_protocol.h"
#include "httpd.h"

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
                                    ResponseHeaders* headers,
                                    ResponseHeaders* err_headers) {
  headers->set_status_code(request.status);
  if (request.proto_num >= 1000) {
    // proto_num is the version number of protocol; 1.1 = 1001
    headers->set_major_version(request.proto_num / 1000);
    headers->set_minor_version(request.proto_num % 1000);
  }
  apr_table_do(AddResponseAttributeCallback, headers,
               request.headers_out, NULL);
  if (err_headers != NULL) {
    apr_table_do(AddResponseAttributeCallback, err_headers,
                 request.err_headers_out, NULL);
  }
}

void AddResponseHeadersToRequestHelper(const ResponseHeaders& response_headers,
                                       request_rec* request,
                                       apr_table_t* table) {
  for (int i = 0, n = response_headers.NumAttributes(); i < n; ++i) {
    const GoogleString& name = response_headers.Name(i);
    const GoogleString& value = response_headers.Value(i);
    if (StringCaseEqual(name, HttpAttributes::kContentType)) {
      // ap_set_content_type does not make a copy of the string, we need
      // to duplicate it.
      char* ptr = apr_pstrdup(request->pool, value.c_str());
      ap_set_content_type(request, ptr);
    } else {
      // apr_table_add makes copies of both head key and value, so we do not
      // have to duplicate them.
      apr_table_add(table, name.c_str(), value.c_str());
    }
  }
}

void ResponseHeadersToApacheRequest(const ResponseHeaders& response_headers,
                                    request_rec* request) {
  AddResponseHeadersToRequestHelper(response_headers, request,
                                    request->headers_out);
}

void ErrorHeadersToApacheRequest(const ResponseHeaders& err_response_headers,
                                 request_rec* request) {
  AddResponseHeadersToRequestHelper(err_response_headers, request,
                                    request->err_headers_out);
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
  fprintf(stdout, "Err_Output headers:\n");
  apr_table_do(PrintAttributeCallback, NULL, request->err_headers_out, NULL);
  fflush(stdout);
}

class ApacheCachingHeaders : public CachingHeaders {
 public:
  explicit ApacheCachingHeaders(request_rec* request)
      : CachingHeaders(request->status),
        request_(request) {
  }

  virtual bool Lookup(const StringPiece& key, StringPieceVector* values) {
    const char* value = apr_table_get(request_->headers_out,
                                      key.as_string().c_str());
    if (value == NULL) {
      return false;
    }
    SplitStringPieceToVector(value, ",", values, true);
    for (int i = 0, n = values->size(); i < n; ++i) {
      TrimWhitespace(&((*values)[i]));
    }
    return true;
  }

  virtual bool IsLikelyStaticResourceType() const {
    DCHECK(false);  // not called in our use-case.
    return false;
  }

  virtual bool IsCacheableResourceStatusCode() const {
    DCHECK(false);  // not called in our use-case.
    return false;
  }

 private:
  request_rec* request_;

  DISALLOW_COPY_AND_ASSIGN(ApacheCachingHeaders);
};

void DisableCacheControlHeader(request_rec* request) {
  // Turn off Cache-Control header for the HTTP requests.
  ApacheCachingHeaders headers(request);
  apr_table_set(request->headers_out, HttpAttributes::kCacheControl,
                headers.GenerateDisabledCacheControl().c_str());
}

void DisableCachingRelatedHeaders(request_rec* request) {
  // Turn off headers related to caching (but not Cache-Control) for the
  // HTTP requests.
  StringPieceVector response_headers_to_remove =
      HttpAttributes::CachingHeadersToBeRemoved();
  for (int i = 0, n = response_headers_to_remove.size(); i < n; ++i) {
    apr_table_unset(request->headers_out,
                    response_headers_to_remove[i].as_string().c_str());
  }
}

}  // namespace net_instaweb
