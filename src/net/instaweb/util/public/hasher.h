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
//
// Interface for a hash function.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_HASHER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_HASHER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Hasher {
 public:
  // The passed in max_chars will be used to limit the length of
  // Hash() and HashSizeInChars()
  explicit Hasher(int max_chars);
  virtual ~Hasher();

  // Computes a web64-encoded hash of a single string.  This
  // operation is thread-safe.
  //
  // This is implemented in terms of RawHash, and honors the length limit
  // passed in to the constructor.
  GoogleString Hash(const StringPiece& content) const;

  // Return string length of hashes produced by this hasher's Hash
  // method.
  //
  // This is implemented in terms of RawHashSizeInBytes() and the length limit
  // passed in to the constructor.
  int HashSizeInChars() const;

  // Computes a binary hash of the given content. The returned value
  // is not printable as it is the direct binary encoding of the hash.
  // This operation is thread-safe.
  virtual GoogleString RawHash(const StringPiece& content) const = 0;

  // The number of bytes RawHash will produce.
  virtual int RawHashSizeInBytes() const = 0;

 private:
  int max_chars_;  // limit on length of Hash/HashSizeInChars set by subclass.

  DISALLOW_COPY_AND_ASSIGN(Hasher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_HASHER_H_
