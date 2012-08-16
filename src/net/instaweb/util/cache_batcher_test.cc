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

// Unit-test CacheBatcher, using LRUCache, AsyncCache, and DelayCache.

#include "net/instaweb/util/public/cache_batcher.h"

#include <cstddef>
#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/worker_test_base.h"
#include "net/instaweb/util/public/async_cache.h"
#include "net/instaweb/util/public/delay_cache.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/timer.h"

namespace {
const size_t kMaxSize = 100;
const int kMaxWorkers = 2;
}

namespace net_instaweb {

class CacheBatcherTest : public CacheTestBase {
 protected:
  CacheBatcherTest() : expected_pending_(0) {
    CacheBatcher::Initialize(&statistics_);
    lru_cache_ = new LRUCache(kMaxSize);
    thread_system_.reset(ThreadSystem::CreateThreadSystem());
    timer_.reset(thread_system_->NewTimer());
    pool_.reset(new QueuedWorkerPool(kMaxWorkers, thread_system_.get()));
    ThreadsafeCache* threadsafe_cache = new ThreadsafeCache(
        lru_cache_, thread_system_->NewMutex());
    async_cache_.reset(new AsyncCache(threadsafe_cache,
                                      thread_system_->NewMutex(),
                                      pool_.get()));
    delay_cache_ = new DelayCache(async_cache_.get(), thread_system_.get());
    batcher_.reset(new CacheBatcher(delay_cache_, thread_system_->NewMutex(),
                                    &statistics_));
    set_mutex(thread_system_->NewMutex());
  }

  // Make sure we shut down the worker pool prior to destructing AsyncCache.
  virtual void TearDown() {
    pool_->ShutDown();
  }

  class SyncPointCallback : public CacheTestBase::Callback {
   public:
    explicit SyncPointCallback(CacheBatcherTest* test)
        : Callback(test),
          sync_point_(test->thread_system_.get()) {
    }

    virtual void Done(CacheInterface::KeyState state) {
      Callback::Done(state);
      sync_point_.Notify();
    }

    virtual void Wait() { sync_point_.Wait(); }

   private:
    WorkerTestBase::SyncPoint sync_point_;
  };

  virtual CacheInterface* Cache() { return batcher_.get(); }
  virtual Callback* NewCallback() { return new SyncPointCallback(this); }

  // After the Done() callback is be called, there is a slight delay
  // in the worker thread before the CacheBatcher knows it can
  // schedule another lookup.  To test the sequences we want, wait
  // till the batcher catches up with our expectations.
  virtual void PostOpCleanup() {
    while (batcher_->Pending() != expected_pending_) {
      timer_->SleepMs(1);
    }
  }

  void DelayKey(const GoogleString& key) {
    delay_cache_->DelayKey(key);
    ++expected_pending_;
  }

  void ReleaseKey(const GoogleString& key) {
    delay_cache_->ReleaseKey(key);
    --expected_pending_;
  }

  LRUCache* lru_cache_;  // owned by ThreadsafeCache.
  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<Timer> timer_;
  scoped_ptr<QueuedWorkerPool> pool_;
  scoped_ptr<AsyncCache> async_cache_;
  DelayCache* delay_cache_;  // owned by CacheBatcher.
  SimpleStats statistics_;
  scoped_ptr<CacheBatcher> batcher_;
  int expected_pending_;
};

// In this version, no keys are delayed, so the batcher has no opportunity
// to batch.  This test is copied from lru_cache_test.cc.  Note that we are
// going through the CacheBatcher/Delay/AsyncCache/ThreadsafeCache but the
// LRUCache should be quiescent every time we look directly at it.
TEST_F(CacheBatcherTest, PutGetDelete) {
  EXPECT_EQ(static_cast<size_t>(0), lru_cache_->size_bytes());
  EXPECT_EQ(static_cast<size_t>(0), lru_cache_->num_elements());
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  EXPECT_EQ(static_cast<size_t>(9), lru_cache_->size_bytes());
  EXPECT_EQ(static_cast<size_t>(1), lru_cache_->num_elements());
  CheckNotFound("Another Name");

  CheckPut("Name", "NewValue");
  CheckGet("Name", "NewValue");
  EXPECT_EQ(static_cast<size_t>(12), lru_cache_->size_bytes());
  EXPECT_EQ(static_cast<size_t>(1), lru_cache_->num_elements());

  batcher_->Delete("Name");
  lru_cache_->SanityCheck();
  CheckNotFound("Name");
  EXPECT_EQ(static_cast<size_t>(0), lru_cache_->size_bytes());
  EXPECT_EQ(static_cast<size_t>(0), lru_cache_->num_elements());
  lru_cache_->SanityCheck();
}

TEST_F(CacheBatcherTest, DelayN0NoParallelism) {
  batcher_->set_max_parallel_lookups(1);

  PopulateCache(4);

  // Delaying "n0" causes the fetches for "n1" and "n2" to be batched in
  // CacheBatcher.  They can be executed once "n0" is released.
  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());
  Callback* n1 = InitiateGet("n1");
  Callback* not_found = InitiateGet("not found");
  EXPECT_EQ(3, outstanding_fetches());
  Callback* n2 = InitiateGet("n2");
  EXPECT_EQ(4, outstanding_fetches());

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheck(n1, "v1");
  WaitAndCheck(n2, "v2");
  WaitAndCheckNotFound(not_found);

