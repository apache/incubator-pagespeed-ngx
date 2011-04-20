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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_LOCK_MANAGER_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_LOCK_MANAGER_TEST_BASE_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/shared_mem_lock_manager.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"

namespace net_instaweb {

class SharedMemLockManagerTestBase : public testing::Test {
 protected:
  typedef void (SharedMemLockManagerTestBase::*TestMethod)();

  explicit SharedMemLockManagerTestBase(SharedMemTestEnv* test_env);
  virtual void SetUp();
  virtual void TearDown();

  void TestBasic();
  void TestDestructorUnlock();
  void TestSteal();

 private:
  typedef SharedMemTestEnv::MethodCallback<SharedMemLockManagerTestBase>
      MethodCallback;
  bool CreateChild(TestMethod method);

  SharedMemLockManager* CreateLockManager();
  SharedMemLockManager* AttachDefault();

  void TestBasicChild();
  void TestStealChild();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  MockMessageHandler handler_;
  MockTimer timer_;   // note: this is thread-unsafe, and if we are running in
                      // a process-based environment it's not shared at all.
                      // Therefore,  all advancement must be done in either
                      // parent or kid but not both.
  MD5Hasher hasher_;
  scoped_ptr<SharedMemLockManager> root_lock_manager_;  // used for init only.

  DISALLOW_COPY_AND_ASSIGN(SharedMemLockManagerTestBase);
};

template<typename ConcreteTestEnv>
class SharedMemLockManagerTestTemplate : public SharedMemLockManagerTestBase {
 public:
  SharedMemLockManagerTestTemplate()
      : SharedMemLockManagerTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(SharedMemLockManagerTestTemplate);

TYPED_TEST_P(SharedMemLockManagerTestTemplate, TestBasic) {
  SharedMemLockManagerTestBase::TestBasic();
}

TYPED_TEST_P(SharedMemLockManagerTestTemplate, TestDestructorUnlock) {
  SharedMemLockManagerTestBase::TestDestructorUnlock();
}

TYPED_TEST_P(SharedMemLockManagerTestTemplate, TestSteal) {
  SharedMemLockManagerTestBase::TestSteal();
}

REGISTER_TYPED_TEST_CASE_P(SharedMemLockManagerTestTemplate, TestBasic,
                           TestDestructorUnlock, TestSteal);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_LOCK_MANAGER_TEST_BASE_H_
