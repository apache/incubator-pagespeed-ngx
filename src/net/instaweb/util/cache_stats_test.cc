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

// Unit-test the cache statistics wrapper.  Creates an LRU-cache first, and then
// wraps a CacheStats around that.  A DelayCache and MockTimer cache are added
// as well though they are not used yet in the test.  The intent is to make sure
// the latency histogram looks sane.

#include "net/instaweb/util/public/cache_stats.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/delay_cache.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/thread_system.h"

namespace {
const int kMaxSize = 100;
const int kNumThreads = 4;
const int kNumInserts = 10;
}

namespace net_instaweb {

class CacheStatsTest : public testing::Test {
 protected:
  CacheStatsTest()
      : lru_cache_(kMaxSize),
        thread_system_(ThreadSystem::CreateThreadSystem()),
        delay_cache_(new DelayCache(&lru_cache_, thread_system_.get())),
        timer_(MockTimer::kApr_5_2010_ms) {
    CacheStats::InitStats("test", &stats_);
    cache_stats_.reset(new CacheStats("test", delay_cache_, &timer_, &stats_));
  }

  LRUCache lru_cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  DelayCache* delay_cache_;
  MockTimer timer_;
  SimpleStats stats_;
  scoped_ptr<CacheStats> cache_stats_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheStatsTest);
};

TEST_F(CacheStatsTest, BasicOperation) {
  SharedString put_buffer("val");
  cache_stats_->Put("key", &put_buffer);
  EXPECT_EQ(1, stats_.GetVariable("test_inserts")->Get());
  CacheTestBase::Callback callback;
  cache_stats_->Get("key", &callback);
  EXPECT_EQ(1, stats_.GetVariable("test_hits")->Get());
  EXPECT_EQ(0, stats_.GetVariable("test_misses")->Get());
  EXPECT_TRUE(callback.called_);
  EXPECT_EQ(CacheInterface::kAvailable, callback.state_);
  EXPECT_EQ(GoogleString("val"), *callback.value()->get());

  cache_stats_->Get("no such key", &callback);
  EXPECT_EQ(1, stats_.GetVariable("test_misses")->Get());
  EXPECT_EQ(CacheInterface::kNotFound, callback.state_);

  cache_stats_->Delete("key");
  EXPECT_EQ(1, stats_.GetVariable("test_deletes")->Get());

  // TODO(jmarantz): we do not currently have a functional histogram
  // implementation in SimpleStats, so we cannot conveniently test it.
  // It is feasible we could mock it.  Note that this makes the
  // instantiation of MockTimer and DelayCache in this test file sort
  // of pointless, but are left here for convenience on follow-up.
  //
  // It should be possible to use SharedMemHistogram, we just need an
  // in-process version of SharedMemSystem.
  //
  // Histogram* latency = stats_.GetHistogram("test_hit_latency_us");
  // EXPECT_EQ(1, latency->Count());
}

}  // namespace net_instaweb
