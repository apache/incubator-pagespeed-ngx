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

#ifndef NET_INSTAWEB_UTIL_SHARED_MEM_CACHE_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_SHARED_MEM_CACHE_TEST_BASE_H_

#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/shared_mem_cache.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class CacheInterface;

class SharedMemCacheTestBase : public CacheTestBase {
 protected:
  typedef void (SharedMemCacheTestBase::*TestMethod)();
  static const int kBlockSize = 512;

  explicit SharedMemCacheTestBase(SharedMemTestEnv* test_env);

  virtual void TearDown();

  virtual CacheInterface* Cache() { return cache_.get(); }
  virtual void SanityCheck();

  void TestBasic();
  void TestReinsert();
  void TestReplacement();
  void TestReaderWriter();
  void TestConflict();
  void TestEvict();

 private:
  bool CreateChild(TestMethod method);

  SharedMemCache<kBlockSize>* MakeCache();
  void CheckDelete(const char* key);
  void TestReaderWriterChild();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  scoped_ptr<SharedMemCache<kBlockSize> > cache_;
  MD5Hasher hasher_;
  MockMessageHandler handler_;
  MockTimer timer_;

  GoogleString large_;
  GoogleString gigantic_;

  bool sanity_checks_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemCacheTestBase);
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

REGISTER_TYPED_TEST_CASE_P(SharedMemCacheTestTemplate, TestBasic, TestReinsert,
                           TestReplacement, TestReaderWriter, TestConflict,
                           TestEvict);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_SHARED_MEM_CACHE_TEST_BASE_H_
