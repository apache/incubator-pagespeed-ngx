// Copyright 2011 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef PAGESPEED_KERNEL_HTTP_HTTP_OPTIONS_H_
#define PAGESPEED_KERNEL_HTTP_HTTP_OPTIONS_H_

#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

// Any options which need to be accessed in http/ should be in here.
struct HttpOptions {
  // Weather to respect Vary headers for resources.
  // Vary is always respected for HTML.
  bool respect_vary;
  // TTL assigned to resources with no explicit caching headers.
  int64 implicit_cache_ttl_ms;

  // Allow copy and assign.
};

extern const HttpOptions kDefaultHttpOptionsForTests;

// TODO(sligocki): We should not be using these default options. Fix callers.
extern const HttpOptions kDeprecatedDefaultHttpOptions;

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTTP_HTTP_OPTIONS_H_
