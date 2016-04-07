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
// Author: jefftk@google.com (Jeff Kaufman)

#ifndef PAGESPEED_APACHE_MOD_INSTAWEB_H_
#define PAGESPEED_APACHE_MOD_INSTAWEB_H_

#include "http_config.h"
#include "pagespeed/apache/apache_httpd_includes.h"

namespace net_instaweb {

// Filter used for HTML rewriting.
const char kModPagespeedFilterName[] = "MOD_PAGESPEED_OUTPUT_FILTER";
// Filter used to fix headers after mod_headers runs.
const char kModPagespeedFixHeadersName[] = "MOD_PAGESPEED_FIX_HEADERS_FILTER";
// Filters used for In-Place Resource Optimization.
// First filter stores un-gzipped contents.
const char kModPagespeedInPlaceFilterName[] = "MOD_PAGESPEED_IN_PLACE_FILTER";
// Second filter fixes headers to avoid caching by shared proxies.
const char kModPagespeedInPlaceFixHeadersName[] =
    "MOD_PAGESPEED_IN_PLACE_FIX_HEADERS_FILTER";
// Third filter checks headers for cacheability and writes the recorded resource
// to our cache.
const char kModPagespeedInPlaceCheckHeadersName[] =
    "MOD_PAGESPEED_IN_PLACE_CHECK_HEADERS_FILTER";

}  // namespace net_instaweb

extern "C" {
extern module AP_MODULE_DECLARE_DATA pagespeed_module;
}

#endif  // PAGESPEED_APACHE_MOD_INSTAWEB_H_
