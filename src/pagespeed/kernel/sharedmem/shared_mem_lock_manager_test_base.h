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

#ifndef PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_LOCK_MANAGER_TEST_BASE_H_
#define PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_LOCK_MANAGER_TEST_BASE_H_

#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/sharedmem/shared_mem_lock_manager.h"
#include "pagespeed/kernel/sharedmem/shared_mem_test_base.h"
#include "pagespeed/kernel/thread/mock_scheduler.h"

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
  bool CreateChild(TestMethod method);

  SharedMemLockManager* CreateLockManager();
  SharedMemLockManager* AttachDefault();

  void TestBasicChild();
  void TestStealChild();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;   // Note: if we are running in a process-based environment
                      // this object is not shared at all; therefore all time
                      // advancement must be done in either parent or kid but
                      // not both.
  MockMessageHandler handler_;
  MockScheduler scheduler_;
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

#endif  // PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_LOCK_MANAGER_TEST_BASE_H_
