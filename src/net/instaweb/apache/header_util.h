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

#ifndef NET_INSTAWEB_APACHE_HEADER_UTIL_H_
#define NET_INSTAWEB_APACHE_HEADER_UTIL_H_

#include "net/instaweb/util/public/meta_data.h"
// The httpd header must be after the meta_data.h.  Otherwise, the
// compiler will complain
//   "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "httpd.h"

namespace net_instaweb {

// Converts Apache header structure into an Instaweb MetaData.
//
// proto_num is the version number of protocol; 1.1 = 1001
void ApacheHeaderToMetaData(const apr_table_t* apache_headers,
                            int status_code,
                            int proto_num,
                            MetaData* meta_data);

// Converts MetaData structure into an Apache header.
//
// proto_num is the version number of protocol; 1.1 = 1001
void MetaDataToApacheHeader(const MetaData& meta_data,
                            apr_table_t* apache_headers,
                            int* status_code,
                            int* proto_num);

// Examines a cache-control string and updates the output headers in
// request to match it.  If the response is cacheable, then
// we assume it's cacheable forever (via cache extension, and
// so we set an etag and a matching Expires header.
//
// If the content is not cacheable, then we instead clear the etag,
// last-modified, and expires, in addition to setting the cache-control
// as specified.
void SetCacheControl(const char* cache_control, request_rec* request);

// Like SetCacheControl but only updates the other headers, does not
// set the cache-control header itself.  Call this if cache-control is
// already set and you wish to make the other headers consistent.
void UpdateCacheHeaders(const char* cache_control, request_rec* request);

// mod_headers typically runs after mod_pagespeed and borks its headers.
// So we must insert a late filter to unbork the headers.
void SetupCacheRepair(const char* cache_control, request_rec* request);

// This is called from the late-running filter to unbork the headers.
void RepairCachingHeaders(request_rec* request);

// Debug utility for printing Apache headers to stdout
void PrintHeaders(request_rec* request);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_HEADER_UTIL_H_
