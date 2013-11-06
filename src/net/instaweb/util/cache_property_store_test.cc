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

// Author: pulkitg@google.com (Pulkit Goyal)

// Unit test for CachePropertyStore.

#include "net/instaweb/util/public/cache_property_store.h"

#include <cstddef>

#include "net/instaweb/util/property_cache.pb.h"
#include "net/instaweb/util/public/abstract_property_store_get_callback.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"

namespace net_instaweb {

namespace {

const size_t kMaxCacheSize = 200;
const char kCohortName1[] = "cohort1";
const char kCohortName2[] = "cohort2";
const char kUrl[] = "www.test.com/sample.html";
const char kParsableContent[] =
    "value { name: 'prop1' value: 'value1' }";
const char kNonParsableContent[] = "random";
const char kOptionsSignatureHash[] = "hash";
const char kCacheKeySuffix[] = "CacheKeySuffix";

}  // namespace

class CachePropertyStoreTest : public testing::Test {
 public:
  CachePropertyStoreTest()
     : lru_cache_(kMaxCacheSize),
       thread_system_(Platform::CreateThreadSystem()),
       timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms),
       cache_property_store_(
           "test/", &lru_cache_, &timer_, &stats_, thread_system_.get()),
       property_cache_(&cache_property_store_,
                       &timer_,
                       &stats_,
                       thread_system_.get()),
       num_callback_with_false_called_(0),
       num_callback_with_true_called_(0),
       cache_lookup_status_(false) {
    PropertyCache::InitCohortStats(kCohortName1, &stats_);
    PropertyStoreGetCallback::InitStats(&stats_);
    cohort_ = property_cache_.AddCohort(kCohortName1);
    cache_property_store_.AddCohort(kCohortName1);
    cohort_list_.push_back(cohort_);
  }

  void SetUp() {
    page_.reset(
        new MockPropertyPage(
            thread_system_.get(),
            &property_cache_,
            kUrl,
            kOptionsSignatureHash,
            kCacheKeySuffix));
    property_cache_.Read(page_.get());
  }

  void ResultCallback(bool result) {
    cache_lookup_status_ = result;
    if (result) {
      ++num_callback_with_true_called_;
    } else {
      ++num_callback_with_false_called_;
    }
  }

  bool ExecuteGet(PropertyPage* page) {
    AbstractPropertyStoreGetCallback* callback = NULL;
    cache_property_store_.Get(
        kUrl,
        kOptionsSignatureHash,
        kCacheKeySuffix,
        cohort_list_,
        page,
        NewCallback(this, &CachePropertyStoreTest::ResultCallback),
        &callback);
    callback->DeleteWhenDone();
    return cache_lookup_status_;
  }

 protected:
  LRUCache lru_cache_;
  SimpleStats stats_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;
  CachePropertyStore cache_property_store_;
  PropertyCache property_cache_;
  const PropertyCache::Cohort* cohort_;
  PropertyCache::CohortVector cohort_list_;
  scoped_ptr<MockPropertyPage> page_;
  int num_callback_with_false_called_;
  int num_callback_with_true_called_;
  bool cache_lookup_status_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CachePropertyStoreTest);
};

TEST_F(CachePropertyStoreTest, TestNoResultAvailable) {
  EXPECT_FALSE(ExecuteGet(page_.get()));
  EXPECT_EQ(CacheInterface::kNotFound, page_->GetCacheState(cohort_));
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
}

TEST_F(CachePropertyStoreTest, TestResultAvailable) {
  PropertyCacheValues values;
  values.ParseFromString(kParsableContent);
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      kCacheKeySuffix,
      cohort_,
      &values,
      NULL);
  EXPECT_TRUE(ExecuteGet(page_.get()));
  EXPECT_EQ(CacheInterface::kAvailable, page_->GetCacheState(cohort_));
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
}

TEST_F(CachePropertyStoreTest, TestResultAvailableButNonParsable) {
  SharedString put_buffer(kNonParsableContent);
  lru_cache_.Put(cache_property_store_.CacheKey(kUrl,
                                                kOptionsSignatureHash,
                                                kCacheKeySuffix,
                                                cohort_),
                 &put_buffer);
  EXPECT_FALSE(ExecuteGet(page_.get()));
  EXPECT_EQ(CacheInterface::kAvailable, page_->GetCacheState(cohort_));
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
}

