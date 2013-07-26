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

// Unit-test for DelayCache

#include "pagespeed/kernel/cache/delay_cache.h"

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {
namespace {

const size_t kMaxSize = 100;

class DelayCacheTest : public CacheTestBase {
 protected:
  DelayCacheTest()
      : lru_cache_(kMaxSize),
        thread_system_(Platform::CreateThreadSystem()),
        cache_(&lru_cache_, thread_system_.get()) {}

  virtual CacheInterface* Cache() { return &cache_; }

 protected:
  LRUCache lru_cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  DelayCache cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DelayCacheTest);
};

TEST_F(DelayCacheTest, NoDelayOps) {
  // Basic operation w/o any delay injected.
  CheckNotFound("Name");
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  cache_.Delete("Name");
  CheckNotFound("Name");
}

TEST_F(DelayCacheTest, DelayOpsFound) {
  // Load the value.
  CheckPut("Name", "Value");
  CheckPut("OtherName", "OtherValue");

  cache_.DelayKey("Name");
  cache_.DelayKey("OtherName");

  // Try getting...
  CacheTestBase::Callback result, other_result;
  cache_.Get("Name", &result);
  cache_.Get("OtherName", &other_result);

  // Initially, should not have been called.
  EXPECT_FALSE(result.called());

  // Release an unrelated key.  That should not call "Name".
  cache_.ReleaseKey("OtherName");
  EXPECT_FALSE(result.called());
  EXPECT_TRUE(other_result.called());
  EXPECT_EQ(CacheInterface::kAvailable, other_result.state());

  // Now after it is released, it should be OK.
  cache_.ReleaseKey("Name");
  EXPECT_TRUE(result.called());
  EXPECT_EQ(CacheInterface::kAvailable, result.state());
  EXPECT_EQ("Value", result.value()->Value());
}

TEST_F(DelayCacheTest, DelayOpsNotFound) {
  // Do the same thing with a miss.
  cache_.DelayKey("Name");
  cache_.DelayKey("OtherName");
  CacheTestBase::Callback result, other_result;
  cache_.Get("Name", result.Reset());
  cache_.Get("OtherName", &other_result);
  EXPECT_FALSE(result.called());
  cache_.ReleaseKey("OtherName");
  EXPECT_FALSE(result.called());
  EXPECT_TRUE(other_result.called());
  EXPECT_EQ(CacheInterface::kNotFound, other_result.state());
  cache_.ReleaseKey("Name");
  EXPECT_TRUE(result.called());
  EXPECT_EQ(CacheInterface::kNotFound, result.state());
}

TEST_F(DelayCacheTest, DelayOpsFoundInSequence) {
  scoped_ptr<ThreadSystem> thread_system(Platform::CreateThreadSystem());
  QueuedWorkerPool pool(1, "test", thread_system.get());
  QueuedWorkerPool::Sequence* sequence = pool.NewSequence();
  WorkerTestBase::SyncPoint sync_point(thread_system.get());

  // Load the value.
  CheckPut("Name", "Value");
  CheckPut("OtherName", "OtherValue");

  cache_.DelayKey("Name");
  cache_.DelayKey("OtherName");

  // Try getting...
  CacheTestBase::Callback result, other_result;
  cache_.Get("Name", &result);
  cache_.Get("OtherName", &other_result);

  // Initially, should not have been called.
  EXPECT_FALSE(result.called());

  // Release an unrelated key.  That should not call "Name".
  cache_.ReleaseKeyInSequence("OtherName", sequence);
  sequence->Add(new WorkerTestBase::NotifyRunFunction(&sync_point));
  sync_point.Wait();

  EXPECT_FALSE(result.called());
  EXPECT_TRUE(other_result.called());
  EXPECT_EQ(CacheInterface::kAvailable, other_result.state());

  // Now after it is released, it should be OK.
  cache_.ReleaseKey("Name");
  sequence->Add(new WorkerTestBase::NotifyRunFunction(&sync_point));
  sync_point.Wait();

  EXPECT_TRUE(result.called());
  EXPECT_EQ(CacheInterface::kAvailable, result.state());
  EXPECT_EQ("Value", result.value()->Value());

  pool.ShutDown();
}

}  // namespace

}  // namespace net_instaweb
