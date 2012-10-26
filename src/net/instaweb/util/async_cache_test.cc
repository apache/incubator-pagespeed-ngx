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
#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/basictypes.h"
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
          delay_map_(delay_map),
          sync_point_(NULL) {
      set_is_healthy(true);
    }
    virtual ~SyncedLRUCache() {}

    void set_sync_point(WorkerTestBase::SyncPoint* x) { sync_point_ = x; }

    void Get(const GoogleString& key, Callback* callback) {
      if (sync_point_ != NULL) {
        sync_point_->Notify();
      }
      delay_map_->Wait(key);
      ThreadsafeCache::Get(key, callback);
    }

    virtual bool IsHealthy() const { return is_healthy_.value(); }
    void set_is_healthy(bool x) { is_healthy_.set_value(x); }

   private:
    DelayMap* delay_map_;
    WorkerTestBase::SyncPoint* sync_point_;
    AtomicBool is_healthy_;

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
        suppress_post_get_cleanup_(false),
        synced_lru_cache_(NULL),
        expected_outstanding_operations_(0) {
    set_mutex(thread_system_->NewMutex());
    pool_.reset(new QueuedWorkerPool(1, thread_system_.get()));
    synced_lru_cache_ = new SyncedLRUCache(
        &delay_map_, lru_cache_, thread_system_->NewMutex());
    async_cache_.reset(new AsyncCache(synced_lru_cache_, pool_.get()));
  }

  ~AsyncCacheTest() {
    pool_->ShutDown();  // quiesce before destructing cache.
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
    while (async_cache_->outstanding_operations() >
           expected_outstanding_operations_) {
      timer_->SleepMs(1);
    }
  }

  void DelayKey(const GoogleString& key) {
    delay_map_.Delay(key);
    ++expected_outstanding_operations_;
  }

  void ReleaseKey(const GoogleString& key) {
    delay_map_.Notify(key);
    --expected_outstanding_operations_;
  }

  // Delays the specified key, and initiates a Get, waiting for the
  // Get to be initiated prior to the callback being called.
  Callback* InitiateDelayedGet(const GoogleString& key) {
    WorkerTestBase::SyncPoint sync_point(thread_system_.get());
    DelayKey(key);
    synced_lru_cache_->set_sync_point(&sync_point);
    Callback* callback = InitiateGet(key);
    sync_point.Wait();
    synced_lru_cache_->set_sync_point(NULL);
    return callback;
  }

  LRUCache* lru_cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  DelayMap delay_map_;
  scoped_ptr<Timer> timer_;
  scoped_ptr<QueuedWorkerPool> pool_;
  scoped_ptr<AsyncCache> async_cache_;
  bool suppress_post_get_cleanup_;
  SyncedLRUCache* synced_lru_cache_;
  int32 expected_outstanding_operations_;
};

// In this version, no keys are delayed, so AsyncCache will not
// introduce parallelism.  This test is copied from lru_cache_test.cc.
// Note that we are going through the AsyncCache/ThreadsafeCache but
// the LRUCache should be quiescent every time we look directly at it.
//
// TODO(jmarantz): refactor this with LRUCacheTest::PutGetDelete.
TEST_F(AsyncCacheTest, PutGetDelete) {
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
  SharedString value_buffer;
  CheckNotFound("Name");
  EXPECT_EQ(static_cast<size_t>(0), lru_cache_->size_bytes());
  EXPECT_EQ(static_cast<size_t>(0), lru_cache_->num_elements());
  lru_cache_->SanityCheck();
}

TEST_F(AsyncCacheTest, DelayN0NoParallelism) {
  PopulateCache(4);  // Inserts "n0"->"v0", "n1"->"v1", "n2"->"v2", "n3"->"v3".

  Callback* n0 = InitiateDelayedGet("n0");
  EXPECT_EQ(1, outstanding_fetches());
  Callback* n1 = InitiateGet("n1");
  EXPECT_EQ(2, outstanding_fetches());
  async_cache_->CancelPendingOperations();
  WaitAndCheckNotFound(n1);
  EXPECT_EQ(1, outstanding_fetches());

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  CheckNotFound("not found");
  EXPECT_EQ(0, outstanding_fetches());

  // Further fetches will execute immediately again.
  CheckGet("n3", "v3");
}

