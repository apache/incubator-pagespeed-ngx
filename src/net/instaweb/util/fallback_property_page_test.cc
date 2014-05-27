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

#include "net/instaweb/util/public/fallback_property_page.h"

#include <cstddef>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_property_store.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/property_store.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/thread_system.h"
#include "testing/base/public/gunit.h"

namespace net_instaweb {

const size_t kMaxCacheSize = 200;
const char kCohortName1[] = "cohort1";
const char kCacheKey1[] = "Key1";
const char kCacheKey2[] = "Key2";
const char kPropertyName1[] = "prop1";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kOptionsSignatureHash[] = "hash";
const char kCacheKeySuffix[] = "CacheKeySuffix";

class FallbackPropertyPageTest : public testing::Test {
 protected:
  FallbackPropertyPageTest()
      : lru_cache_(kMaxCacheSize),
        thread_system_(Platform::CreateThreadSystem()),
        timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms),
        stats_(thread_system_.get()),
        cache_property_store_(
            "test/", &lru_cache_, &timer_, &stats_, thread_system_.get()),
        property_cache_(&cache_property_store_,
                        &timer_,
                        &stats_,
                        thread_system_.get()) {
    PropertyCache::InitCohortStats(kCohortName1, &stats_);
    PropertyStoreGetCallback::InitStats(&stats_);
    cohort_ = property_cache_.AddCohort(kCohortName1);
    cache_property_store_.AddCohort(cohort_->name());
  }

  // Sets both actual and fallback property page.
  void SetupFallbackPage() {
    PropertyPage* actual_property_page = new MockPropertyPage(
        thread_system_.get(),
        &property_cache_,
        kCacheKey1,
        kOptionsSignatureHash,
        kCacheKeySuffix);
    PropertyPage* fallback_property_page = new MockPropertyPage(
        thread_system_.get(),
        &property_cache_,
        kCacheKey2,
        kOptionsSignatureHash,
        kCacheKeySuffix);
    fallback_page_.reset(new FallbackPropertyPage(
        actual_property_page, fallback_property_page));
    property_cache_.Read(actual_property_page);
    property_cache_.Read(fallback_property_page);
  }

  void CheckNoValuePreset() {
    PropertyValue* property =
        fallback_page_->GetProperty(cohort_, kPropertyName1);
    EXPECT_FALSE(property->has_value());
    property = fallback_page_->actual_property_page()->GetProperty(
        cohort_, kPropertyName1);
    EXPECT_FALSE(property->has_value());
    property =
        fallback_page_->property_page_with_fallback_values()->GetProperty(
            cohort_, kPropertyName1);
    EXPECT_FALSE(property->has_value());
  }

  void CheckValueIsPresent(const GoogleString& value) {
    // Property present in page.
    PropertyValue* property =
        fallback_page_->GetProperty(cohort_, kPropertyName1);
    EXPECT_TRUE(property->has_value());
    EXPECT_EQ(value, property->value());

    // Property present in actual page.
    property = fallback_page_->actual_property_page()->GetProperty(
        cohort_, kPropertyName1);
    EXPECT_TRUE(property->has_value());
    EXPECT_EQ(value, property->value());

    // Property present in fallback page.
    property =
        fallback_page_->property_page_with_fallback_values()->GetProperty(
            cohort_, kPropertyName1);
    EXPECT_TRUE(property->has_value());
    EXPECT_EQ(value, property->value());
  }

  scoped_ptr<FallbackPropertyPage> fallback_page_;
  LRUCache lru_cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;
  SimpleStats stats_;
  CachePropertyStore cache_property_store_;
  PropertyCache property_cache_;
  const PropertyCache::Cohort* cohort_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FallbackPropertyPageTest);
};

TEST_F(FallbackPropertyPageTest, TestIfNoFallbackPageSet) {
  PropertyPage* actual_property_page = new MockPropertyPage(
      thread_system_.get(),
      &property_cache_,
      kCacheKey1,
      kOptionsSignatureHash,
      kCacheKeySuffix);
  fallback_page_.reset(new FallbackPropertyPage(actual_property_page, NULL));
  property_cache_.Read(actual_property_page);
  fallback_page_->UpdateValue(cohort_, kPropertyName1, kValue1);
  fallback_page_->WriteCohort(cohort_);
  PropertyValue* property =
      fallback_page_->GetProperty(cohort_, kPropertyName1);
  EXPECT_TRUE(property->has_value());
  EXPECT_EQ(kValue1, property->value());
}

