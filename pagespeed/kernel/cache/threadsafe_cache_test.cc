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

#include "pagespeed/kernel/cache/threadsafe_cache.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/cache/cache_spammer.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/util/platform.h"

namespace {
const int kMaxSize = 100;
const int kNumThreads = 4;
const int kNumIters = 10000;
const int kNumInserts = 10;
}

namespace net_instaweb {

class ThreadsafeCacheTest : public testing::Test {
 protected:
  ThreadsafeCacheTest()
      : lru_cache_(new LRUCache(kMaxSize)),
        thread_runtime_(Platform::CreateThreadSystem()),
        threadsafe_cache_(lru_cache_.get(), thread_runtime_->NewMutex()) {
  }

  void TestHelper(bool expecting_evictions, bool do_deletes,
                  const char* value_pattern) {
    CacheSpammer::RunTests(kNumThreads, kNumIters, kNumInserts,
                           expecting_evictions, do_deletes, value_pattern,
                           &threadsafe_cache_, thread_runtime_.get());
    lru_cache_->SanityCheck();
  }

  scoped_ptr<LRUCache> lru_cache_;
  scoped_ptr<ThreadSystem> thread_runtime_;
  ThreadsafeCache threadsafe_cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadsafeCacheTest);
};

TEST_F(ThreadsafeCacheTest, BasicOperation) {
  SharedString put_buffer("val");
  threadsafe_cache_.Put("key", &put_buffer);
  CacheTestBase::Callback callback;
  threadsafe_cache_.Get("key", &callback);
  EXPECT_TRUE(callback.called());
  EXPECT_EQ(CacheInterface::kAvailable, callback.state());
  EXPECT_EQ(GoogleString("val"), callback.value()->Value());
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

TEST_F(ThreadsafeCacheTest, Backend) {
  EXPECT_EQ(lru_cache_.get(), threadsafe_cache_.Backend());
}

}  // namespace net_instaweb
