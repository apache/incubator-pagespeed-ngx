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

#ifndef NET_INSTAWEB_UTIL_SHARED_MEM_CACHE_DATA_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_SHARED_MEM_CACHE_DATA_TEST_BASE_H_

#include <vector>

#include "net/instaweb/util/shared_mem_cache_data.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/shared_mem_cache.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"

namespace net_instaweb {

class SharedMemCacheDataTestBase : public testing::Test {
 protected:
  typedef void (SharedMemCacheDataTestBase::*TestMethod)();
  enum { kBlockSize = 512 };  // Can't use static const int here since it's
                              // passed to EXPECT_EQ

  explicit SharedMemCacheDataTestBase(SharedMemTestEnv* test_env);

  void TestFreeList();
  void TestLRU();
  void TestBlockLists();

 private:
  bool CreateChild(TestMethod method);

  void SanityCheckBlockVector(const SharedMemCacheData::BlockVector& blocks,
                              int min_valid, int max_valid);

  void ExtractAndSanityCheckLRU(
      SharedMemCacheData::Sector<kBlockSize>* sector,
      std::vector<SharedMemCacheData::EntryNum>* out_lru);

  void TestFreeListChild();

  bool ParentInit(AbstractSharedMemSegment** out_seg,
                  SharedMemCacheData::Sector<kBlockSize>** out_sector);

  bool ChildInit(AbstractSharedMemSegment** out_seg,
                 SharedMemCacheData::Sector<kBlockSize>** out_sector);

  void ParentCleanup();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  MockMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemCacheDataTestBase);
};

template<typename ConcreteTestEnv>
class SharedMemCacheDataTestTemplate : public SharedMemCacheDataTestBase {
 public:
  SharedMemCacheDataTestTemplate()
      : SharedMemCacheDataTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(SharedMemCacheDataTestTemplate);

TYPED_TEST_P(SharedMemCacheDataTestTemplate, TestFreeList) {
  SharedMemCacheDataTestBase::TestFreeList();
}

TYPED_TEST_P(SharedMemCacheDataTestTemplate, TestLRU) {
  SharedMemCacheDataTestBase::TestLRU();
}

TYPED_TEST_P(SharedMemCacheDataTestTemplate, TestBlockLists) {
  SharedMemCacheDataTestBase::TestBlockLists();
}

REGISTER_TYPED_TEST_CASE_P(SharedMemCacheDataTestTemplate, TestFreeList,
                           TestLRU, TestBlockLists);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_SHARED_MEM_CACHE_DATA_TEST_BASE_H_
