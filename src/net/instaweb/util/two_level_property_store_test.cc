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

#include "net/instaweb/util/public/two_level_property_store.h"

#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/util/property_cache.pb.h"
#include "net/instaweb/util/public/abstract_property_store_get_callback.h"
#include "net/instaweb/util/public/cache_property_store.h"
#include "net/instaweb/util/public/delay_cache.h"
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

namespace net_instaweb {

namespace {

const size_t kMaxCacheSize = 100;
const char kCache1[] = "cache1";
const char kCache2[] = "cache2";
const char kCohortName1[] = "cohort1";
const char kCohortName2[] = "cohort2";
const char kPropName1[] = "prop1";
const char kValueName1[] = "value1";
const char kUrl[] = "www.test.com/sample.html";
const char kParsableContent[] =
    "value { name: 'prop1' value: 'value1' }";
const char kNonParsableContent[] = "random";
const char kOptionsSignatureHash[] = "hash";
const char kCacheKeySuffix[] = "CacheKeySuffix";

}  // namespace

class TwoLevelPropertyStoreTest : public testing::Test {
 public:
  TwoLevelPropertyStoreTest()
     : lru_cache_1_(kMaxCacheSize),
       lru_cache_2_(kMaxCacheSize),
       thread_system_(Platform::CreateThreadSystem()),
       delay_cache_1_(&lru_cache_1_, thread_system_.get()),
       delay_cache_2_(&lru_cache_2_, thread_system_.get()),
       timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms),
       cache_property_store_1_(
           kCache1, &delay_cache_1_, &timer_, &stats_, thread_system_.get()),
       cache_property_store_2_(
           kCache2, &delay_cache_2_, &timer_, &stats_, thread_system_.get()),
       two_level_property_store_(&cache_property_store_1_,
                                 &cache_property_store_2_,
                                 thread_system_.get()),
       num_callback_with_false_called_(0),
       num_callback_with_true_called_(0),
       property_cache_(&two_level_property_store_,
                       &timer_,
                       &stats_,
                       thread_system_.get()) {
    PropertyCache::InitCohortStats(kCohortName1, &stats_);
    PropertyStoreGetCallback::InitStats(&stats_);
    cohort_ = property_cache_.AddCohort(kCohortName1);
    cache_property_store_1_.AddCohort(kCohortName1);
    cache_property_store_2_.AddCohort(kCohortName1);
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
    lru_cache_1_.ClearStats();
    lru_cache_2_.ClearStats();
  }

  void PutHelper(PropertyStore* property_store,
                 const PropertyCache::Cohort* cohort) {
    PropertyCacheValues values;
    PropertyValueProtobuf* value_buf = values.add_value();
    value_buf->set_name(kPropName1);
    value_buf->set_body(kValueName1);
    property_store->Put(
        kUrl,
        kOptionsSignatureHash,
        kCacheKeySuffix,
        cohort,
        &values,
        NULL);
  }

  void ResultCallback(bool result) {
    if (result) {
      ++num_callback_with_true_called_;
    } else {
      ++num_callback_with_false_called_;
    }
  }

  AbstractPropertyStoreGetCallback* ExecuteGet(PropertyPage* page) {
    AbstractPropertyStoreGetCallback* callback = NULL;
    two_level_property_store_.Get(
        kUrl,
        kOptionsSignatureHash,
        kCacheKeySuffix,
        cohort_list_,
        page,
        NewCallback(this, &TwoLevelPropertyStoreTest::ResultCallback),
        &callback);
    callback->DeleteWhenDone();
    return callback;
  }

  void ExpectCacheStats(LRUCache* lru_cache,
                        int expected_num_cache_hits,
                        int expected_num_cache_misses,
                        int expected_num_cache_inserts,
                        const GoogleString& cache_string) {
    EXPECT_EQ(expected_num_cache_hits, lru_cache->num_hits()) << cache_string;
    EXPECT_EQ(expected_num_cache_misses,
              lru_cache->num_misses()) << cache_string;
    EXPECT_EQ(expected_num_cache_inserts,
              lru_cache->num_inserts()) << cache_string;
  }

  void DelayCacheLookup(DelayCache* cache,
                        CachePropertyStore* property_store) {
    GoogleString cache_key = property_store->CacheKey(
        kUrl, kOptionsSignatureHash, kCacheKeySuffix, cohort_);
    LOG(INFO) << "Delay Cache Key:: " << cache_key;
    cache->DelayKey(cache_key);
  }

