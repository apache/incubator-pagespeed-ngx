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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/util/public/hasher.h"

#include <algorithm>

#include "base/logging.h"
#include "net/instaweb/util/public/base64_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

Hasher::Hasher(int max_chars): max_chars_(max_chars) {
  CHECK_LE(0, max_chars);
}

Hasher::~Hasher() {
}

GoogleString Hasher::Hash(const StringPiece& content) const {
  GoogleString raw_hash = RawHash(content);
  GoogleString out;
  Web64Encode(raw_hash, &out);

  // Truncate to how many characters are actually requested. We use
  // HashSizeInChars() here for consistency of rounding.
  out.resize(HashSizeInChars());
  return out;
}

int Hasher::HashSizeInChars() const {
  // For char hashes, we return the hash after Base64 encoding, which expands by
  // 4/3. We round down, this should not matter unless someone really wants that
  // extra few bits.
  return std::min(max_chars_, RawHashSizeInBytes() * 4 / 3);
}

}  // namespace net_instaweb
