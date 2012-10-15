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

// Unit-test the memcache interface.

#include "net/instaweb/apache/apr_mem_cache.h"

#include <cstddef>

#include "apr_pools.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/fallback_cache.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/null_statistics.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

const char kPortString[] = "6765";
const int kTestValueSizeThreshold = 200;
const size_t kLRUCacheSize = 3 * kTestValueSizeThreshold;
const size_t kJustUnderThreshold = kTestValueSizeThreshold - 100;
const size_t kLargeWriteSize = kTestValueSizeThreshold + 1;
const size_t kHugeWriteSize = 2 * kTestValueSizeThreshold;

}  // namespace

class AprMemCacheTest : public CacheTestBase {
 protected:
  AprMemCacheTest()
      : timer_(MockTimer::kApr_5_2010_ms),
        lru_cache_(new LRUCache(kLRUCacheSize)) {
    AprMemCache::InitStats(&statistics_);
  }

  static void SetUpTestCase() {
    apr_initialize();
    atexit(apr_terminate);
  }

  bool ConnectToMemcached(bool use_md5_hasher) {
    GoogleString servers = StrCat("localhost:", kPortString);
    Hasher* hasher = &mock_hasher_;
    if (use_md5_hasher) {
      hasher = &md5_hasher_;
    }
    servers_.reset(new AprMemCache(servers, 5, hasher, &statistics_, &timer_,
                                   &handler_));

    // apr_memcache actually lazy-connects to memcached, it seems, so
    // if we fail the Connect call then something is truly broken.  To
    // make sure memcached is actually up, we have to make an API
    // call, such as GetStatus.
    GoogleString buf;
    if (!servers_->Connect() || !servers_->GetStatus(&buf)) {
      LOG(ERROR) << "please start 'memcached -p 6765";
      return false;
    }

    cache_.reset(new FallbackCache(servers_.get(), lru_cache_.get(),
                                   kTestValueSizeThreshold,
                                   &handler_));
    return true;
  }
  virtual CacheInterface* Cache() { return cache_.get(); }

  GoogleMessageHandler handler_;
  MD5Hasher md5_hasher_;
  MockHasher mock_hasher_;
  SimpleStats statistics_;
  MockTimer timer_;
  scoped_ptr<LRUCache> lru_cache_;
  scoped_ptr<AprMemCache> servers_;
  scoped_ptr<FallbackCache> cache_;
};

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(AprMemCacheTest, PutGetDelete) {
  ASSERT_TRUE(ConnectToMemcached(true));

  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  CheckNotFound("Another Name");

  CheckPut("Name", "NewValue");
  CheckGet("Name", "NewValue");

  cache_->Delete("Name");
  CheckNotFound("Name");
  EXPECT_EQ(0, lru_cache_->size_bytes()) << "fallback not used.";
}

TEST_F(AprMemCacheTest, MultiGet) {
  ASSERT_TRUE(ConnectToMemcached(true));
  TestMultiGet();
  EXPECT_EQ(0, lru_cache_->size_bytes()) << "fallback not used.";
}

TEST_F(AprMemCacheTest, BasicInvalid) {
  ASSERT_TRUE(ConnectToMemcached(true));

  // Check that we honor callback veto on validity.
  CheckPut("nameA", "valueA");
  CheckPut("nameB", "valueB");
  CheckGet("nameA", "valueA");
  CheckGet("nameB", "valueB");
  set_invalid_value("valueA");
  CheckNotFound("nameA");
  CheckGet("nameB", "valueB");
  EXPECT_EQ(0, lru_cache_->size_bytes()) << "fallback not used.";
}

TEST_F(AprMemCacheTest, SizeTest) {
  ASSERT_TRUE(ConnectToMemcached(true));

  for (int x = 0; x < 10; ++x) {
    for (int i = kJustUnderThreshold/2; i < kJustUnderThreshold - 10; ++i) {
      GoogleString value(i, 'a');
      GoogleString key = StrCat("big", IntegerToString(i));
      CheckPut(key, value);
      CheckGet(key, value);
    }
  }
  EXPECT_EQ(0, lru_cache_->size_bytes()) << "fallback not used.";
}

