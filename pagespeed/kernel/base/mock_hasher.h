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

#ifndef PAGESPEED_KERNEL_BASE_MOCK_HASHER_H_
#define PAGESPEED_KERNEL_BASE_MOCK_HASHER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class MockHasher : public Hasher {
 public:
  MockHasher()
      : Hasher(kint32max),
        hash_value_("\xd0") {  // base64-encodes to "0"
  }
  explicit MockHasher(StringPiece hash_value)
      : Hasher(hash_value.size()),
        hash_value_(hash_value.data(), hash_value.size()) {
  }

  virtual ~MockHasher();

  virtual GoogleString RawHash(const StringPiece& content) const {
    return hash_value_;
  }

  void set_hash_value(const GoogleString& new_hash_value) {
    hash_value_ = new_hash_value;
  }

  virtual int RawHashSizeInBytes() const { return hash_value_.length(); }

 private:
  GoogleString hash_value_;
  DISALLOW_COPY_AND_ASSIGN(MockHasher);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_MOCK_HASHER_H_
