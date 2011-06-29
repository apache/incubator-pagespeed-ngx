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
// Author: fangfei@google.com (Fangfei Zhou)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_CIRCULAR_BUFFER_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_CIRCULAR_BUFFER_TEST_BASE_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"

namespace net_instaweb {
class SharedCircularBuffer;

// This TestBase is added to pthread_shared_mem_test
class SharedCircularBufferTestBase : public testing::Test {
 protected:
  typedef void (SharedCircularBufferTestBase::*TestMethod)();

  explicit SharedCircularBufferTestBase(SharedMemTestEnv* test_env);

  bool CreateChild(TestMethod method);

  // Test basic initialization/writing/cleanup.
  void TestCreate();
  // Test writing from child process.
  void TestAdd();
  // Test cleanup from child process.
  void TestClear();
  // Test the shared memory circular buffer.
  void TestCircular();

 private:
  typedef SharedMemTestEnv::MethodCallback<SharedCircularBufferTestBase>
      MethodCallback;

  // Helper functions.
  void TestCreateChild();
  void TestAddChild();
  void TestClearChild();

  // Initialize SharedMemoryCircularBuffer from child process.
  SharedCircularBuffer* ChildInit();
  // Initialize SharedMemoryCircularBuffer from root process.
  SharedCircularBuffer* ParentInit();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  MockMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(SharedCircularBufferTestBase);
};

template<typename ConcreteTestEnv>
class SharedCircularBufferTestTemplate : public SharedCircularBufferTestBase {
 public:
  SharedCircularBufferTestTemplate()
      : SharedCircularBufferTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(SharedCircularBufferTestTemplate);

TYPED_TEST_P(SharedCircularBufferTestTemplate, TestCreate) {
  SharedCircularBufferTestBase::TestCreate();
}

TYPED_TEST_P(SharedCircularBufferTestTemplate, TestAdd) {
  SharedCircularBufferTestBase::TestAdd();
}

TYPED_TEST_P(SharedCircularBufferTestTemplate, TestClear) {
  SharedCircularBufferTestBase::TestClear();
}

TYPED_TEST_P(SharedCircularBufferTestTemplate, TestCircular) {
  SharedCircularBufferTestBase::TestCircular();
}

REGISTER_TYPED_TEST_CASE_P(SharedCircularBufferTestTemplate, TestCreate,
                           TestAdd, TestClear, TestCircular);

}  // namespace net_instaweb
#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_CIRCULAR_BUFFER_TEST_BASE_H_