TEST_F(AprMemCacheTest, StatsTest) {
  ASSERT_TRUE(ConnectToMemcached(true));
  GoogleString buf;
  ASSERT_TRUE(servers_->GetStatus(&buf));
  EXPECT_TRUE(buf.find("memcached server localhost:") != GoogleString::npos);
  EXPECT_TRUE(buf.find(" pid ") != GoogleString::npos);
  EXPECT_TRUE(buf.find("\nbytes_read: ") != GoogleString::npos);
  EXPECT_TRUE(buf.find("\ncurr_connections: ") != GoogleString::npos);
  EXPECT_TRUE(buf.find("\ntotal_items: ") != GoogleString::npos);
  EXPECT_EQ(0, lru_cache_->size_bytes()) << "fallback not used.";
}

TEST_F(AprMemCacheTest, HashCollision) {
  ASSERT_TRUE(ConnectToMemcached(false));
  CheckPut("N1", "V1");
  CheckGet("N1", "V1");

  // Since we are using a mock hasher, which always returns "0", the
  // put on "N2" will overwrite "N1" in memcached due to hash
  // collision.
  CheckPut("N2", "V2");
  CheckGet("N2", "V2");
  CheckNotFound("N1");
  EXPECT_EQ(0, lru_cache_->size_bytes()) << "fallback not used.";
}

TEST_F(AprMemCacheTest, JustUnderThreshold) {
  ASSERT_TRUE(ConnectToMemcached(true));
  const GoogleString kValue(kJustUnderThreshold, 'a');
  const char kKey[] = "just_under_threshold";
  CheckPut(kKey, kValue);
  CheckGet(kKey, kValue);
  EXPECT_EQ(0, lru_cache_->size_bytes()) << "fallback not used.";
}

// Basic operation with huge values, only one of which will fit
// in the fallback cache at a time.
TEST_F(AprMemCacheTest, HugeValue) {
  ASSERT_TRUE(ConnectToMemcached(true));
  const GoogleString kValue(kHugeWriteSize, 'a');
  const char kKey1[] = "large1";
  CheckPut(kKey1, kValue);
  CheckGet(kKey1, kValue);
  EXPECT_LE(kHugeWriteSize, lru_cache_->size_bytes());

  // Now put in another large value, causing the 1st to get evicted from
  // the fallback cache.
  const char kKey2[] = "large2";
  CheckPut(kKey2, kValue);
  CheckGet(kKey2, kValue);
  CheckNotFound(kKey1);

  // Finally, delete the second value explicitly.  Note that value will be
  // in the fallback cache, but we will not be able to get to it because
  // we've removed the sentinal from memcached.
  CheckGet(kKey2, kValue);
  cache_->Delete(kKey2);
  CheckNotFound(kKey2);
}

TEST_F(AprMemCacheTest, LargeValueMultiGet) {
  ASSERT_TRUE(ConnectToMemcached(true));
  const GoogleString kLargeValue1(kLargeWriteSize, 'a');
  const char kKey1[] = "large1";
  CheckPut(kKey1, kLargeValue1);
  CheckGet(kKey1, kLargeValue1);
  EXPECT_EQ(kLargeWriteSize + STATIC_STRLEN(kKey1),
            lru_cache_->size_bytes());

  const char kSmallKey[] = "small";
  const char kSmallValue[] = "value";
  CheckPut(kSmallKey, kSmallValue);

  const GoogleString kLargeValue2(kLargeWriteSize, 'b');
  const char kKey2[] = "large2";
  CheckPut(kKey2, kLargeValue2);
  CheckGet(kKey2, kLargeValue2);
  EXPECT_LE(2 * kLargeWriteSize, lru_cache_->size_bytes())
      << "Checks that both large values were written to the fallback cache";

  Callback* large1 = AddCallback();
  Callback* small = AddCallback();
  Callback* large2 = AddCallback();
  IssueMultiGet(large1, kKey1, small, kSmallKey, large2, kKey2);
  WaitAndCheck(large1, kLargeValue1);
  WaitAndCheck(small, "value");
  WaitAndCheck(large2, kLargeValue2);
}

