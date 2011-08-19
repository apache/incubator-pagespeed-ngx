/*
 * Copyright 2011 Google Inc.
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

// Author: jhoch@google.com (Jason Hoch)

#include "net/instaweb/util/public/hashed_referer_statistics.h"

namespace net_instaweb {

class AbstractSharedMem;

HashedRefererStatistics::HashedRefererStatistics(
    size_t number_of_strings,
    size_t average_string_length,
    AbstractSharedMem* shm_runtime,
    const GoogleString& filename_prefix,
    const GoogleString& filename_suffix,
    Hasher* hasher)
    : SharedMemRefererStatistics(number_of_strings,
                                 average_string_length,
                                 shm_runtime,
                                 filename_prefix,
                                 filename_suffix),
      hasher_(hasher) {
}

GoogleString HashedRefererStatistics::GetEntryStringForUrlString(
     const StringPiece& url) const {
  return hasher_->Hash(url);
}

GoogleString HashedRefererStatistics::GetEntryStringForDivLocation(
     const StringPiece& div_location) const {
  // An empty div location signifies that there is no div location, but the
  // hash of an empty string isn't empty, so a little meddling is necessary.
  if (div_location.empty()) {
    return div_location.as_string();
  } else {
    return hasher_->Hash(div_location);
  }
}

}  // namespace net_instaweb
