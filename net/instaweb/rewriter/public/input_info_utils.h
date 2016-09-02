/*
 * Copyright 2016 Google Inc.
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

// Author: morlovich@google.com (Maks Orlovich)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_INPUT_INFO_UTILS_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_INPUT_INFO_UTILS_H_

#include "net/instaweb/rewriter/input_info.pb.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

namespace input_info_utils {

// Computes whether the given input_info is valid at now_ms, with the filesystem
// and its metadata cache in server_context, considering invalidation
// information and policy in options.
//
// *purged will be set if the entry was invalidated due to a cache purge.
// *stale_rewrite will be set (and true will be returned) if
// options->metadata_cache_staleness_threshold_ms() permitted reuse past
// expiration at this time, andthe rewrite isn't nested.
bool IsInputValid(
    ServerContext* server_context, const RewriteOptions* options,
    bool nested_rewrite, const InputInfo& input_info,
    int64 now_ms, bool* purged, bool* stale_rewrite);

}  // namespace input_info_utils
}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_INPUT_INFO_UTILS_H_
