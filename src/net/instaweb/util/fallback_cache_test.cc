/*
 * Copyright 2012 Google Inc.
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

#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/fallback_cache.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const int kTestValueSizeThreshold = 200;
const int kFallbackCacheSize = 3 * kTestValueSizeThreshold;
const int kMediumValueSize = kTestValueSizeThreshold - 100;
const int kLargeWriteSize = kTestValueSizeThreshold + 1;
const int kHugeWriteSize = 2 * kTestValueSizeThreshold;
const char kLargeKey1[] = "large1";
const char kLargeKey2[] = "large2";

}  // namespace

class FallbackCacheTest : public CacheTestBase {
 protected:
  FallbackCacheTest()
      : small_cache_(kFallbackCacheSize),
        large_cache_(kFallbackCacheSize),
        fallback_cache_(&small_cache_, &large_cache_,
                        kTestValueSizeThreshold, &handler_) {
  }

  virtual CacheInterface* Cache() { return &fallback_cache_; }

  GoogleMessageHandler handler_;
  LRUCache small_cache_;
  LRUCache large_cache_;
  FallbackCache fallback_cache_;
};

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(FallbackCacheTest, PutGetDelete) {
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  CheckNotFound("Another Name");

  CheckPut("Name", "NewValue");
  CheckGet("Name", "NewValue");

  EXPECT_LT(0, small_cache_.size_bytes()) << "small cache used.";
  EXPECT_EQ(0, large_cache_.size_bytes()) << "large cache not used.";

  Cache()->Delete("Name");
  CheckNotFound("Name");

  EXPECT_EQ(0, small_cache_.size_bytes());
  EXPECT_EQ(0, large_cache_.size_bytes());
}

TEST_F(FallbackCacheTest, MultiGet) {
  TestMultiGet();
  EXPECT_EQ(0, large_cache_.size_bytes()) << "fallback not used.";
}

TEST_F(FallbackCacheTest, BasicInvalid) {
  // Check that we honor callback veto on validity.
  CheckPut("nameA", "valueA");
  CheckPut("nameB", "valueB");
  CheckGet("nameA", "valueA");
  CheckGet("nameB", "valueB");
  set_invalid_value("valueA");
  CheckNotFound("nameA");
  CheckGet("nameB", "valueB");
  EXPECT_EQ(0, large_cache_.size_bytes()) << "fallback not used.";
}

TEST_F(FallbackCacheTest, LargeInvalid) {
  const GoogleString kValueA(kLargeWriteSize, 'a');
  const GoogleString kValueB(kLargeWriteSize, 'b');

  // Check that we honor callback veto on validity.
  CheckPut("nameA", kValueA);
  CheckPut("nameB", kValueB);
  CheckGet("nameA", kValueA);
  CheckGet("nameB", kValueB);
  set_invalid_value(kValueA.c_str());
  CheckNotFound("nameA");
  CheckGet("nameB", kValueB);
  EXPECT_LT(0, large_cache_.size_bytes()) << "fallback was used.";
}

TEST_F(FallbackCacheTest, SizeTest) {
  for (int x = 0; x < 10; ++x) {
    for (int i = kMediumValueSize / 2; i < kMediumValueSize; ++i) {
      GoogleString value(i, 'a');
      GoogleString key = StrCat("big", IntegerToString(i));
      CheckPut(key, value);
      CheckGet(key, value);
    }
  }
  EXPECT_EQ(0, large_cache_.size_bytes()) << "fallback not used.";
}

TEST_F(FallbackCacheTest, JustUnderThreshold) {
  const GoogleString kValue(kMediumValueSize, 'a');
  const char kKey[] = "just_under_threshold";
  CheckPut(kKey, kValue);
  CheckGet(kKey, kValue);
  EXPECT_EQ(0, large_cache_.size_bytes()) << "fallback not used.";
}

// Basic operation with huge values, only one of which will fit
// in the fallback cache at a time.
TEST_F(FallbackCacheTest, HugeValue) {
  const GoogleString kValue(kHugeWriteSize, 'a');
  CheckPut(kLargeKey1, kValue);
  CheckGet(kLargeKey1, kValue);
  EXPECT_LE(kHugeWriteSize, large_cache_.size_bytes());

  // Now put in another large value, causing the 1st to get evicted from
  // the large cache.
  CheckPut(kLargeKey2, kValue);
  CheckGet(kLargeKey2, kValue);
  CheckNotFound(kLargeKey1);

  // Finally, delete the second value explicitly.
  CheckGet(kLargeKey2, kValue);
  Cache()->Delete(kLargeKey2);
  CheckNotFound(kLargeKey2);
}

TEST_F(FallbackCacheTest, LargeValueMultiGet) {
  const GoogleString kLargeValue1(kLargeWriteSize, 'a');
  CheckPut(kLargeKey1, kLargeValue1);
  CheckGet(kLargeKey1, kLargeValue1);
  EXPECT_EQ(kLargeWriteSize + STATIC_STRLEN(kLargeKey1),
            large_cache_.size_bytes());

  const char kSmallKey[] = "small";
  const char kSmallValue[] = "value";
  CheckPut(kSmallKey, kSmallValue);

  const GoogleString kLargeValue2(kLargeWriteSize, 'b');
  CheckPut(kLargeKey2, kLargeValue2);
  CheckGet(kLargeKey2, kLargeValue2);
  EXPECT_LE(2 * kLargeWriteSize, large_cache_.size_bytes())
      << "Checks that both large values were written to the fallback cache";

  Callback* large1 = AddCallback();
  Callback* small = AddCallback();
  Callback* large2 = AddCallback();
  IssueMultiGet(large1, kLargeKey1, small, kSmallKey, large2, kLargeKey2);
  WaitAndCheck(large1, kLargeValue1);
  WaitAndCheck(small, "value");
  WaitAndCheck(large2, kLargeValue2);
}

TEST_F(FallbackCacheTest, MultiLargeSharingSmall) {
  // Make another connection to the same small_cache_, but with a different
  // large_ cache.
  LRUCache large_cache2(kFallbackCacheSize);
  FallbackCache fallback_cache2(&small_cache_, &large_cache2,
                                kTestValueSizeThreshold, &handler_);

  // Now when we store a large object from server1, and fetch it from
  // server2, we will get a miss because they do not share fallback caches..
  // But then we can re-store it and fetch it from either server.
  const GoogleString kLargeValue(kLargeWriteSize, 'a');
  CheckPut(kLargeKey1, kLargeValue);
  CheckGet(kLargeKey1, kLargeValue);

  // The large caches are not shared, so we get a miss from fallback_cache2.
  CheckNotFound(&fallback_cache2, kLargeKey1);

  CheckPut(&fallback_cache2, kLargeKey1, kLargeValue);
  CheckGet(&fallback_cache2, kLargeKey1, kLargeValue);
  CheckGet(Cache(), kLargeKey1, kLargeValue);
}

TEST_F(FallbackCacheTest, LargeKeyUnderThreshold) {
  const GoogleString kKey(kMediumValueSize, 'a');
  const char kValue[] = "value";
  CheckPut(kKey, kValue);
  CheckGet(kKey, kValue);
  EXPECT_EQ(0, large_cache_.size_bytes());
}

// Even keys that are over the *value* threshold can be stored in and
// retrieved from the fallback cache.
//
// Note: we do not expect to see ridiculously large keys; we are just
// testing for corner cases here.
TEST_F(FallbackCacheTest, LargeKeyOverThreshold) {
  const GoogleString kKey(kLargeWriteSize, 'a');
  const char kValue[] = "value";
  CheckPut(kKey, kValue);
  CheckGet(kKey, kValue);
  EXPECT_EQ(kKey.size() + STATIC_STRLEN(kValue),
            large_cache_.size_bytes());
}

// Tests what happens when we read an empty value, lacking the trailing L
// or S, from the small cache.
TEST_F(FallbackCacheTest, EmptyValue) {
  CheckPut(&small_cache_, "key", "");
  CheckNotFound("key");
}

// Tests what happens when we read a non-empty value, lacking the trailing L
// or S, from the small cache.
TEST_F(FallbackCacheTest, CorruptValue) {
  CheckPut(&small_cache_, "key", "garbage");
  CheckNotFound("key");
}

// If the last-character is 'L', it should be a one-character value.
TEST_F(FallbackCacheTest, CorruptValueLastCharL) {
  CheckPut(&small_cache_, "key", "xL");
  CheckNotFound("key");
}

}  // namespace net_instaweb
