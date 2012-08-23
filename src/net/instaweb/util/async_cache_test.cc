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

// Unit-test AsyncCache, using LRUCache.

#include "net/instaweb/util/public/async_cache.h"

#include <cstddef>
#include <map>
#include <utility>                      // for pair

#include "base/logging.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/worker_test_base.h"

namespace {
const size_t kMaxSize = 100;
}

namespace net_instaweb {

class AsyncCacheTest : public CacheTestBase {
 protected:
  class DelayMap {
   public:
    explicit DelayMap(ThreadSystem* thread_system)
        : mutex_(thread_system->NewMutex()),
          thread_system_(thread_system) {
    }
    ~DelayMap() { STLDeleteValues(&map_); }

    // Note that Delay is called only in test mainlines, prior to
    // any cache lookups being queued for that key.
    void Delay(const GoogleString& key) {
      WorkerTestBase::SyncPoint* sync_point =
          new WorkerTestBase::SyncPoint(thread_system_);
      {
        ScopedMutex lock(mutex_.get());
        map_[key] = sync_point;
      }
    }

    // Note that Wait is only called once per key, so there is no wait/wait
    // race.
    void Wait(const GoogleString& key) {
      // We can't use ScopedMutex easily here because we want to avoid
      // holding our mutex while blocking or freeing memory.
      mutex_->Lock();
      Map::iterator p = map_.find(key);
      if (p != map_.end()) {
        WorkerTestBase::SyncPoint* sync_point = p->second;

        // In order to avoid deadlock with Wait/Delay on other keys,
        // and most importantly Notify() on this key, we must release
        // the lock before waiting on the sync-point.
        mutex_->Unlock();
        sync_point->Wait();
        mutex_->Lock();

        map_.erase(p);
        mutex_->Unlock();
        delete sync_point;
      } else {
        mutex_->Unlock();
      }
    }

    void Notify(const GoogleString& key) {
      WorkerTestBase::SyncPoint* sync_point = NULL;
      {
        ScopedMutex lock(mutex_.get());
        Map::iterator p = map_.find(key);
        if (p != map_.end()) {
          sync_point = p->second;
        }
      }
      CHECK(sync_point != NULL);
      sync_point->Notify();
    }

    typedef std::map<GoogleString, WorkerTestBase::SyncPoint*> Map;

    scoped_ptr<AbstractMutex> mutex_;
    ThreadSystem* thread_system_;
    Map map_;
  };

  // Tweak of LRU cache to block in Get on a sync-point.  Note that we don't
  // use DelayCache because that doeesn't block; it only defers the Done
  // callback.  In this case we want to mimic the behavior of a slow blocking
  // cache using a fast blocking cache, so we use a sync-point.
  class SyncedLRUCache : public ThreadsafeCache {
   public:
    SyncedLRUCache(DelayMap* delay_map, LRUCache* lru_cache,
                   AbstractMutex* mutex)
        : ThreadsafeCache(lru_cache, mutex),
          delay_map_(delay_map) {
    }
    virtual ~SyncedLRUCache() {}

    void Get(const GoogleString& key, Callback* callback) {
      delay_map_->Wait(key);
      ThreadsafeCache::Get(key, callback);
    }

   private:
    DelayMap* delay_map_;
    DISALLOW_COPY_AND_ASSIGN(SyncedLRUCache);
  };

  class AsyncCallback : public CacheTestBase::Callback {
   public:
    explicit AsyncCallback(AsyncCacheTest* test)
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

  AsyncCacheTest()
      : lru_cache_(new LRUCache(kMaxSize)),
        thread_system_(ThreadSystem::CreateThreadSystem()),
        delay_map_(thread_system_.get()),
        timer_(thread_system_->NewTimer()),
        suppress_post_get_cleanup_(false) {
    set_mutex(thread_system_->NewMutex());
  }

  ~AsyncCacheTest() {
    pool_->ShutDown();  // quiesce before destructing cache.
  }

  void InitCache(int num_workers, int num_threads) {
    pool_.reset(new QueuedWorkerPool(num_workers, thread_system_.get()));
    SyncedLRUCache* synced_lru_cache = new SyncedLRUCache(
        &delay_map_, lru_cache_, thread_system_->NewMutex());
    async_cache_.reset(new AsyncCache(
        synced_lru_cache, thread_system_->NewMutex(), pool_.get()));
    async_cache_->set_num_threads(num_threads);
  }