TEST_F(AsyncCacheTest, MultiGet) {
  TestMultiGet();
}

TEST_F(AsyncCacheTest, MultiGetDrop) {
  PopulateCache(3);
  Callback* n2 = InitiateDelayedGet("n2");
  Callback* n0 = AddCallback();
  Callback* not_found = AddCallback();
  Callback* n1 = AddCallback();
  IssueMultiGet(n0, "n0", not_found, "not_found", n1, "n1");
  async_cache_->CancelPendingOperations();
  WaitAndCheckNotFound(n0);
  WaitAndCheckNotFound(not_found);
  WaitAndCheckNotFound(n1);

  ReleaseKey("n2");
  WaitAndCheck(n2, "v2");
}

TEST_F(AsyncCacheTest, StopGets) {
  PopulateCache(1);
  CheckGet("n0", "v0");
  async_cache_->StopCacheActivity();
  suppress_post_get_cleanup_ = true;  // avoid blocking waiting for delayed n0.
  CheckNotFound("n0");
  suppress_post_get_cleanup_ = false;
}

TEST_F(AsyncCacheTest, ShutdownQueue) {
  PopulateCache(1);
  pool_->ShutDown();
  CheckNotFound("n0");
}

TEST_F(AsyncCacheTest, ShutdownQueueWhileBusy) {
  PopulateCache(1);

  Callback* n0 = InitiateDelayedGet("n0");
  Callback* n1 = InitiateGet("n1");
  pool_->InitiateShutDown();
  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheckNotFound(n1);

  pool_->WaitForShutDownComplete();
}

TEST_F(AsyncCacheTest, ShutdownQueueWhileBusyWithMultiGet) {
  PopulateCache(3);

  Callback* n0 = InitiateDelayedGet("n0");
  Callback* n1 = AddCallback();
  Callback* not_found = AddCallback();
  Callback* n2 = AddCallback();
  IssueMultiGet(n1, "n1", not_found, "not_found", n2, "n2");
  pool_->InitiateShutDown();
  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheckNotFound(n1);
  WaitAndCheckNotFound(not_found);
  WaitAndCheckNotFound(n2);

  pool_->WaitForShutDownComplete();
}

TEST_F(AsyncCacheTest, NoPutsOnSickServer) {
  synced_lru_cache_->set_is_healthy(false);
  PopulateCache(3);
  synced_lru_cache_->set_is_healthy(true);
  CheckNotFound("n0");
}

TEST_F(AsyncCacheTest, NoGetsOnSickServer) {
  PopulateCache(3);
  CheckGet("n0", "v0");
  synced_lru_cache_->set_is_healthy(false);
  CheckNotFound("n0");
}

TEST_F(AsyncCacheTest, NoMultiGetsOnSickServer) {
  PopulateCache(3);
  synced_lru_cache_->set_is_healthy(false);
  Callback* n0 = AddCallback();
  Callback* not_found = AddCallback();
  Callback* n1 = AddCallback();
  IssueMultiGet(n0, "n0", not_found, "not_found", n1, "n1");
  WaitAndCheckNotFound(n0);
  WaitAndCheckNotFound(not_found);
  WaitAndCheckNotFound(n1);
}

TEST_F(AsyncCacheTest, NoDeletesOnSickServer) {
  PopulateCache(3);
  CheckGet("n0", "v0");
  synced_lru_cache_->set_is_healthy(false);
  CheckDelete("n0");
  synced_lru_cache_->set_is_healthy(true);
  CheckGet("n0", "v0");
}

TEST_F(AsyncCacheTest, CancelOutstandingDeletes) {
  PopulateCache(3);
  Callback* n0 = InitiateDelayedGet("n0");
  ++expected_outstanding_operations_;  // Delete will be blocked.
  CheckDelete("n1");
  async_cache_->CancelPendingOperations();  // Delete will not happen.
  --expected_outstanding_operations_;  // Delete was canceled.
  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  CheckGet("n1", "v1");   // works because the delete did not happen.
}