TEST_F(CachePropertyStoreTest, TestMultipleCohorts) {
  PropertyCache::InitCohortStats(kCohortName2, &stats_);
  const PropertyCache::Cohort* cohort2 =
      property_cache_.AddCohort(kCohortName2);
  cache_property_store_.AddCohort(kCohortName2);
  MockPropertyPage page(thread_system_.get(),
                        &property_cache_,
                        kUrl,
                        kOptionsSignatureHash,
                        kCacheKeySuffix);
  property_cache_.Read(&page);
  PropertyCacheValues values;
  values.ParseFromString(kParsableContent);
  cohort_list_.push_back(cohort2);
  lru_cache_.ClearStats();
  EXPECT_FALSE(ExecuteGet(&page));

  EXPECT_EQ(0, lru_cache_.num_hits());
  EXPECT_EQ(2, lru_cache_.num_misses());
  EXPECT_EQ(0, lru_cache_.num_inserts());

  lru_cache_.ClearStats();
  // Insert the value for cohort1.
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      kCacheKeySuffix,
      cohort_,
      &values,
      NULL);
  EXPECT_TRUE(ExecuteGet(&page));

  EXPECT_EQ(1, lru_cache_.num_hits());
  EXPECT_EQ(1, lru_cache_.num_misses());
  EXPECT_EQ(1, lru_cache_.num_inserts());

  lru_cache_.ClearStats();
  // Insert the value for cohort2.
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      kCacheKeySuffix,
      cohort2,
      &values,
      NULL);
  EXPECT_TRUE(ExecuteGet(&page));

  EXPECT_EQ(2, lru_cache_.num_hits());
  EXPECT_EQ(0, lru_cache_.num_misses());
  EXPECT_EQ(1, lru_cache_.num_inserts());

  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(2, num_callback_with_true_called_);
}

TEST_F(CachePropertyStoreTest, TestMultipleCacheBackends) {
  // Create a second cache implementation.
  LRUCache second_cache(kMaxCacheSize);
  PropertyCache::InitCohortStats(kCohortName2, &stats_);
  const PropertyCache::Cohort* cohort2 =
      property_cache_.AddCohort(kCohortName2);
  cache_property_store_.AddCohortWithCache(kCohortName2, &second_cache);
  MockPropertyPage page(
      thread_system_.get(),
      &property_cache_,
      kUrl,
      kOptionsSignatureHash,
      kCacheKeySuffix);
  property_cache_.Read(&page);
  PropertyCacheValues values;
  values.ParseFromString(kParsableContent);
  lru_cache_.ClearStats();
  second_cache.ClearStats();
  // Insert the value for cohort1.
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      kCacheKeySuffix,
      cohort_,
      &values,
      NULL);
  // Insert the value for cohort2.
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      kCacheKeySuffix,
      cohort2,
      &values,
      NULL);
  cohort_list_.push_back(cohort2);
  // Get the value for cohort1 and cohort2.
  EXPECT_TRUE(ExecuteGet(&page));
  EXPECT_EQ(CacheInterface::kAvailable, page.GetCacheState(cohort_));
  EXPECT_EQ(CacheInterface::kAvailable, page.GetCacheState(cohort2));

  EXPECT_EQ(1, lru_cache_.num_hits());
  EXPECT_EQ(0, lru_cache_.num_misses());
  EXPECT_EQ(1, lru_cache_.num_inserts());

  EXPECT_EQ(1, second_cache.num_hits());
  EXPECT_EQ(0, second_cache.num_misses());
  EXPECT_EQ(1, second_cache.num_inserts());

  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
}

TEST_F(CachePropertyStoreTest, TestPropertyCacheKeyMethod) {
  GoogleString cache_key = cache_property_store_.CacheKey(
      kUrl,
      kOptionsSignatureHash,
      kCacheKeySuffix,
      cohort_);
  GoogleString expected = StrCat(
      "test/",
      kUrl, "_",
      kOptionsSignatureHash,
      kCacheKeySuffix, "@",
      cohort_->name());
  EXPECT_EQ(expected, cache_key);
}

TEST_F(CachePropertyStoreTest, TestPutHandlesNonNullCallback) {
  PropertyCacheValues values;
  values.ParseFromString(kParsableContent);
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      kCacheKeySuffix,
      cohort_,
      &values,
      NewCallback(this, &CachePropertyStoreTest::ResultCallback));
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
}

}  // namespace net_instaweb
