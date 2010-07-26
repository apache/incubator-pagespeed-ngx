/**
 * Copyright 2010 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)
//         jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/resource.h"

namespace net_instaweb {

const int64 Resource::kDefaultExpireTimeMs = 5 * 60 * 1000;

Resource::~Resource() {
}

int64 Resource::CacheExpirationTimeMs() const {
  int64 input_expire_time_ms = kDefaultExpireTimeMs;
  if (meta_data_.IsCacheable()) {
    input_expire_time_ms = meta_data_.CacheExpirationTimeMs();
  }
  return input_expire_time_ms;
}

}  // namespace net_instaweb