  virtual CacheInterface* Cache() { return async_cache_.get(); }
  virtual Callback* NewCallback() { return new AsyncCallback(this); }

  virtual void PostOpCleanup() {
    // Wait until the AsyncCache available thread-count is restored to
    // non-zero.  Note that in AsyncCache we call blocking cache
    // Get/MultiGet first, then decrement the in-use thread-count, so
    // the cache is not immediately available for another Get until
    // the thread-count has been decremented.
    //
    // If mainline issues another Get too quickly after the callback is
    // called, it will immediately fail due to the count not being
    // updated yet.
    if (!suppress_post_get_cleanup_) {
      while (!async_cache_->CanIssueGet()) {
        timer_->SleepMs(1);
      }
    }
  }

  void DelayKey(const GoogleString& key) {
    delay_map_.Delay(key);
  }

  void ReleaseKey(const GoogleString& key) {
    delay_map_.Notify(key);
  }

  LRUCache* lru_cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  DelayMap delay_map_;
  scoped_ptr<Timer> timer_;
  scoped_ptr<QueuedWorkerPool> pool_;
  scoped_ptr<AsyncCache> async_cache_;
  bool suppress_post_get_cleanup_;
};

// In this version, no keys are delayed, so AsyncCache will not
// introduce parallelism.  This test is copied from lru_cache_test.cc.
// Note that we are going through the AsyncCache/ThreadsafeCache but
// the LRUCache should be quiescent every time we look directly at it.
//
// TODO(jmarantz): refactor this with LRUCacheTest::PutGetDelete.
TEST_F(AsyncCacheTest, PutGetDelete) {
  InitCache(1, 1);

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

  async_cache_->Delete("Name");
  lru_cache_->SanityCheck();
  SharedString value_buffer;
  CheckNotFound("Name");
  EXPECT_EQ(static_cast<size_t>(0), lru_cache_->size_bytes());
  EXPECT_EQ(static_cast<size_t>(0), lru_cache_->num_elements());
  lru_cache_->SanityCheck();
}

TEST_F(AsyncCacheTest, DelayN0NoParallelism) {
  InitCache(1, 1);

  PopulateCache(4);  // Inserts "n0"->"v0", "n1"->"v1", "n2"->"v2", "n3"->"v3".

  // Delaying "n0" causes the fetches for "n1" to be dropped
  // immediately because we've initialized the cache to only one
  // request at a time.
  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());
  suppress_post_get_cleanup_ = true;  // avoid blocking waiting for delayed n0.
  CheckNotFound("n1");
  suppress_post_get_cleanup_ = false;
  EXPECT_EQ(1, outstanding_fetches());

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  CheckNotFound("not found");
  EXPECT_EQ(0, outstanding_fetches());

