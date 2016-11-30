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

#include "pagespeed/kernel/cache/cache_batcher.h"

#include <cstddef>

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/async_cache.h"
#include "pagespeed/kernel/cache/cache_batcher_testing_peer.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/cache/delay_cache.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/cache/threadsafe_cache.h"
#include "pagespeed/kernel/cache/write_through_cache.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"

namespace {
const size_t kMaxSize = 100;
const int kMaxWorkers = 2;
}

namespace net_instaweb {

class CacheBatcherTest : public CacheTestBase {
 protected:
  CacheBatcherTest() : expected_pending_(0) {
    thread_system_.reset(Platform::CreateThreadSystem());
    statistics_.reset(new SimpleStats(thread_system_.get()));
    CacheBatcher::InitStats(statistics_.get());
    lru_cache_.reset(new LRUCache(kMaxSize));
    timer_.reset(thread_system_->NewTimer());
    pool_.reset(
        new QueuedWorkerPool(kMaxWorkers, "cache", thread_system_.get()));
    threadsafe_cache_.reset(new ThreadsafeCache(
        lru_cache_.get(), thread_system_->NewMutex()));
    async_cache_.reset(new AsyncCache(threadsafe_cache_.get(), pool_.get()));
    delay_cache_.reset(new DelayCache(async_cache_.get(),
                                      thread_system_.get()));
    // Note: it is each test's responsibility to reset batcher_ with the backend
    // and options that it needs.
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

  void ChangeBatcherConfig(const CacheBatcher::Options& options,
                           CacheInterface* cache) {
    batcher_.reset(new CacheBatcher(options,
                                    cache,
                                    thread_system_->NewMutex(),
                                    statistics_.get()));
  }

  // After the Done() callback is be called, there is a slight delay
  // in the worker thread before the CacheBatcher knows it can
  // schedule another lookup.  To test the sequences we want, wait
  // till the batcher catches up with our expectations.
  virtual void PostOpCleanup() {
    while ((num_in_flight_keys() != expected_pending_) ||
           (async_cache_->outstanding_operations() != 0)) {
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

  int num_in_flight_keys() {
    return peer_.num_in_flight_keys(batcher_.get());
  }

  int LastBatchSize() {
    return peer_.last_batch_size(batcher_.get());
  }

  scoped_ptr<LRUCache> lru_cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<ThreadsafeCache> threadsafe_cache_;
  scoped_ptr<Timer> timer_;
  scoped_ptr<QueuedWorkerPool> pool_;
  scoped_ptr<AsyncCache> async_cache_;
  scoped_ptr<DelayCache> delay_cache_;
  scoped_ptr<SimpleStats> statistics_;
  scoped_ptr<CacheBatcher> batcher_;
  CacheBatcherTestingPeer peer_;
  int expected_pending_;
};

// In this version, no keys are delayed, so the batcher has no opportunity
// to batch.  This test is copied from lru_cache_test.cc.  Note that we are
// going through the CacheBatcher/Delay/AsyncCache/ThreadsafeCache but the
// LRUCache should be quiescent every time we look directly at it.
TEST_F(CacheBatcherTest, PutGetDelete) {
  ChangeBatcherConfig(CacheBatcher::Options(), delay_cache_.get());

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

  CheckDelete("Name");
  lru_cache_->SanityCheck();
  CheckNotFound("Name");
  EXPECT_EQ(static_cast<size_t>(0), lru_cache_->size_bytes());
  EXPECT_EQ(static_cast<size_t>(0), lru_cache_->num_elements());
  lru_cache_->SanityCheck();
}

TEST_F(CacheBatcherTest, DelayN0NoParallelism) {
  CacheBatcher::Options options;
  options.max_parallel_lookups = 1;
  ChangeBatcherConfig(options, delay_cache_.get());

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
  EXPECT_EQ(3, LastBatchSize());

  // Further fetches will execute immediately again.
  CheckGet("n3", "v3");
}

TEST_F(CacheBatcherTest, DelayN0TwoWayParallelism) {
  CacheBatcher::Options options;
  options.max_parallel_lookups = 2;
  ChangeBatcherConfig(options, delay_cache_.get());

  PopulateCache(8);

  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());

  // We still have some parallelism available to us, so "n1" and "n2" will
  // complete even while "n0" is outstanding.
  CheckGet("n1", "v1");
  CheckGet("n2", "v2");
  ASSERT_EQ(1, num_in_flight_keys());

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
  EXPECT_EQ(3, LastBatchSize());

  // Finally, release n3 and we are all clean.
  ReleaseKey("n3");
  WaitAndCheck(n3, "v3");
}

TEST_F(CacheBatcherTest, ExceedMaxPendingUniqueAndDrop) {
  CacheBatcher::Options options;
  options.max_parallel_lookups = 1;
  options.max_pending_gets = 4;
  ChangeBatcherConfig(options, delay_cache_.get());

  PopulateCache(5);

  // Delaying "n0" causes the fetches for "n1" and "n2" to be batched in
  // CacheBatcher.  They can be executed once "n0" is released.
  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());
  Callback* n1 = InitiateGet("n1");
  EXPECT_EQ(2, outstanding_fetches());
  Callback* not_found = InitiateGet("not found");
  EXPECT_EQ(3, outstanding_fetches());
  Callback* n2 = InitiateGet("n2");
  EXPECT_EQ(4, outstanding_fetches());
  Callback* n3 = InitiateGet("n3");     // This will be dropped immediately and
  WaitAndCheckNotFound(n3);             // reported as not found.
  EXPECT_EQ(1, statistics_->GetVariable("cache_batcher_dropped_gets")->Get());

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheck(n1, "v1");
  WaitAndCheckNotFound(not_found);
  WaitAndCheck(n2, "v2");

  // outstanding_fetches() won't be stable to look at until all 3 callback Waits
  // are called.
  EXPECT_EQ(0, outstanding_fetches());
  EXPECT_EQ(3, LastBatchSize());

  // Further fetches will execute immediately again.
  CheckGet("n4", "v4");
}

TEST_F(CacheBatcherTest, ExceedMaxPendingDuplicateAndDrop) {
  // Similar to ExceedMaxPendingUniqueAndDrop, except that this fills the
  // queue with lookups of the same key.
  CacheBatcher::Options options;
  options.max_parallel_lookups = 1;
  options.max_pending_gets = 4;
  ChangeBatcherConfig(options, delay_cache_.get());

  PopulateCache(5);

  // Delaying "n0" causes the fetches for "n1" to be batched in CacheBatcher.
  // They can be executed once "n0" is released.
  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());
  Callback* n1_0 = InitiateGet("n1");
  Callback* n1_1 = InitiateGet("n1");
  Callback* n1_2 = InitiateGet("n1");
  EXPECT_EQ(4, outstanding_fetches());
  Callback* n1_3 = InitiateGet("n1");   // This will be dropped immediately and
  WaitAndCheckNotFound(n1_3);           // reported as not found.
  EXPECT_EQ(1, statistics_->GetVariable("cache_batcher_dropped_gets")->Get());

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheck(n1_0, "v1");
  WaitAndCheck(n1_1, "v1");
  WaitAndCheck(n1_2, "v1");

