/*
 * Copyright 2010 Google Inc.
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

// Unit-test the threadsafe cache.  Creates an LRU-cache first, and then
// wraps a thread-safe cache around that and a mutex

#include "net/instaweb/util/public/threadsafe_cache.h"

#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/thread.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/pthread_thread_system.h"

namespace {
const int kMaxSize = 100;
const int kNumThreads = 4;
const int kNumIters = 10000;
const int kNumInserts = 10;
}

namespace net_instaweb {

class CacheSpammer : public ThreadSystem::Thread {
 public:
  CacheSpammer(ThreadSystem* runtime,
               ThreadSystem::ThreadFlags flags,
               CacheInterface* cache,
               bool expecting_evictions,
               bool do_deletes,
               const char* value_pattern,
               int index)
      : Thread(runtime, flags),
        cache_(cache),
        expecting_evictions_(expecting_evictions),
        do_deletes_(do_deletes),
        value_pattern_(value_pattern),
        index_(index) {
  }

 protected:
  virtual void Run() {
    const char name_pattern[] = "name%d";
    SharedString inserts[kNumInserts];
    for (int j = 0; j < kNumInserts; ++j) {
      *inserts[j] = StringPrintf(value_pattern_, j);
    }

    for (int i = 0; i < kNumIters; ++i) {
      for (int j = 0; j < kNumInserts; ++j) {
        cache_->Put(StringPrintf(name_pattern, j), &inserts[j]);
      }
      for (int j = 0; j < kNumInserts; ++j) {
        // Ignore the result.  Thread interactions will make it hard to
        // predict if the Get will succeed or not.
        GoogleString key = StringPrintf(name_pattern, j);
        CacheTestBase::Callback callback;
        cache_->Get(key, &callback);
        bool found = (callback.called_ &&
                      (callback.state_ == CacheInterface::kAvailable));

        // We cannot assume that a Get succeeds if there are evictions
        // or deletions going on.  But we are still verifying that the code
        // will not crash, and that after the threads have all quiesced,
        // that the cache is still sane.
        EXPECT_TRUE(found || expecting_evictions_ || do_deletes_)
            << "Failed on key " << key << " i=" << i << " j=" << j
            << " thread=" << index_;
      }
      if (do_deletes_) {
        for (int j = 0; j < kNumInserts; ++j) {
          cache_->Delete(StringPrintf(name_pattern, j));
        }
      }
    }
  }

 private:
  CacheInterface* cache_;
  bool expecting_evictions_;
  bool do_deletes_;
  const char* value_pattern_;
  int index_;

  DISALLOW_COPY_AND_ASSIGN(CacheSpammer);
};

class ThreadsafeCacheTest : public testing::Test {
 protected:
  ThreadsafeCacheTest()
      : lru_cache_(new LRUCache(kMaxSize)),
        mutex_(thread_runtime_.NewMutex()),
        threadsafe_cache_(lru_cache_, mutex_.get()) {
  }

  void TestHelper(bool expecting_evictions, bool do_deletes,
                  const char* value_pattern) {
    CacheSpammer* spammers[kNumThreads];

    // First, create all the threads.
    for (int i = 0; i < kNumThreads; ++i) {
      spammers[i] = new CacheSpammer(
          &thread_runtime_, ThreadSystem::kJoinable,
          &threadsafe_cache_,  // &lru_cache_ will make this fail.
          expecting_evictions, do_deletes, value_pattern, i);
    }

    // Then, start them.
    for (int i = 0; i < kNumThreads; ++i) {
      spammers[i]->Start();
    }

    // Finally, wait for them to complete by joining them.
    for (int i = 0; i < kNumThreads; ++i) {
      spammers[i]->Join();
      delete spammers[i];
    }
    lru_cache_->SanityCheck();
  }

  LRUCache* lru_cache_;
  PthreadThreadSystem thread_runtime_;
  scoped_ptr<AbstractMutex> mutex_;
  ThreadsafeCache threadsafe_cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadsafeCacheTest);
};

TEST_F(ThreadsafeCacheTest, BasicOperation) {
  SharedString put_buffer("val");
  threadsafe_cache_.Put("key", &put_buffer);
  CacheTestBase::Callback callback;
  threadsafe_cache_.Get("key", &callback);
  EXPECT_TRUE(callback.called_);
  EXPECT_EQ(CacheInterface::kAvailable, callback.state_);
  EXPECT_EQ(GoogleString("val"), *callback.value()->get());
}

TEST_F(ThreadsafeCacheTest, SpamCacheNoEvictionsOrDeletions) {
  // By writing 10 inserts, with 5 bytes of value "valu%d" plus 5 bytes
  // of key, we should never evict anything.  In this test the
  // threads can each check that all their Gets succeed.
  TestHelper(false, false, "valu%d");
}

TEST_F(ThreadsafeCacheTest, SpamCacheWithEvictions) {
  // By writing 10 inserts, with 6 bytes of value "value%d" plus 5
  // bytes of key, we may get evictions.  In this test the threads
  // ignores the return value from Get, but we ensure that the
  // eviction logic in the cache is tested in a multi-threaded context.
  TestHelper(true, false, "value%d");
}

TEST_F(ThreadsafeCacheTest, SpamCacheWithDeletions) {
  // In this testcase, we expect no evictions, but we will be
  // doing some deletions, so we do not require Gets to succeed.
  TestHelper(false, true, "valu%d");
}

TEST_F(ThreadsafeCacheTest, SpamCacheWithDeletionsAndEvictions) {
  // In this testcase, we expect evictions, and we will also be
  // doing deletions.
  TestHelper(true, true, "value%d");
}

}  // namespace net_instaweb