  // outstanding_fetches() won't be stable to look at until all 3 callback Waits
  // are called.
  EXPECT_EQ(0, outstanding_fetches());
  EXPECT_EQ(3, batcher_->last_batch_size());

  // Further fetches will execute immediately again.
  CheckGet("n3", "v3");
}

TEST_F(CacheBatcherTest, DelayN0TwoWayParallelism) {
  batcher_->set_max_parallel_lookups(2);

  PopulateCache(8);

  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());

  // We still have some parallelism available to us, so "n1" and "n2" will
  // complete even while "n0" is outstanding.
  CheckGet("n1", "v1");
  CheckGet("n2", "v2");
  ASSERT_EQ(1, batcher_->Pending());

  // Now block "n3" and look it up.  n4 and n5 will now be delayed and batched.
  DelayKey("n3");
  Callback* n3 = InitiateGet("n3");
  Callback* not_found = InitiateGet("not found");
  Callback* n4 = InitiateGet("n4");
  EXPECT_EQ(4, outstanding_fetches());  // n0, n3, "not found", n4
  Callback* n5 = InitiateGet("n5");
  EXPECT_EQ(5, outstanding_fetches());

  // Releasing n0 frees a thread and now n4 and n5 can be completed.
  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheckNotFound(not_found);
  WaitAndCheck(n4, "v4");
  WaitAndCheck(n5, "v5");
  EXPECT_EQ(1, outstanding_fetches());
  EXPECT_EQ(3, batcher_->last_batch_size());

  // Finally, release n3 and we are all clean.
  ReleaseKey("n3");
  WaitAndCheck(n3, "v3");
}

TEST_F(CacheBatcherTest, ExceedMaxQueueAndDrop) {
  batcher_->set_max_parallel_lookups(1);
  batcher_->set_max_queue_size(3);

  PopulateCache(5);

  // Delaying "n0" causes the fetches for "n1" and "n2" to be batched in
  // CacheBatcher.  They can be executed once "n0" is released.
  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());
  Callback* n1 = InitiateGet("n1");
  Callback* not_found = InitiateGet("not found");
  EXPECT_EQ(3, outstanding_fetches());
  Callback* n2 = InitiateGet("n2");
  EXPECT_EQ(4, outstanding_fetches());
  Callback* n3 = InitiateGet("n3");     // This will be dropped immediately and
  WaitAndCheckNotFound(n3);             // reported as not found.
  EXPECT_EQ(1, statistics_.GetVariable("cache_batcher_dropped_gets")->Get());

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheck(n1, "v1");
  WaitAndCheckNotFound(not_found);
  WaitAndCheck(n2, "v2");

  // outstanding_fetches() won't be stable to look at until all 3 callback Waits
  // are called.
  EXPECT_EQ(0, outstanding_fetches());
  EXPECT_EQ(3, batcher_->last_batch_size());

  // Further fetches will execute immediately again.
  CheckGet("n4", "v4");
}

}  // namespace net_instaweb
