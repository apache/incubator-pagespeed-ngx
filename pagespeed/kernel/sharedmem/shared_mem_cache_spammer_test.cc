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

#include "pagespeed/kernel/sharedmem/shared_mem_cache.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/cache/cache_spammer.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/sharedmem/inprocess_shared_mem.h"
#include "pagespeed/kernel/util/platform.h"

namespace {
const int kMaxSize = 100;
const int kNumThreads = 4;
const int kNumIters = 10000;
const int kNumInserts = 10;
const int kSectors = 128;
const char kSegmentName[] = "/shared_cache_spammer_test_segment";
}

namespace net_instaweb {

class SharedMemCacheSpammerTest : public testing::Test {
 protected:
  typedef SharedMemCache<64> ShmCache;

  SharedMemCacheSpammerTest()
      : timer_(Platform::CreateTimer()),
        thread_system_(Platform::CreateThreadSystem()),
        in_process_shared_mem_(thread_system_.get()) {
    cache_.reset(MakeCache());
    CHECK(cache_->Initialize());
    CHECK(cache_->Attach());
  }

  ~SharedMemCacheSpammerTest() {
    ShmCache::GlobalCleanup(&in_process_shared_mem_,
                            kSegmentName,
                            &message_handler_);
  }

  ShmCache* MakeCache() {
    int entries, blocks;
    int64 size_cap;
    ShmCache::ComputeDimensions(
        kMaxSize /* size_kb */,
        2 /* block/entry ratio, based empirically off load tests */,
        kSectors, &entries, &blocks, &size_cap);
    ShmCache* cache = new ShmCache(
        &in_process_shared_mem_,
        kSegmentName,
        timer_.get(),
        &hasher_,
        kSectors,
        entries,  /* entries per sector */
        blocks /* blocks per sector*/,
        &message_handler_);
    return cache;
  }

  void TestHelper(bool expecting_evictions, bool do_deletes,
                  const char* value_pattern) {
    CacheSpammer::RunTests(kNumThreads, kNumIters, kNumInserts,
                           expecting_evictions, do_deletes, value_pattern,
                           cache_.get(),
                           thread_system_.get());
    cache_->SanityCheck();
  }

  scoped_ptr<Timer> timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  GoogleMessageHandler message_handler_;
  MD5Hasher hasher_;
  InProcessSharedMem in_process_shared_mem_;
  scoped_ptr<ShmCache> cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedMemCacheSpammerTest);
};

TEST_F(SharedMemCacheSpammerTest, BasicOperation) {
  SharedString put_buffer("val");
  cache_->Put("key", put_buffer);
  CacheTestBase::Callback callback;
  cache_->Get("key", &callback);
  EXPECT_TRUE(callback.called());
  EXPECT_EQ(CacheInterface::kAvailable, callback.state());
  EXPECT_EQ(GoogleString("val"), callback.value().Value());
}

TEST_F(SharedMemCacheSpammerTest, SpamCacheEvictionsNoDeletions) {
  // By writing 10 inserts, with 5 bytes of value "valu%d" plus 5 bytes
  // of key, we should never evict anything.  In this test the
  // threads can each check that all their Gets succeed.
  //
  // We have expect_evictions set here to true. This is not actually because
  // we expect evictions --- we are inserting just 10 small key/value pairs;
  // but because SharedMemCache::Get happening concurrently to an in-progress
  // Put to the same key will miss.
  TestHelper(true, false, "valu");
}

TEST_F(SharedMemCacheSpammerTest, SpamCacheWithEvictions) {
  // By writing 10 inserts, with 6 bytes of value "value%d" plus 5
  // bytes of key, we may get evictions.  In this test the threads
  // ignores the return value from Get, but we ensure that the
  // eviction logic in the cache is tested in a multi-threaded context.
  TestHelper(true, false, "value");
}

TEST_F(SharedMemCacheSpammerTest, SpamCacheWithDeletions) {
  // In this testcase, we expect no evictions, but we will be
  // doing some deletions, so we do not require Gets to succeed.
  TestHelper(false, true, "valu");
}

TEST_F(SharedMemCacheSpammerTest, SpamCacheWithDeletionsAndEvictions) {
  // In this testcase, we expect evictions, and we will also be
  // doing deletions.
  TestHelper(true, true, "value");
}

}  // namespace net_instaweb