TEST_F(AprMemCacheTest, MultiServerFallback) {
  ASSERT_TRUE(ConnectToMemcached(true));

  // Make another connection to the same memcached, but with a different
  // fallback cache.
  LRUCache lru_cache2(kLRUCacheSize);
  FallbackCache mem_cache2(servers_.get(), &lru_cache2,
                           kTestValueSizeThreshold,
                           &handler_);

  // Now when we store a large object from server1, and fetch it from
  // server2, we will get a miss because they do not share fallback caches..
  // But then we can re-store it and fetch it from either server.
  const GoogleString kLargeValue(kLargeWriteSize, 'a');
  const char kKey1[] = "large1";
  CheckPut(kKey1, kLargeValue);
  CheckGet(kKey1, kLargeValue);

  // The fallback caches are not shared, so we get a miss from mem_cache2.
  CheckNotFound(&mem_cache2, kKey1);

  CheckPut(&mem_cache2, kKey1, kLargeValue);
  CheckGet(&mem_cache2, kKey1, kLargeValue);
  CheckGet(cache_.get(), kKey1, kLargeValue);
}

TEST_F(AprMemCacheTest, KeyOver64kDropped) {
  ASSERT_TRUE(ConnectToMemcached(true));

  // We set our testing byte thresholds too low to trigger the case where
  // the key-value encoding fails, so make an alternate fallback cache
  // with a threshold over 64k.

  // Make another connection to the same memcached, but with a different
  // fallback cache.
  const int kBigLruSize = 1000000;
  const int kBigKeySize = 100000;    // >64k
  const int kThreshold =  200000;    // fits key and small value.
  LRUCache lru_cache2(kLRUCacheSize);
  FallbackCache mem_cache2(servers_.get(), &lru_cache2,
                           kThreshold, &handler_);

  const GoogleString kKey(kBigLruSize, 'a');
  CheckPut(&mem_cache2, kKey, "value");
  CheckNotFound(&mem_cache2, kKey.c_str());
}

// Even keys that are over the *value* threshold can be stored in and
// retrieved from the fallback cache.  This is because we don't even
// store the key in memcached.
//
// Note: we do not expect to see ridiculously large keys; we are just
// testing for corner cases here.
TEST_F(AprMemCacheTest, LargeKeyOverThreshold) {
  ASSERT_TRUE(ConnectToMemcached(true));
  const GoogleString kKey(kLargeWriteSize, 'a');
  const char kValue[] = "value";
  CheckPut(kKey, kValue);
  CheckGet(kKey, kValue);
  EXPECT_EQ(kKey.size() + STATIC_STRLEN(kValue), lru_cache_->size_bytes());
}

TEST_F(AprMemCacheTest, ThrottleLogMessages) {
  ASSERT_TRUE(ConnectToMemcached(true));
  EXPECT_TRUE(servers_->ShouldLogAprError());
  EXPECT_TRUE(servers_->ShouldLogAprError());
  EXPECT_TRUE(servers_->ShouldLogAprError());
  EXPECT_TRUE(servers_->ShouldLogAprError());
  EXPECT_FALSE(servers_->ShouldLogAprError());  // Too many.

  timer_.AdvanceMs(40 * Timer::kMinuteMs);
  EXPECT_TRUE(servers_->ShouldLogAprError());
  EXPECT_TRUE(servers_->ShouldLogAprError());
  EXPECT_TRUE(servers_->ShouldLogAprError());
  EXPECT_TRUE(servers_->ShouldLogAprError());
  EXPECT_FALSE(servers_->ShouldLogAprError());  // Too many.
}

}  // namespace net_instaweb