  // Further fetches will execute immediately again.
  CheckGet("n3", "v3");
}

TEST_F(AsyncCacheTest, DelayN0TwoWayParallelism) {
  InitCache(2, 2);

  PopulateCache(8);

  DelayKey("n0");
  Callback* n0 = InitiateGet("n0");
  EXPECT_EQ(1, outstanding_fetches());

  // We still have some parallelism available to us, so "n2" and "n3" will
  // complete even while n1 is outstanding.
  CheckGet("n1", "v1");
  CheckGet("n2", "v2");

  // Now block "n3" and look it up.  n4 and n5 will now be dropped.
  DelayKey("n3");
  Callback* n3 = InitiateGet("n3");
  Callback* n4 = InitiateGet("n4");
  Callback* n5 = InitiateGet("n5");
  EXPECT_EQ(2, outstanding_fetches());
  suppress_post_get_cleanup_ = true;  // avoid blocking waiting for delayed n0.
  WaitAndCheckNotFound(n4);
  WaitAndCheckNotFound(n5);
  suppress_post_get_cleanup_ = false;

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  CheckGet("n6", "v6");
  EXPECT_EQ(1, outstanding_fetches());

  // Finally, release n3 and we are all clean.
  ReleaseKey("n3");
  WaitAndCheck(n3, "v3");
  EXPECT_EQ(0, outstanding_fetches());

  CheckGet("n7", "v7");
  EXPECT_EQ(0, outstanding_fetches());
}

TEST_F(AsyncCacheTest, MultiGet) {
  InitCache(1, 1);
  TestMultiGet();
}

TEST_F(AsyncCacheTest, MultiGetDrop) {
  InitCache(1, 1);

  PopulateCache(3);
  DelayKey("n2");
  Callback* n2 = InitiateGet("n2");

  Callback* n0 = AddCallback();
  Callback* not_found = AddCallback();
  Callback* n1 = AddCallback();
  IssueMultiGet(n0, "n0", not_found, "not_found", n1, "n1");
  suppress_post_get_cleanup_ = true;  // avoid blocking waiting for delayed n0.
  WaitAndCheckNotFound(n0);
  WaitAndCheckNotFound(not_found);
  WaitAndCheckNotFound(n1);
  suppress_post_get_cleanup_ = false;

  ReleaseKey("n2");
  WaitAndCheck(n2, "v2");
}

TEST_F(AsyncCacheTest, StopGets) {
  InitCache(1, 1);
  PopulateCache(1);
  CheckGet("n0", "v0");
  async_cache_->StopCacheGets();
  suppress_post_get_cleanup_ = true;  // avoid blocking waiting for delayed n0.
  CheckNotFound("n0");
  suppress_post_get_cleanup_ = false;
}

TEST_F(AsyncCacheTest, ShutdownQueue) {
  InitCache(1, 1);
  PopulateCache(1);
  pool_->ShutDown();
  CheckNotFound("n0");
}

TEST_F(AsyncCacheTest, ShutdownQueueWhileBusy) {
  InitCache(1, 1);
  PopulateCache(1);

  WorkerTestBase::SyncPoint sync_point(thread_system_.get());
  QueuedWorkerPool::Sequence* sequence = pool_->NewSequence();
  sequence->Add(new WorkerTestBase::WaitRunFunction(&sync_point));

  Callback* n0 = InitiateGet("n0");  // blocks waiting for timer.

  QueuedWorkerPool pool2(2, thread_system_.get());
  QueuedWorkerPool::Sequence* sequence2 = pool2.NewSequence();
  sequence2->Add(MakeFunction(pool_.get(), &QueuedWorkerPool::ShutDown));

  // We must now wait for the ShutDown sequence be *initiated* on
  // pool_.  However, there is no callback for this at all, and
  // ShutDown itself will block due to the fact that the function
  // currently running in pool_ is blocked.  To resolve this we can
  // add a short sleep, so we are likely to get into the ShutDown
  // flow before releasing the sync_point.
  //
  // Calling pool->ShutDown() directly blocks on the completion
  // of the function blocked on sync_point.
  timer_->SleepMs(10);
  sync_point.Notify();
  pool2.ShutDown();

  WaitAndCheckNotFound(n0);
}

TEST_F(AsyncCacheTest, ShutdownQueueWhileBusyWithMultiGet) {
  InitCache(1, 1);
  PopulateCache(1);

  WorkerTestBase::SyncPoint sync_point(thread_system_.get());
  QueuedWorkerPool::Sequence* sequence = pool_->NewSequence();
  sequence->Add(new WorkerTestBase::WaitRunFunction(&sync_point));

  Callback* n0 = AddCallback();
  Callback* not_found = AddCallback();
  Callback* n1 = AddCallback();
  IssueMultiGet(n0, "n0", not_found, "not_found", n1, "n1");

  QueuedWorkerPool pool2(2, thread_system_.get());
  QueuedWorkerPool::Sequence* sequence2 = pool2.NewSequence();
  sequence2->Add(MakeFunction(pool_.get(), &QueuedWorkerPool::ShutDown));

  // We must now wait for the ShutDown sequence be *initiated* on
  // pool_.  However, there is no callback for this at all, and
  // ShutDown itself will block due to the fact that the function
  // currently running in pool_ is blocked.  To resolve this we can
  // add a short sleep, so we are certain we'll get into the ShutDown
  // flow before releaing the sync_point.
  timer_->SleepMs(10);

  sync_point.Notify();
  pool2.ShutDown();

  WaitAndCheckNotFound(n0);
  WaitAndCheckNotFound(not_found);
  WaitAndCheckNotFound(n1);
}

}  // namespace net_instaweb
