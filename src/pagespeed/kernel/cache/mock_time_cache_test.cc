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

// Author: morlovich@google.com (Maksim Orlovich)

// Unit-test for MockTimeCache

#include "pagespeed/kernel/cache/mock_time_cache.h"

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/thread/mock_scheduler.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {
namespace {

const size_t kMaxSize = 100;

// We start time from a non-zero value to make sure we don't confuse
// relative and absolute. The value itself is of no particular significance.
const int64 kStartTime = 3456;

class MockTimeCacheTest : public CacheTestBase {
 protected:
  MockTimeCacheTest()
      : timer_(kStartTime),
        thread_system_(Platform::CreateThreadSystem()),
        scheduler_(thread_system_.get(), &timer_),
        lru_cache_(kMaxSize),
        cache_(&scheduler_, &lru_cache_) {
  }

  virtual CacheInterface* Cache() { return &cache_; }

  void AdvanceTimeUs(int64 interval_us) {
    scheduler_.AdvanceTimeUs(interval_us);
  }

 protected:
  MockTimer timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockScheduler scheduler_;
  LRUCache lru_cache_;
  MockTimeCache cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTimeCacheTest);
};

TEST_F(MockTimeCacheTest, NoDelayOps) {
  // Basic operation w/o any delay injected.
  CheckNotFound("Name");
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  cache_.Delete("Name");
  CheckNotFound("Name");
}

TEST_F(MockTimeCacheTest, DelayOps) {
  const int64 kDelayUs = 10000;
  cache_.set_delay_us(kDelayUs);

  // Load the value.
  CheckPut("Name", "Value");

  // Try getting...
  CacheTestBase::Callback result;
  cache_.Get("Name", &result);

  // Initially, should not have been called.
  EXPECT_FALSE(result.called());

  // Move halfways to completion; should still have not been called.
  AdvanceTimeUs(kDelayUs / 2);
  EXPECT_FALSE(result.called());

  // Now after it expires, it should be OK.
  AdvanceTimeUs(kDelayUs / 2 + 1);
  EXPECT_TRUE(result.called());
  EXPECT_EQ(CacheInterface::kAvailable, result.state());
  EXPECT_EQ("Value", result.value()->Value());

  // Do the same thing after deleting it.
  cache_.Delete("Name");
  cache_.Get("Name", result.Reset());
  EXPECT_FALSE(result.called());
  AdvanceTimeUs(kDelayUs / 2);
  EXPECT_FALSE(result.called());
  AdvanceTimeUs(kDelayUs / 2 + 1);
  EXPECT_TRUE(result.called());
  EXPECT_EQ(CacheInterface::kNotFound, result.state());
}

}  // namespace
}  // namespace net_instaweb
