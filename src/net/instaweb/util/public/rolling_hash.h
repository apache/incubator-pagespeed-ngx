/*
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
// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_ROLLING_HASH_H_
#define NET_INSTAWEB_UTIL_PUBLIC_ROLLING_HASH_H_

#include <cstddef>
#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

// Rolling hash for char buffers based on a polynomial lookup table.
// See http://en.wikipedia.org/wiki/Rolling_hash

// Per character hash values.  Exported for use in NextRollingHash.
extern const uint64 kRollingHashCharTable[256];

// Compute the rolling hash of buf[start : start + n - 1]
uint64 RollingHash(const char* buf, size_t start, size_t n);

// Given the rolling hash prev of buf[start - 1 : start + n - 2], efficiently
// compute the hash of buf[start : start + n - 1].  Note that this indexes
// buf[start - 1], so we can't just use a StringPiece here.  We eschew
// StringPiece in any case, because of efficiency.
//
// Note that to get efficient operation here for fixed n (eg when we're doing
// something like Rabin-Karp string matching), we must inline the computation of
// shift amounts and then hoist them as loop invariants.  That is why this
// function (intended for use in an inner loop) is inlined.
inline uint64 NextRollingHash(
    const char* buf, size_t start, size_t n, uint64 prev) {
  // In a reasonable loop, the following two tests should be eliminated based on
  // contextual information, if our compiler is optimizing enough.
  CHECK_LT(static_cast<size_t>(0), start);
  uint64 start_hash =
      kRollingHashCharTable[static_cast<uint8>(buf[start - 1])];
  uint64 end_hash =
      kRollingHashCharTable[static_cast<uint8>(buf[start - 1 + n])];
  uint64 prev_rot1 = (prev << 1) | (prev >> 63);  // rotate left 1
  uint64 start_hash_rotn;
  // Corner case: shift by >= 64 bits is not defined in C.  gcc had better
  // constant-fold this to a rotate!  (It appears to.)  We inline in large part
  // to ensure the truthiness of this fact.
  size_t shift = n % 64;
  if (shift == 0) {
    start_hash_rotn = start_hash;
  } else {
    // rotate left by shift (equiv to rotating left n times).
    start_hash_rotn = (start_hash << shift) | (start_hash >> (64 - shift));
  }
  return (start_hash_rotn ^ prev_rot1 ^ end_hash);
}
}  // net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_ROLLING_HASH_H_
