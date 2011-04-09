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

#include "net/instaweb/util/public/rolling_hash.h"

#include "net/instaweb/util/public/dense_hash_set.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
namespace {

const char kTestString[] =
    "The quick brown fox jumps over the lazy dog.\n"
    "Now is the time for ALL good men to come to the aid of their party.\r\n"
    "@$%^@#$%#^%^987293 458798\x8f\xfa\xce\t";
const size_t kTestStringSize = STATIC_STRLEN(kTestString);

TEST(RollingHashTest, EmptyString) {
  EXPECT_EQ(0, RollingHash("", 0, 0));
  EXPECT_EQ(0, RollingHash(NULL, 0, 0));
}

TEST(RollingHashTest, SingleChar) {
  EXPECT_EQ(kRollingHashCharTable[' '], RollingHash(" ", 0, 1));
}

TEST(RollingHashTest, SingleRoll) {
  static const char kCSpace[] = "C ";
  uint64 h0 = RollingHash(kCSpace, 0, 1);
  EXPECT_EQ(kRollingHashCharTable['C'], h0);
  EXPECT_EQ(kRollingHashCharTable[' '], NextRollingHash(kCSpace, 1, 1, h0));
}

TEST(RollingHashTest, RollShakedown) {
  for (size_t i = 1; i < kTestStringSize; ++i) {
    uint64 hash = RollingHash(kTestString, 0, i);
    for (size_t j = 1; j < kTestStringSize - i; ++j) {
      hash = NextRollingHash(kTestString, j, i, hash);
      EXPECT_EQ(RollingHash(kTestString, j, i), hash);
    }
  }
}

// Prove that there are no trivial 1-, 2-, or 3-gram overlaps.
// Note that the open-vcdiff rolling hash cannot pass this test
// as it only has 23 bits!
TEST(RollingHashTest, NGrams) {
  // Using dense_hash_set is MUCH faster (6x) than std::set.
  // That keeps this test "small".
  dense_hash_set<uint64> grams;
  grams.set_empty_key(0ULL);
  char buf[3];
  uint64 hash;
  bool gramOverlap = false;
  for (int i = 0; i < 256; i++) {
    buf[0] = i;
    hash = RollingHash(buf, 0, 1);
    ASSERT_NE(0, hash);
    if (!grams.insert(hash).second) {
      gramOverlap = true;
      printf("Gram overlap including %2x", i);
    }
  }
  for (int i = 0; i < 256; i++) {
    buf[0] = i;
    for (int j = 0; j < 256; j++) {
      buf[1] = j;
      hash = RollingHash(buf, 0, 2);
      ASSERT_NE(0, hash);
      if (!grams.insert(hash).second) {
        gramOverlap = true;
        printf("Gram overlap including %2x %2x", i, j);
      }
    }
  }
  for (int i = 0; i < 256; i++) {
    buf[0] = i;
    for (int j = 0; j < 256; j++) {
      buf[1] = j;
      for (int k = 0; k < 256; k++) {
        buf[2] = k;
        hash = RollingHash(buf, 0, 3);
        ASSERT_NE(0, hash);
        if (!grams.insert(hash).second) {
          gramOverlap = true;
          printf("Gram overlap including %2x %2x %2x\n", i, j, k);
        }
      }
    }
  }
  EXPECT_FALSE(gramOverlap);
}

}  // namespace
}  // namespace net_instaweb
