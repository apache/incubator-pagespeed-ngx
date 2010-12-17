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

#include "http_core.h"

namespace net_instaweb {

namespace {

const char kRepairCachingHeader[] = "X-Mod-Pagespeed-Repair";

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

void MetaDataToApacheHeader(const MetaData& meta_data,
                            apr_table_t* apache_headers,
                            int* status_code,
                            int* proto_num) {
  *status_code = meta_data.status_code();
  *proto_num = (meta_data.major_version() * 1000) + meta_data.minor_version();
  for (int i = 0, n = meta_data.NumAttributes(); i < n; ++i) {
    apr_table_add(apache_headers, meta_data.Name(i), meta_data.Value(i));
  }
}

void UpdateCacheHeaders(const char* cache_control, request_rec* request) {
  SimpleMetaData response_headers;
  response_headers.set_status_code(request->status);
  if (request->proto_num >= 1000) {
    response_headers.set_major_version(request->proto_num / 1000);
    response_headers.set_minor_version(request->proto_num % 1000);
  }
  AprTimer timer;
  response_headers.Add(HttpAttributes::kCacheControl, cache_control);
  response_headers.SetDate(timer.NowMs());
  response_headers.ComputeCaching();
  if (response_headers.IsCacheable()) {
    apr_table_set(request->headers_out, HttpAttributes::kCacheControl,
                  cache_control);
    apr_table_setn(request->headers_out, HttpAttributes::kEtag,
                   ResourceManager::kResourceEtagValue);  // no copy neeeded

    // Convert our own cache-control data into an Expires header.
    int64 expire_time_ms = response_headers.CacheExpirationTimeMs();
    bool unset_expires = true;
    if (expire_time_ms > 0) {
      std::string time_string;
      if (ConvertTimeToString(expire_time_ms, &time_string)) {
        apr_table_set(request->headers_out, HttpAttributes::kExpires,
                      time_string.c_str());
        unset_expires = false;
      }
    }
    if (unset_expires) {
      apr_table_unset(request->headers_out, HttpAttributes::kExpires);
    }
  } else {
    apr_table_unset(request->headers_out, HttpAttributes::kExpires);
    apr_table_unset(request->headers_out, HttpAttributes::kEtag);
    apr_table_unset(request->headers_out, HttpAttributes::kLastModified);
  }
}

void SetupCacheRepair(const char* cache_control, request_rec* request) {
  // In case Apache configuration directives set up caching headers,
  // we will need override our cache-extended resources in a late-running
  // output filter.
  // TODO(jmarantz): Do not use headers_out as out message passing
  // mechanism. Switch to configuration vectors or something like that
  // instead.
  apr_table_add(request->headers_out, kRepairCachingHeader, cache_control);
  // Add the repair headers filter to fix the cacheing header.
  ap_add_output_filter(InstawebContext::kRepairHeadersFilterName,
                       NULL, request, request->connection);
}

void RepairCachingHeaders(request_rec* request) {
  const char* cache_control = apr_table_get(request->headers_out,
                                            kRepairCachingHeader);
  if (cache_control != NULL) {
    SetCacheControl(cache_control, request);
    apr_table_unset(request->headers_out, kRepairCachingHeader);
  }
}

void SetCacheControl(const char* cache_control, request_rec* request) {
  UpdateCacheHeaders(cache_control, request);
  apr_table_set(request->headers_out, HttpAttributes::kCacheControl,
                cache_control);
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