  // outstanding_fetches() won't be stable to look at until all 3 callback Waits
  // are called.
  EXPECT_EQ(0, outstanding_fetches());
  EXPECT_EQ(1, LastBatchSize());

  // Further fetches will execute immediately again.
  CheckGet("n4", "v4");
}

TEST_F(CacheBatcherTest, ExceedMaxPendingInFlightAndDrop) {
  // Similar to ExceedMaxPendingUniqueAndDrop, except that this fills the
  // queue with lookups of different keys, lets them go into the
  // in_flight queue, then verifies that no more keys can be put into the
  // queue.
  CacheBatcher::Options options;
  options.max_parallel_lookups = 1;
  options.max_pending_gets = 3;
  ChangeBatcherConfig(options, delay_cache_.get());

  PopulateCache(5);

  // Delaying "n0" causes subsequent fetches to be batched in CacheBatcher.
  // They can be executed once "n0" is released.
  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());

  // Now fill the queue.
  // Note that ValidateCandidate is called ASAP, but fetches are in-memory until
  // Done is called (which happens only when DelayCache releases the key for
  // that callback).
  Callback* n1 = InitiateGet("n1");
  EXPECT_EQ(2, outstanding_fetches());
  Callback* n2 = InitiateGet("n2");
  EXPECT_EQ(3, outstanding_fetches());  // n0, n1, n2
  Callback* n3 = InitiateGet("n3");  // This should be dropped immediately
  EXPECT_EQ(3, outstanding_fetches());  // n0, n1, n2
  WaitAndCheckNotFound(n3);

  DelayKey("n1");
  DelayKey("n2");

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  EXPECT_EQ(2, outstanding_fetches());

  Callback* n0_dup = InitiateGet("n0");
  EXPECT_EQ(3, outstanding_fetches());

  // Now n-1 fetches are in flight, 1 fetch is queued, so we should not be able
  // to queue any more fetches.

  Callback* n0_drop = InitiateGet("n0");
  WaitAndCheckNotFound(n0_drop);

  // Clean out in_flight_ and queue_.
  ReleaseKey("n1");
  ReleaseKey("n2");
  WaitAndCheck(n1, "v1");
  WaitAndCheck(n2, "v2");
  WaitAndCheck(n0_dup, "v0");

  // Further fetches will execute immediately again.
  CheckGet("n4", "v4");
}

