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
//
// Interface for a hash function.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_HASHER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_HASHER_H_

#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class Hasher {
 public:
  virtual ~Hasher();

  // Interface to compute a hash of a single string.
  std::string Hash(const StringPiece& content);

  // Interface to accummulate a hash of data.
  virtual void Reset() = 0;
  virtual void Add(const StringPiece& content) = 0;
  virtual void ComputeHash(std::string* hash) = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_HASHER_H_
