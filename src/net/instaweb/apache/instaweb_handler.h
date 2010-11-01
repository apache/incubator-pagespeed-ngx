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

#ifndef MOD_INSTAWEB_INSTAWEB_HANDLER_H_
#define MOD_INSTAWEB_INSTAWEB_HANDLER_H_

// The httpd header must be after the instaweb_context.h. Otherwise,
// the compiler will complain
// "strtoul_is_not_a_portable_function_use_strtol_instead".
#include "net/instaweb/apache/instaweb_context.h"
#include "httpd.h"  // NOLINT

namespace net_instaweb {

// The content generator for instaweb generated content, for example, the
// combined CSS file.  Requests for not-instab generated content will be
// declined so that other Apache handlers may operate on them.
int instaweb_handler(request_rec* request);

// output-filter function to repair caching headers, which might have
// been altered by a directive like:
//
//     <FilesMatch "\.(jpg|jpeg|gif|png|js|css)$">
//       Header set Cache-control "public, max-age=600"
//     </FilesMatch>
apr_status_t repair_caching_header(ap_filter_t *filter, apr_bucket_brigade *bb);

}  // namespace net_instaweb

#endif  // MOD_INSTAWEB_INSTAWEB_HANDLER_H_