TEST_F(AsyncCacheTest, DeleteNotQueuedOnSickServer) {
  PopulateCache(3);
  Callback* n0 = InitiateDelayedGet("n0");
  synced_lru_cache_->set_is_healthy(false);
  CheckDelete("n1");
  synced_lru_cache_->set_is_healthy(true);
  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  CheckGet("n1", "v1");   // works because the delete did not happen.
}

TEST_F(AsyncCacheTest, PutNotQueuedOnSickServer) {
  PopulateCache(3);
  Callback* n0 = InitiateDelayedGet("n0");
  synced_lru_cache_->set_is_healthy(false);
  CheckPut("n1", "new value for n1");
  synced_lru_cache_->set_is_healthy(true);
  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  CheckGet("n1", "v1");   // still "v1" not "new value for n1"
}

TEST_F(AsyncCacheTest, GetNotQueuedOnSickServer) {
  PopulateCache(3);
  Callback* n0 = InitiateDelayedGet("n0");
  synced_lru_cache_->set_is_healthy(false);
  Callback* n1 = InitiateGet("n1");
  synced_lru_cache_->set_is_healthy(true);
  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheckNotFound(n1);  // 'Get' was never queued cause server was sick.
}

TEST_F(AsyncCacheTest, MultiGetNotQueuedOnSickServer) {
  PopulateCache(3);
  Callback* n0 = InitiateDelayedGet("n0");
  synced_lru_cache_->set_is_healthy(false);
  Callback* n1 = AddCallback();
  Callback* not_found = AddCallback();
  Callback* n2 = AddCallback();
  IssueMultiGet(n1, "n1", not_found, "not_found", n2, "n2");
  synced_lru_cache_->set_is_healthy(true);
  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");
  WaitAndCheckNotFound(n1);          // 'MultiGet' was never queued cause server
  WaitAndCheckNotFound(not_found);   //  was sick.
  WaitAndCheckNotFound(n2);
}

TEST_F(AsyncCacheTest, RetireOldOperations) {
  PopulateCache(4);
  Callback* n0 = InitiateDelayedGet("n0");

  // Now the AsyncCache is stuck.  While it's stuck, add in 4 operations which
  // are all destined to fail.  Here's a MultiGet and a Get which will all get
  // a miss.
  Callback* n1 = AddCallback();
  Callback* not_found = AddCallback();
  Callback* n2 = AddCallback();
  ++expected_outstanding_operations_;  // MultiGet will be blocked.
  IssueMultiGet(n1, "n1", not_found, "not_found", n2, "n2");
  ++expected_outstanding_operations_;
  Callback* n3 = InitiateGet("n3");

  ++expected_outstanding_operations_;
  CheckDelete("n1");

  ++expected_outstanding_operations_;
  CheckPut("n5", "v5");

  // Now make a bunch of new Delete calls which, though ineffective, will push
  // the above operations out of the FIFO causing them to fail.
  for (int64 i = 0; i < AsyncCache::kMaxQueueSize; ++i) {
    ++expected_outstanding_operations_;  // The deletes are blocked.
    CheckDelete("no such key anyway");
  }

  ReleaseKey("n0");
  WaitAndCheck(n0, "v0");

  // The bogus Deletes have pushed all the gets other than n0 off the queue.
  // Because we released the blocking Get that was active ahead of
  // the bogus deletes will all be executed and we should have drained
  // the queue.
  expected_outstanding_operations_ = 0;
  PostOpCleanup();   // waits for the Deletes to complete.

  // Now see that the MultiGet and Get failed.
  WaitAndCheckNotFound(n1);          // 'MultiGet' was never queued because
  WaitAndCheckNotFound(not_found);   // the server was sick.
  WaitAndCheckNotFound(n2);
  WaitAndCheckNotFound(n3);

  CheckGet("n1", "v1");  // Delete "n1" got dropped.
  CheckNotFound("n5");   // Put "n5", "v5" got dropped.
}

}  // namespace net_instaweb
