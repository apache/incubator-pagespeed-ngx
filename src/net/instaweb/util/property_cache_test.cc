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

// Unit-test the property cache

#include "net/instaweb/util/public/property_cache.h"

#include <cstddef>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

namespace {

const size_t kMaxCacheSize = 100;
const char kCohortName1[] = "cohort1";
const char kCohortName2[] = "cohort2";
const char kCacheKey1[] = "Key1";
const char kPropertyName1[] = "prop1";
const char kPropertyName2[] = "prop2";

class PropertyCacheTest : public testing::Test {
 protected:
  PropertyCacheTest()
      : lru_cache_(kMaxCacheSize),
        timer_(MockTimer::kApr_5_2010_ms),
        thread_system_(ThreadSystem::CreateThreadSystem()),
        property_cache_(&lru_cache_, &timer_, thread_system_.get()) {
    cohort_ = property_cache_.AddCohort(kCohortName1);
  }

  class MockPage : public PropertyPage {
   public:
    explicit MockPage(AbstractMutex* mutex)
        : PropertyPage(mutex),
          called_(false),
          valid_(false) {}
    virtual ~MockPage() {}
    virtual void Done(bool valid) {
      called_ = true;
      valid_ = valid;
    }
    bool called() const { return called_; }
    bool valid() const { return valid_; }

   private:
    bool called_;
    bool valid_;

    DISALLOW_COPY_AND_ASSIGN(MockPage);
  };

  // Performs a Read/Modify/Write transaction intended for a cold
  // cache, verifying that this worked.
  //
  // Returns whether the value is considered Stable or not.  In general
  // we would expect this routine to return false.
  bool ReadWriteInitial(const GoogleString& key, const GoogleString& value) {
    MockPage page(thread_system_->NewMutex());
    property_cache_.Read(kCacheKey1, &page);
    PropertyValue* property = page.GetProperty(cohort_, kPropertyName1);
    EXPECT_FALSE(page.valid());
    EXPECT_TRUE(page.called());
    property_cache_.UpdateValue(value, property);
    property_cache_.WriteCohort(kCacheKey1, cohort_, &page);
    EXPECT_TRUE(property->has_value());
    return property_cache_.IsStable(property);
  }

  // Performs a Read/Modify/Write transaction intended for a warm
  // cache, verifying that this worked, and that the old-value was
  // previously found.  Returns whether the value was considered
  // stable.
  bool ReadWriteTestStable(const GoogleString& key,
                           const GoogleString& old_value,
                           const GoogleString& new_value) {
    MockPage page(thread_system_->NewMutex());
    property_cache_.Read(kCacheKey1, &page);
    PropertyValue* property = page.GetProperty(cohort_, kPropertyName1);
    EXPECT_TRUE(page.valid());
    EXPECT_TRUE(page.called());
    EXPECT_STREQ(old_value, property->value());
    property_cache_.UpdateValue(new_value, property);
    property_cache_.WriteCohort(kCacheKey1, cohort_, &page);
    return property_cache_.IsStable(property);
  }

  LRUCache lru_cache_;
  MockTimer timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  PropertyCache property_cache_;
  const PropertyCache::Cohort* cohort_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PropertyCacheTest);
};

TEST_F(PropertyCacheTest, TrackStability) {
  // Tests that the current stability heuristics work as expected.  Note
  // that I don't think the heuristic is really great yet.  It needs some
  // iteration.  The 0.3 threshold comes from
  //     const int kDefaultMutationsPer1000WritesThreshold = 300;
  // in property_cache.cc.
  EXPECT_FALSE(ReadWriteInitial(kCacheKey1, "Value1")) << "1/1 > .300";
  EXPECT_FALSE(ReadWriteTestStable(kCacheKey1, "Value1", "Value1"))
      << "1/2 > .300";
  EXPECT_FALSE(ReadWriteTestStable(kCacheKey1, "Value1", "Value1"))
      << "1/3 > .300";
  EXPECT_TRUE(ReadWriteTestStable(kCacheKey1, "Value1", "Value1"))
      << "1/4 < .300";
  EXPECT_TRUE(ReadWriteTestStable(kCacheKey1, "Value1", "Value1"))
      << "1/5 < .300";
  EXPECT_FALSE(ReadWriteTestStable(kCacheKey1, "Value1", "Value2"))
      << "2/6 > .300";
  EXPECT_TRUE(ReadWriteTestStable(kCacheKey1, "Value2", "Value2"))
      << "2/7 < .300";
  EXPECT_TRUE(ReadWriteTestStable(kCacheKey1, "Value2", "Value2"))
      << "2/8 < .300";

  // Saturate the update-count by looping 62 more times, making 64 straight
  // writes where we did not change the value.
  for (int i = 0; i < 62; ++i) {
    EXPECT_TRUE(ReadWriteTestStable(kCacheKey1, "Value2", "Value2"))
        << "2/8 < .300";
  }

  // Now, to get to less than 300/1000 we'll have to change values 20
  // times.  On the first 19 we'll consider the system stable, but on
  // the 20th, the system figures out this value looks stable enough.
  //
  // TODO(jmarantz): This feels like maybe it's not a good metric, and
  // we should give up sooner once we see the instability.  But at
  // least for now this tests the system is working as expected.
  GoogleString prev_value = "Value2";
  for (int i = 0; i < 19; ++i) {
    GoogleString new_value = StringPrintf("Value%d", i + 3);
    EXPECT_TRUE(ReadWriteTestStable(kCacheKey1, prev_value, new_value)) <<
        " still stable after " << i << " mutations";
    prev_value = new_value;
  }
  EXPECT_FALSE(ReadWriteTestStable(kCacheKey1, prev_value, "Final"))
      << " finally unstable";

  // Now that we have 20 mutations in the system, it will take 64-20=44
  // repeats to flush them out to get back to 19 instabilities.
  for (int i = 0; i < 44; ++i) {
    EXPECT_FALSE(ReadWriteTestStable(kCacheKey1, "Final", "Final"))
        << "still unstable after " << i << " mutations";
  }
  EXPECT_TRUE(ReadWriteTestStable(kCacheKey1, "Final", "Final"))
      << " stable again";
}

