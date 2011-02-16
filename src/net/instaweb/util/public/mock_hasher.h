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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MOCK_HASHER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MOCK_HASHER_H_

#include "net/instaweb/util/public/hasher.h"
#include <string>

namespace net_instaweb {

class MockHasher : public Hasher {
 public:
  MockHasher(): hash_value_("0") {}
  virtual ~MockHasher();

  virtual std::string Hash(const StringPiece& content) const {
    return hash_value_;
  }

  void set_hash_value(const std::string& new_hash_value) {
    hash_value_ = new_hash_value;
  }

  virtual int HashSizeInChars() const { return hash_value_.length(); }

 private:
  std::string hash_value_;
  DISALLOW_COPY_AND_ASSIGN(MockHasher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MOCK_HASHER_H_
