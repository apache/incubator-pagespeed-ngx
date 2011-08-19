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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_HASHED_REFERER_STATISTICS_H_
#define NET_INSTAWEB_UTIL_PUBLIC_HASHED_REFERER_STATISTICS_H_

#include <cstddef>
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/shared_mem_referer_statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractSharedMem;

// An implementation of SharedMemRefererStatistics that hashes all Url and
// div location information.
//   Encoding and decoding of referals is inherited, so useful pre-fetch
// information can still be inferred anonymously.
class HashedRefererStatistics : public SharedMemRefererStatistics {
 public:
  HashedRefererStatistics(size_t number_of_strings,
                          size_t average_string_length,
                          AbstractSharedMem* shm_runtime,
                          const GoogleString& filename_prefix,
                          const GoogleString& filename_suffix,
                          Hasher* hasher);

 protected:
  GoogleString GetEntryStringForUrlString(const StringPiece& url) const;
  GoogleString GetEntryStringForDivLocation(const StringPiece& url) const;

 private:
  scoped_ptr<Hasher> hasher_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_HASHED_REFERER_STATISTICS_H_
