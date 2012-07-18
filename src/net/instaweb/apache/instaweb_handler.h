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
// Author: lsong@google.com (Libo Song)
//         jmarantz@google.com (Joshua Marantz)
//
// The Apache handler for rewriten resources and a couple other Apache hooks.

#ifndef MOD_INSTAWEB_INSTAWEB_HANDLER_H_
#define MOD_INSTAWEB_INSTAWEB_HANDLER_H_

// The httpd header must be after the instaweb_context.h. Otherwise,
// the compiler will complain
// "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "net/instaweb/apache/instaweb_context.h"
#include "httpd.h"  // NOLINT

namespace net_instaweb {

// Handle mod_pagespeed-specific requests. Handles both .pagespeed. rewritten
// resources and /mod_pagespeed_statistics, /mod_pagespeed_beacon, etc.
// TODO(sligocki): Why not make each of these different handlers?
apr_status_t instaweb_handler(request_rec* request);

// Save the original URL as a request "note" before mod_rewrite has
// a chance to corrupt mod_pagespeed's generated URLs, which would
// prevent instaweb_handler from being able to decode the resource.
apr_status_t save_url_hook(request_rec *request);

// By default, apache imposes limitations on URL segments of around
// 256 characters that appear to correspond to filename limitations.
// To prevent that, we hook map_to_storage for our own purposes.
apr_status_t instaweb_map_to_storage(request_rec* request);

}  // namespace net_instaweb

#endif  // MOD_INSTAWEB_INSTAWEB_HANDLER_H_
