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

#include "net/instaweb/util/public/mock_time_cache.h"

#include <cstddef>
#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/shared_string.h"

namespace net_instaweb {
namespace {

const size_t kMaxSize = 100;

// We start time from a non-zero value to make sure we don't confuse
// relative and absolute. The value itself is of no particular significance.
const int64 kStartTime = 3456;

class MockTimeCacheTest : public CacheTestBase {
 protected:
  MockTimeCacheTest()
      : timer_(kStartTime), cache_(&timer_, new LRUCache(kMaxSize)) {
  }

  virtual CacheInterface* Cache() { return &cache_; }

 protected:
  MockTimer timer_;
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
  EXPECT_FALSE(result.called_);

  // Move halfways to completion; should still have not been called.
  timer_.AdvanceUs(kDelayUs / 2);
  EXPECT_FALSE(result.called_);

  // Now after it expires, it should be OK.
  timer_.AdvanceUs(kDelayUs / 2 + 1);
  EXPECT_TRUE(result.called_);
  EXPECT_EQ(CacheInterface::kAvailable, result.state_);
  EXPECT_EQ("Value", *result.value()->get());

  // Do the same thing after deleting it.
  cache_.Delete("Name");
  cache_.Get("Name", result.Reset());
  EXPECT_FALSE(result.called_);
  timer_.AdvanceUs(kDelayUs / 2);
  EXPECT_FALSE(result.called_);
  timer_.AdvanceUs(kDelayUs / 2 + 1);
  EXPECT_TRUE(result.called_);
  EXPECT_EQ(CacheInterface::kNotFound, result.state_);
}

}  // namespace
}  // namespace net_instaweb