  void ReleaseCacheLookup(DelayCache* cache,
                          CachePropertyStore* property_store) {
    GoogleString cache_key = property_store->CacheKey(
        kUrl, kOptionsSignatureHash, kCacheKeySuffix, cohort_);
    cache->ReleaseKey(cache_key);
  }

 protected:
  LRUCache lru_cache_1_;
  LRUCache lru_cache_2_;
  scoped_ptr<ThreadSystem> thread_system_;
  DelayCache delay_cache_1_;
  DelayCache delay_cache_2_;
  MockTimer timer_;
  SimpleStats stats_;
  CachePropertyStore cache_property_store_1_;
  CachePropertyStore cache_property_store_2_;
  TwoLevelPropertyStore two_level_property_store_;
  int num_callback_with_false_called_;
  int num_callback_with_true_called_;

  PropertyCache property_cache_;
  const PropertyCache::Cohort* cohort_;
  PropertyCache::CohortVector cohort_list_;
  scoped_ptr<MockPropertyPage> page_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoLevelPropertyStoreTest);
};

TEST_F(TwoLevelPropertyStoreTest, TestBothCacheMiss) {
  ExecuteGet(page_.get());
  EXPECT_EQ(CacheInterface::kNotFound, page_->GetCacheState(cohort_));
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   0,  /* Cache hit */
                   1,  /* Cache miss */
                   0  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   0,  /* Cache hit */
                   1,  /* Cache miss */
                   0  /* Cache inserts */,
                   kCache2);
}

TEST_F(TwoLevelPropertyStoreTest, TestPrimaryLevelCacheHit) {
  PutHelper(&cache_property_store_1_, cohort_);
  ExecuteGet(page_.get());
  EXPECT_EQ(CacheInterface::kAvailable, page_->GetCacheState(cohort_));
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   0,  /* Cache hit */
                   0,  /* Cache miss */
                   0  /* Cache inserts */,
                   kCache2);
}

TEST_F(TwoLevelPropertyStoreTest, TestSecondaryLevelCacheHit) {
  PutHelper(&cache_property_store_2_, cohort_);
  ExecuteGet(page_.get());
  EXPECT_EQ(CacheInterface::kAvailable, page_->GetCacheState(cohort_));
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   0,  /* Cache hit */
                   1,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache2);
}

TEST_F(TwoLevelPropertyStoreTest, TestOnlyPrimaryHitWhenPresentInBoth) {
  PutHelper(&two_level_property_store_, cohort_);
  ExecuteGet(page_.get());
  EXPECT_EQ(CacheInterface::kAvailable, page_->GetCacheState(cohort_));
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   0,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache2);
}

TEST_F(TwoLevelPropertyStoreTest, TestCancelBeforePrimaryLookupDone) {
  PutHelper(&cache_property_store_2_, cohort_);
  DelayCacheLookup(&delay_cache_1_, &cache_property_store_1_);
  AbstractPropertyStoreGetCallback* callback =
      ExecuteGet(page_.get());
  callback->FastFinishLookup();
  ReleaseCacheLookup(&delay_cache_1_, &cache_property_store_1_);
  EXPECT_EQ(CacheInterface::kNotFound, page_->GetCacheState(cohort_));
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   0,  /* Cache hit */
                   1,  /* Cache miss */
                   0  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   0,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache2);
}

TEST_F(TwoLevelPropertyStoreTest, TestCancelBeforeSecondaryLookupDone) {
  PutHelper(&cache_property_store_2_, cohort_);
  DelayCacheLookup(&delay_cache_2_, &cache_property_store_2_);
  AbstractPropertyStoreGetCallback* callback =
      ExecuteGet(page_.get());
  callback->FastFinishLookup();
  ReleaseCacheLookup(&delay_cache_2_, &cache_property_store_2_);
  EXPECT_EQ(CacheInterface::kNotFound, page_->GetCacheState(cohort_));
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   0,  /* Cache hit */
                   1,  /* Cache miss */
                   0  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache2);
}