TEST_F(FallbackPropertyPageTest, TestUpdatePropertyIfFallbackPageIsSet) {
  SetupFallbackPage();
  // Intially, No value present.
  CheckNoValuePreset();
  // Update property in both pages.
  fallback_page_->UpdateValue(cohort_, kPropertyName1, kValue1);
  fallback_page_->WriteCohort(cohort_);

  CheckValueIsPresent(kValue1);
}

TEST_F(FallbackPropertyPageTest, TestDeletePropertyIfFallbackPageIsSet) {
  SetupFallbackPage();
  // Intially, no value present.
  CheckNoValuePreset();

  // Update property in both pages.
  fallback_page_->UpdateValue(cohort_, kPropertyName1, kValue1);
  fallback_page_->WriteCohort(cohort_);

  // Delete will delete from both the pages.
  fallback_page_->DeleteProperty(cohort_, kPropertyName1);

  // No value present.
  CheckNoValuePreset();
}

TEST_F(FallbackPropertyPageTest, TestGetPropertyIfFallbackPageIsSet) {
  SetupFallbackPage();
  // Intially, No value present.
  CheckNoValuePreset();

  // Update property in only fallback property page.
  fallback_page_->property_page_with_fallback_values()->UpdateValue(
      cohort_, kPropertyName1, kValue1);
  fallback_page_->WriteCohort(cohort_);

  // No property present in actual page.
  PropertyValue* property = fallback_page_->actual_property_page()->GetProperty(
      cohort_, kPropertyName1);
  EXPECT_FALSE(property->has_value());

  // GetProperty will return value from fallback page.
  property = fallback_page_->GetProperty(cohort_, kPropertyName1);
  EXPECT_TRUE(property->has_value());
  EXPECT_EQ(kValue1, property->value());

  // Update actual page with different value.
  fallback_page_->actual_property_page()->UpdateValue(
      cohort_, kPropertyName1, kValue2);
  // GetProperty will return value from actual page.
  property = fallback_page_->GetProperty(cohort_, kPropertyName1);
  EXPECT_TRUE(property->has_value());
  EXPECT_EQ(kValue2, property->value());
}

TEST_F(FallbackPropertyPageTest, TestWriteCohortIfFallbackPageIsSet) {
  SetupFallbackPage();
  // Intially, no value present.
  CheckNoValuePreset();

  // Update property in both pages.
  fallback_page_->UpdateValue(cohort_, kPropertyName1, kValue1);
  fallback_page_->WriteCohort(cohort_);

  // Re-read the values from cache.
  SetupFallbackPage();

  CheckValueIsPresent(kValue1);
}

TEST_F(FallbackPropertyPageTest, TestGetFallbackPageUrl) {
  GoogleString fallback_path("http://www.abc.com/b/");
  GoogleString device_type_suffix("0");
  GoogleUrl url_query(StrCat(fallback_path, "?c=d"));
  GoogleUrl url_base_path(StrCat(fallback_path, "c/"));

  EXPECT_EQ(StrCat(fallback_path, "@fallback"),
            FallbackPropertyPage::GetFallbackPageUrl(url_query));
  EXPECT_EQ(StrCat(fallback_path, "#fallback"),
            FallbackPropertyPage::GetFallbackPageUrl(url_base_path));
}

TEST_F(FallbackPropertyPageTest, TestIsFallbackUrl) {
  EXPECT_FALSE(FallbackPropertyPage::IsFallbackUrl("http://www.abc.com/b/"));
  EXPECT_TRUE(
      FallbackPropertyPage::IsFallbackUrl("http://www.abc.com/b/@fallback"));
  EXPECT_TRUE(
      FallbackPropertyPage::IsFallbackUrl("http://www.abc.com/b/#fallback"));
}

}  // namespace net_instaweb
