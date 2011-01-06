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

#include "net/instaweb/apache/header_util.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/instaweb_context.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/util/public/simple_meta_data.h"
#include "net/instaweb/util/public/time_util.h"

#include "apr_strings.h"
#include "http_core.h"
#include "http_protocol.h"

namespace net_instaweb {

namespace {

int AddAttributeCallback(void *rec, const char *key, const char *value) {
  MetaData* meta_data = static_cast<MetaData*>(rec);
  meta_data->Add(key, value);
  return 1;
}

}  // namespace

// proto_num is the version number of protocol; 1.1 = 1001
void ApacheHeaderToMetaData(const apr_table_t* apache_headers,
                            int status_code,
                            int proto_num,
                            MetaData* meta_data) {
  meta_data->SetStatusAndReason(static_cast<HttpStatus::Code>(status_code));
  if (proto_num >= 1000) {
    meta_data->set_major_version(proto_num / 1000);
    meta_data->set_minor_version(proto_num % 1000);
  }
  apr_table_do(AddAttributeCallback, meta_data, apache_headers, NULL);
}

void MetaDataToApacheHeader(const MetaData& meta_data, request_rec* request) {
  request->status = meta_data.status_code();
  request->proto_num =
      (meta_data.major_version() * 1000) + meta_data.minor_version();
  for (int i = 0, n = meta_data.NumAttributes(); i < n; ++i) {
    const char* name = meta_data.Name(i);
    const char* value = meta_data.Value(i);
    if (strcasecmp(name, HttpAttributes::kContentType) == 0) {
      // ap_set_content_type does not make a copy of the string, we need
      // to duplicate it.
      char* ptr = apr_pstrdup(request->pool, value);
      ap_set_content_type(request, ptr);
    } else {
      if (strcasecmp(name, HttpAttributes::kCacheControl) == 0) {
        DisableDownstreamHeaderFilters(request);
      }
      // apr_table_add makes copies of both head key and value, so we do not
      // have to duplicate them.
      apr_table_add(request->headers_out, name, value);
    }
  }
}

void DisableDownstreamHeaderFilters(request_rec* request) {
  // Prevent downstream filters from corrupting our headers.
  ap_filter_t* filter = request->output_filters;
  while (filter != NULL) {
    ap_filter_t* next = filter->next;
    if ((strcasecmp(filter->frec->name, "MOD_EXPIRES") == 0) ||
        (strcasecmp(filter->frec->name, "FIXUP_HEADERS_OUT") == 0)) {
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
  apr_table_do(PrintAttributeCallback, NULL, request->headers_in, NULL);
  fflush(stdout);
}

}  // namespace net_instaweb