TEST_F(PropertyCacheTest, DropOldWrites) {
  timer_.SetTimeMs(MockTimer::kApr_5_2010_ms);
  ReadWriteInitial(kCacheKey1, "Value1");
  ReadWriteTestStable(kCacheKey1, "Value1", "Value1");

  // Now imagine we are on a second server, which is trying to write
  // an older value into the same physical cache.  Make sure we don't let it.
  MockTimer timer2(MockTimer::kApr_5_2010_ms - 100);
  PropertyCache property_cache2(&lru_cache_, &timer2, thread_system_.get());
  property_cache2.AddCohort(kCohortName1);
  const PropertyCache::Cohort* cohort2 = property_cache2.GetCohort(
      kCohortName1);
  {
    MockPage page(thread_system_->NewMutex());
    property_cache2.Read(kCacheKey1, &page);
    EXPECT_TRUE(page.valid());
    EXPECT_TRUE(page.called());
    PropertyValue* property = page.GetProperty(cohort2, kPropertyName1);
    property_cache2.UpdateValue("Value2", property);
    // Stale value dropped.
    property_cache2.WriteCohort(kCacheKey1, cohort2, &page);
  }
  {
    MockPage page(thread_system_->NewMutex());
    property_cache2.Read(kCacheKey1, &page);
    EXPECT_TRUE(page.valid());
    EXPECT_TRUE(page.called());
    PropertyValue* property = page.GetProperty(cohort2, kPropertyName1);
    EXPECT_STREQ("Value1", property->value());  // Value2 was dropped.
  }
}

TEST_F(PropertyCacheTest, EmptyReadNewPropertyWasRead) {
  MockPage page(thread_system_->NewMutex());
  property_cache_.Read(kCacheKey1, &page);
  PropertyValue* property = page.GetProperty(cohort_, kPropertyName1);
  EXPECT_TRUE(property->was_read());
  EXPECT_FALSE(property->has_value());
}

TEST_F(PropertyCacheTest, TwoCohorts) {
  EXPECT_EQ(cohort_, property_cache_.GetCohort(kCohortName1));
  EXPECT_EQ(cohort_, property_cache_.AddCohort(kCohortName1));
  EXPECT_TRUE(property_cache_.GetCohort(kCohortName2) == NULL);
  const PropertyCache::Cohort* cohort2 = property_cache_.AddCohort(
      kCohortName2);
  ReadWriteInitial(kCacheKey1, "Value1");
  EXPECT_EQ(2, lru_cache_.num_misses()) << "one miss per cohort";
  EXPECT_EQ(1, lru_cache_.num_inserts()) << "only cohort1 written";
  lru_cache_.ClearStats();

  // ReadWriteInitial found something for cohort1 but no value has
  // yet been established for cohort2, so we'll get a hit and a miss.
  {
    MockPage page(thread_system_->NewMutex());
    property_cache_.Read(kCacheKey1, &page);
    EXPECT_EQ(1, lru_cache_.num_hits()) << "cohort1";
    EXPECT_EQ(1, lru_cache_.num_misses()) << "cohort2";
    PropertyValue* p2 = page.GetProperty(cohort2, kPropertyName2);
    EXPECT_TRUE(p2->was_read());
    EXPECT_FALSE(p2->has_value());
    property_cache_.UpdateValue("v2", p2);
    property_cache_.WriteCohort(kCacheKey1, cohort2, &page);
    EXPECT_EQ(1, lru_cache_.num_inserts()) << "cohort2 written";
  }

  lru_cache_.ClearStats();

  // Now a second read will get two hits, no misses, and both data elements
  // present.
  {
    MockPage page(thread_system_->NewMutex());
    property_cache_.Read(kCacheKey1, &page);
    EXPECT_EQ(2, lru_cache_.num_hits()) << "both cohorts hit";
    EXPECT_EQ(0, lru_cache_.num_misses());
    PropertyValue* p2 = page.GetProperty(cohort2, kPropertyName2);
    EXPECT_TRUE(p2->was_read());
    EXPECT_TRUE(p2->has_value());
  }
}

}  // namespace

}  // namespace net_instaweb
