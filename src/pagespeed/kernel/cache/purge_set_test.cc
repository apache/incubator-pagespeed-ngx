/*
 * Copyright 2013 Google Inc.
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

// Unit-test PurgeSet

#include "pagespeed/kernel/cache/purge_set.h"

#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {
const size_t kMaxSize = 100;
}

class PurgeSetTest : public testing::Test {
 protected:
  PurgeSetTest() : purge_set_(kMaxSize) {
  }

  PurgeSet purge_set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PurgeSetTest);
};

TEST_F(PurgeSetTest, SimpleInvaldations) {
  EXPECT_TRUE(purge_set_.IsValid("a", 1));
  ASSERT_TRUE(purge_set_.Put("a", 2));
  EXPECT_FALSE(purge_set_.IsValid("a", 1));
  EXPECT_TRUE(purge_set_.IsValid("a", 3));
  EXPECT_TRUE(purge_set_.IsValid("b", 1));
}

TEST_F(PurgeSetTest, NoEvictionsOnUpdateSameEntry) {
  int64 last_eviction_time_ms = 1;
  for (int i = 0; i < kMaxSize * 10; ++i) {
    ++last_eviction_time_ms;
    ASSERT_TRUE(purge_set_.Put("a", last_eviction_time_ms));
  }
  EXPECT_EQ(0, purge_set_.global_invalidation_timestamp_ms());
  EXPECT_FALSE(purge_set_.IsValid("a", 1));
  EXPECT_FALSE(purge_set_.IsValid("a", last_eviction_time_ms));
  EXPECT_TRUE(purge_set_.IsValid("a", last_eviction_time_ms + 1));
  EXPECT_TRUE(purge_set_.IsValid("b", 1));
}

TEST_F(PurgeSetTest, EvictionsOnUpdateNewEntries) {
  for (int i = 0; i < kMaxSize * 10; ++i) {
    ASSERT_TRUE(purge_set_.Put(StrCat("a", IntegerToString(i)), i + 1));
  }
  EXPECT_LT(0, purge_set_.global_invalidation_timestamp_ms());
  EXPECT_FALSE(purge_set_.IsValid("a", 1));
  EXPECT_FALSE(purge_set_.IsValid("b", 1));

  // Check that all explicitly disallowed entries are still disallowed
  // whether they are before or after the global invalidation
  // timestamp.
  for (int i = 0; i < kMaxSize * 10; ++i) {
    EXPECT_FALSE(purge_set_.IsValid(StrCat("a", IntegerToString(i)), i + 1));
  }
}

TEST_F(PurgeSetTest, Merge) {
  ASSERT_TRUE(purge_set_.UpdateGlobalInvalidationTimestampMs(10));
  ASSERT_TRUE(purge_set_.Put("b", 50));
  EXPECT_FALSE(purge_set_.IsValid("c", 5));
  EXPECT_TRUE(purge_set_.IsValid("c", 20));
  EXPECT_FALSE(purge_set_.IsValid("b", 40));

  PurgeSet src(kMaxSize);
  ASSERT_TRUE(src.UpdateGlobalInvalidationTimestampMs(20));
  ASSERT_TRUE(src.Put("a", 50));
  purge_set_.Merge(src);
  EXPECT_FALSE(purge_set_.IsValid("a", 40));
  EXPECT_FALSE(purge_set_.IsValid("b", 40));
  EXPECT_FALSE(purge_set_.IsValid("c", 20));
  EXPECT_TRUE(purge_set_.IsValid("c", 40));
}

TEST_F(PurgeSetTest, MergeMaxWins) {
  ASSERT_TRUE(purge_set_.UpdateGlobalInvalidationTimestampMs(10));
  ASSERT_TRUE(purge_set_.Put("a", 40));
  ASSERT_TRUE(purge_set_.Put("b", 70));

  PurgeSet src(kMaxSize);
  ASSERT_TRUE(src.UpdateGlobalInvalidationTimestampMs(5));  // ignored on merge
  ASSERT_TRUE(src.Put("a", 50));
  ASSERT_TRUE(src.Put("b", 60));
  purge_set_.Merge(src);

  EXPECT_FALSE(purge_set_.IsValid("a", 45));
  EXPECT_TRUE(purge_set_.IsValid("a", 55));

  EXPECT_FALSE(purge_set_.IsValid("b", 65));  // implement combining policy
  EXPECT_TRUE(purge_set_.IsValid("b", 75));

  EXPECT_FALSE(purge_set_.IsValid("c", 9));
  EXPECT_TRUE(purge_set_.IsValid("c", 11));
}

TEST_F(PurgeSetTest, SlightSkew) {
  ASSERT_TRUE(purge_set_.Put("a", 10));
  ASSERT_TRUE(purge_set_.UpdateGlobalInvalidationTimestampMs(8));  // clamped
  EXPECT_FALSE(purge_set_.IsValid("b", 9));
  EXPECT_FALSE(purge_set_.IsValid("b", 10));
  EXPECT_TRUE(purge_set_.IsValid("b", 11));
}

TEST_F(PurgeSetTest, TooMuchSkew) {
  ASSERT_TRUE(purge_set_.Put("a", PurgeSet::kClockSkewAllowanceMs + 100));
  EXPECT_FALSE(purge_set_.UpdateGlobalInvalidationTimestampMs(10));  // ignored
  EXPECT_TRUE(purge_set_.IsValid("b", 9));
  EXPECT_TRUE(purge_set_.IsValid("b", 10));
  EXPECT_TRUE(purge_set_.IsValid("b", 11));
}

}  // namespace net_instaweb
