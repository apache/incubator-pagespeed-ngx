// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: morlovich@google.com (Maksim Orlovich)

#ifndef PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_CACHE_TEST_BASE_H_
#define PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_CACHE_TEST_BASE_H_

#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/sharedmem/shared_mem_cache.h"
#include "pagespeed/kernel/sharedmem/shared_mem_test_base.h"
#include "pagespeed/kernel/thread/slow_worker.h"
#include "pagespeed/kernel/util/simple_stats.h"

namespace net_instaweb {

class SharedMemCacheDump;
class ThreadSystem;

class SharedMemCacheTestBase : public CacheTestBase {
 protected:
  typedef void (SharedMemCacheTestBase::*TestMethod)();
  static const int kBlockSize = 512;

  explicit SharedMemCacheTestBase(SharedMemTestEnv* test_env);

  virtual void TearDown();

  virtual SharedMemCache<kBlockSize>* Cache() { return cache_.get(); }
  virtual void SanityCheck();

  void TestBasic();
  void TestReinsert();
  void TestReplacement();
  void TestReaderWriter();
  void TestConflict();
  void TestEvict();
  void TestSnapshot();
  void TestRegisterSnapshotFileCache();
  void TestCheckpointAndRestore();

  void ResetCache();

 private:
  bool CreateChild(TestMethod method);

  void CheckDumpsEqual(const SharedMemCacheDump& a,
                       const SharedMemCacheDump& b,
                       const char* test_label);

  SharedMemCache<kBlockSize>* MakeCache();
  void CheckDelete(const char* key);
  void TestReaderWriterChild();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  scoped_ptr<SharedMemCache<kBlockSize> > cache_;
  MD5Hasher hasher_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockMessageHandler handler_;
  MockTimer timer_;

  GoogleString large_;
  GoogleString gigantic_;

  bool sanity_checks_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemCacheTestBase);
};

class FileCacheTestWrapper {
 public:
  FileCacheTestWrapper(const GoogleString& path,
                       ThreadSystem* thread_system,
                       Timer* timer,
                       MessageHandler* handler) {
    filesystem_.reset(new MemFileSystem(thread_system, timer));
    worker_.reset(new SlowWorker("slow worker", thread_system));
    stats_.reset(new SimpleStats(thread_system));
    FileCache::InitStats(stats_.get());
    hasher_.reset(new MD5Hasher());
    file_cache_.reset(new FileCache(
        path, filesystem_.get(), thread_system, worker_.get(),
        new FileCache::CachePolicy(timer, hasher_.get(),
                                   20*60*1000,  // Clean every 20min.
                                   10*1024*1024,  // 10Mb max size.
                                   1024*1024),  // Allow 1M files.
        stats_.get(), handler));
  }
  ~FileCacheTestWrapper() {}

  FileCache* file_cache() {
    return file_cache_.get();
  }
  MemFileSystem* filesystem() {
    return filesystem_.get();
  }

 private:
  scoped_ptr<MemFileSystem> filesystem_;
  scoped_ptr<SlowWorker> worker_;
  scoped_ptr<SimpleStats> stats_;
  scoped_ptr<MD5Hasher> hasher_;
  scoped_ptr<FileCache> file_cache_;
};

template<typename ConcreteTestEnv>
class SharedMemCacheTestTemplate : public SharedMemCacheTestBase {
 public:
  SharedMemCacheTestTemplate()
      : SharedMemCacheTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(SharedMemCacheTestTemplate);

TYPED_TEST_P(SharedMemCacheTestTemplate, TestBasic) {
  SharedMemCacheTestBase::TestBasic();
}

TYPED_TEST_P(SharedMemCacheTestTemplate, TestReinsert) {
  SharedMemCacheTestBase::TestReinsert();
}

TYPED_TEST_P(SharedMemCacheTestTemplate, TestReplacement) {
  SharedMemCacheTestBase::TestReplacement();
}

TYPED_TEST_P(SharedMemCacheTestTemplate, TestReaderWriter) {
  SharedMemCacheTestBase::TestReaderWriter();
}

TYPED_TEST_P(SharedMemCacheTestTemplate, TestConflict) {
  SharedMemCacheTestBase::TestConflict();
}

TYPED_TEST_P(SharedMemCacheTestTemplate, TestEvict) {
  SharedMemCacheTestBase::TestEvict();
}

TYPED_TEST_P(SharedMemCacheTestTemplate, TestSnapshot) {
  SharedMemCacheTestBase::TestSnapshot();
}

TYPED_TEST_P(SharedMemCacheTestTemplate, TestRegisterSnapshotFileCache) {
  SharedMemCacheTestBase::TestRegisterSnapshotFileCache();
}

TYPED_TEST_P(SharedMemCacheTestTemplate, TestCheckpointAndRestore) {
  SharedMemCacheTestBase::TestCheckpointAndRestore();
}

REGISTER_TYPED_TEST_CASE_P(SharedMemCacheTestTemplate, TestBasic, TestReinsert,
                           TestReplacement, TestReaderWriter, TestConflict,
                           TestEvict, TestSnapshot,
                           TestRegisterSnapshotFileCache,
                           TestCheckpointAndRestore);

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_CACHE_TEST_BASE_H_
