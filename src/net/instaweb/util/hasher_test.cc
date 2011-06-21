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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/util/public/hasher.h"

#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

// Hasher implementation with a trivial RawHash function.
class DummyHasher : public Hasher {
 public:
  DummyHasher() : Hasher(10) {}
  virtual ~DummyHasher() {}

  // Arbitrary number of bytes to return (> 8).
  virtual int RawHashSizeInBytes() const {
    return 16;
  }

  virtual GoogleString RawHash(const StringPiece& content) const {
    GoogleString result = content.as_string();
    result.resize(RawHashSizeInBytes());
    CHECK_EQ(RawHashSizeInBytes(), result.size());
    return result;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyHasher);
};

TEST(HasherTest, HashToUint64) {
  const DummyHasher hasher;
  EXPECT_EQ(0x0000000000000000ull,
            hasher.HashToUint64("\x00\x00\x00\x00\x00\x00\x00\x00"));
  EXPECT_EQ(0xFFFFFFFFFFFFFFFFull,
            hasher.HashToUint64("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"));
  EXPECT_EQ(0x0123456789ABCDEFull,
            hasher.HashToUint64("\x01\x23\x45\x67\x89\xAB\xCD\xEF"));
  EXPECT_EQ(0xDEADBEEF00000000ull,
            hasher.HashToUint64("\xDE\xAD\xBE\xEF"));
  EXPECT_EQ(0x3133703133703133ull,
            hasher.HashToUint64("\x31\x33\x70\x31\x33\x70"
                                "\x31\x33\x70\x31\x33\x70"));
}

}  // namespace

}  // namespace net_instaweb