TEST_F(TwoLevelPropertyStoreTest, TestCancelAfterSecondaryLookupDone) {
  PutHelper(&cache_property_store_2_, cohort_);
  AbstractPropertyStoreGetCallback* callback = NULL;
  two_level_property_store_.Get(
      kUrl,
      kOptionsSignatureHash,
      kCacheKeySuffix,
      cohort_list_,
      page_.get(),
      NewCallback(this, &TwoLevelPropertyStoreTest::ResultCallback),
      &callback);
  callback->FastFinishLookup();
  callback->DeleteWhenDone();
  EXPECT_EQ(CacheInterface::kAvailable, page_->GetCacheState(cohort_));
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   0,  /* Cache hit */
                   1,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache2);
}

TEST_F(TwoLevelPropertyStoreTest, TestDeleteWhenDoneBeforeSecondaryLookupDone) {
  PutHelper(&cache_property_store_2_, cohort_);
  DelayCacheLookup(&delay_cache_2_, &cache_property_store_2_);
  AbstractPropertyStoreGetCallback* callback = NULL;
  two_level_property_store_.Get(
      kUrl,
      kOptionsSignatureHash,
      kCacheKeySuffix,
      cohort_list_,
      page_.get(),
      NewCallback(this, &TwoLevelPropertyStoreTest::ResultCallback),
      &callback);
  callback->FastFinishLookup();
  callback->DeleteWhenDone();
  ReleaseCacheLookup(&delay_cache_2_, &cache_property_store_2_);
  EXPECT_EQ(CacheInterface::kNotFound, page_->GetCacheState(cohort_));
  EXPECT_EQ(1, num_callback_with_false_called_);
  EXPECT_EQ(0, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   0,  /* Cache hit */
                   1,  /* Cache miss */
                   0  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache2);
}

TEST_F(TwoLevelPropertyStoreTest, TestPartialSecondaryLookup) {
  PropertyCache::InitCohortStats(kCohortName2, &stats_);
  const PropertyCache::Cohort* cohort2 =
      property_cache_.AddCohort(kCohortName2);
  cache_property_store_1_.AddCohort(kCohortName2);
  cache_property_store_2_.AddCohort(kCohortName2);
  MockPropertyPage page(thread_system_.get(),
                        &property_cache_,
                        kUrl,
                        kOptionsSignatureHash,
                        kCacheKeySuffix);
  property_cache_.Read(&page);
  cohort_list_.push_back(cohort2);
  lru_cache_1_.ClearStats();
  lru_cache_2_.ClearStats();
  PutHelper(&two_level_property_store_, cohort_);
  PutHelper(&cache_property_store_2_, cohort2);
  ExecuteGet(&page);
  EXPECT_EQ(CacheInterface::kAvailable, page.GetCacheState(cohort_));
  EXPECT_EQ(CacheInterface::kAvailable, page.GetCacheState(cohort2));
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   1,  /* Cache hit */
                   1,  /* Cache miss */
                   2  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   2  /* Cache inserts */,
                   kCache2);
}

TEST_F(TwoLevelPropertyStoreTest, TestInsertValueIntoPrimaryFromSecondary) {
  PutHelper(&cache_property_store_2_, cohort_);
  ExecuteGet(page_.get());
  EXPECT_EQ(CacheInterface::kAvailable, page_->GetCacheState(cohort_));
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(1, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   0,  /* Cache hit */
                   1,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */,
                   kCache2);

  PropertyValue* pv = page_->GetProperty(cohort_, kPropName1);
  EXPECT_TRUE(pv->has_value());
  EXPECT_EQ(kValueName1, pv->value());

  lru_cache_1_.ClearStats();
  lru_cache_2_.ClearStats();
  ExecuteGet(page_.get());
  EXPECT_EQ(CacheInterface::kAvailable, page_->GetCacheState(cohort_));
  EXPECT_EQ(0, num_callback_with_false_called_);
  EXPECT_EQ(2, num_callback_with_true_called_);
  ExpectCacheStats(&lru_cache_1_,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   0  /* Cache inserts */,
                   kCache1);
  ExpectCacheStats(&lru_cache_2_,
                   0,  /* Cache hit */
                   0,  /* Cache miss */
                   0  /* Cache inserts */,
                   kCache2);
  pv = page_->GetProperty(cohort_, kPropName1);
  EXPECT_TRUE(pv->has_value());
  EXPECT_EQ(kValueName1, pv->value());
}

}  // namespace net_instaweb