TEST_F(CacheBatcherTest, CoalesceDuplicateGets) {
  CacheBatcher::Options options;
  options.max_parallel_lookups = 1;
  options.max_pending_gets = 10;
  ChangeBatcherConfig(options, delay_cache_.get());
  PopulateCache(5);

  // Delay n0 so that the first lookup gets fired off immediately, but
  // subsequent lookups block until n0 is released.
  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());
  Callback* n1 = InitiateGet("n1");
  EXPECT_EQ(2, outstanding_fetches());
  Callback* not_found = InitiateGet("not_found");
  EXPECT_EQ(3, outstanding_fetches());
  Callback* n2 = InitiateGet("n2");
  EXPECT_EQ(4, outstanding_fetches());
  Callback* not_found_dup = InitiateGet("not_found");
  EXPECT_EQ(5, outstanding_fetches());
  Callback* n1_dup = InitiateGet("n1");
  n1_dup->set_invalid_key("n1");
  EXPECT_EQ(6, outstanding_fetches());

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheck(n1, "v1");
  WaitAndCheckNotFound(n1_dup);  // Check duplicate non-validated hit.
  WaitAndCheck(n2, "v2");
  WaitAndCheckNotFound(not_found);
  WaitAndCheckNotFound(not_found_dup);

  // Even though we initiated 4 gets, 2 were for the same key (n1), so we expect
  // 3 cache hits. Similarly, we expect 1 miss, even though we initiated 2 gets
  // for that key.
  EXPECT_EQ(3, lru_cache_->num_hits());
  EXPECT_EQ(1, lru_cache_->num_misses());
}

TEST_F(CacheBatcherTest, CoalesceDuplicateGetsParallel) {
  // Identical to CoalesceDuplicateGets, but with parallism. This is intended to
  // exercise CacheBatcher's internal synchronization and merging of in_flight_
  // and queue_ gets.
  CacheBatcher::Options options;
  options.max_parallel_lookups = 2;
  options.max_pending_gets = 10;
  ChangeBatcherConfig(options, delay_cache_.get());

  PopulateCache(5);

  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());  // n0

  // n0 is in flight and delayed, but we still have one ready thread to use.
  // These gets should not interfere with CacheBatcher's ability to track the
  // in flight get of "n0".
  //
  // (We must delay "n1", to prevent a data race when we make one of its
  // fetches invalid.)
  DelayKey("n1");
  Callback* n1 = InitiateGet("n1");
  Callback* not_found = InitiateGet("not_found");
  Callback* n2 = InitiateGet("n2");
  Callback* not_found_dup = InitiateGet("not_found");
  Callback* n1_dup = InitiateGet("n1");
  n1_dup->set_invalid_key("n1");
  ReleaseKey("n1");

  WaitAndCheck(n1, "v1");
  WaitAndCheckNotFound(n1_dup);  // Check duplicate non-validated hit.
  WaitAndCheck(n2, "v2");
  WaitAndCheckNotFound(not_found);
  WaitAndCheckNotFound(not_found_dup);

  EXPECT_EQ(1, outstanding_fetches());

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
}

TEST_F(CacheBatcherTest, CoalesceInFlightGet) {
  CacheBatcher::Options options;
  options.max_parallel_lookups = 1;
  ChangeBatcherConfig(options, delay_cache_.get());
  PopulateCache(5);

  DelayKey("n0");
  // The first fetch of n0 will be immediate, but will be stuck in flight until
  // n0 is released.
  Callback* n0 = InitiateGet("n0");

  DelayKey("n1");
  Callback* n1 = InitiateGet("n1");  // Should be queued behind n0.

  // Since there's already an in flight fetch of n0, this callback should
  // piggyback on it, rather than going into the queue for a separate request.
  Callback* n0_dup = InitiateGet("n0");

  EXPECT_EQ(3, outstanding_fetches());

  // When n0 is released, both fetches of n0 should be coalesced and be
  // completed by the same cache hit.
  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheck(n0_dup, "v0");

  EXPECT_EQ(1, outstanding_fetches());

  ReleaseKey("n1");
  WaitAndCheck(n1, "v1");

  EXPECT_EQ(0, outstanding_fetches());
  EXPECT_EQ(2, lru_cache_->num_hits());
}

TEST_F(CacheBatcherTest, CheckWriteThroughCacheCompatibility) {
  LRUCache small_cache(kMaxSize);
  LRUCache big_cache(kMaxSize);
  WriteThroughCache write_through_cache(&small_cache, &big_cache);
  CacheBatcher::Options options;
  options.max_parallel_lookups = 1;
  ChangeBatcherConfig(options, &write_through_cache);
  PopulateCache(5);

  // Make sure we find a valid value in L2 if it's shadowed by an invalid one
  // in L1.
  CheckPut(&small_cache, "Name", "invalid");
  CheckPut(&big_cache, "Name", "valid");
  set_invalid_value("invalid");
  CheckNotFound(&small_cache, "Name");
  CheckGet(&big_cache, "Name", "valid");
  // Here's the interesting bit: make sure the write through cache still works
  // properly.
  CheckGet(batcher_.get(), "Name", "valid");
  // Make sure we fixed up the small_cache_
  CheckGet(&small_cache, "Name", "valid");
}

}  // namespace net_instaweb
