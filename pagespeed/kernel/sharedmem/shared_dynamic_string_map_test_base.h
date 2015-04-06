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
// Author: jhoch@google.com (Jason Hoch)

#ifndef PAGESPEED_KERNEL_SHAREDMEM_SHARED_DYNAMIC_STRING_MAP_TEST_BASE_H_
#define PAGESPEED_KERNEL_SHAREDMEM_SHARED_DYNAMIC_STRING_MAP_TEST_BASE_H_

#include <vector>

#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/sharedmem/shared_mem_test_base.h"

namespace net_instaweb {
class SharedDynamicStringMap;
class ThreadSystem;

class SharedDynamicStringMapTestBase : public testing::Test {
 protected:
  typedef void (SharedDynamicStringMapTestBase::*TestMethod0)();
  typedef void (SharedDynamicStringMapTestBase::*TestMethod2)(int, int);

  explicit SharedDynamicStringMapTestBase(SharedMemTestEnv* test_env);

  // Create child process for given method - the latter is used for TestFill
  // methods, which require arguments
  bool CreateChild(TestMethod0 method);
  bool CreateFillChild(TestMethod2 method, int start, int number_of_strings);

  // Test simple functionality using Dump(Writer*)
  void TestSimple();
  // Test the creation and use of a child process.
  void TestCreate();
  // Test the creation and use of two child processes.
  void TestAdd();
  // Test that no unwanted insertions are performed when filling the map 1/4 of
  // the way by checking length of Dump result.
  void TestQuarterFull();
  // Test the filling of the string map.
  void TestFillSingleThread();
  // Test the filling of the string map by more than one thread;
  // no two threads access the same string.
  void TestFillMultipleNonOverlappingThreads();
  // Test the filling of the string map by more than one thread;
  // no two child threads access the same string, but parent process
  // hits strings at the same time as the child threads.
  void TestFillMultipleOverlappingThreads();

 private:
  void AddChild();
  void AddFillChild(int start, int number_of_strings);
  void AddToFullTable();

  // Initialize child process object
  SharedDynamicStringMap* ChildInit();
  // Initialize parent process object
  SharedDynamicStringMap* ParentInit();

  std::vector<GoogleString> strings_;

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(SharedDynamicStringMapTestBase);
};

template<typename ConcreteTestEnv>
class SharedDynamicStringMapTestTemplate
    : public SharedDynamicStringMapTestBase {
 public:
  SharedDynamicStringMapTestTemplate()
      : SharedDynamicStringMapTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(SharedDynamicStringMapTestTemplate);

TYPED_TEST_P(SharedDynamicStringMapTestTemplate, TestSimple) {
  SharedDynamicStringMapTestBase::TestSimple();
}

TYPED_TEST_P(SharedDynamicStringMapTestTemplate, TestCreate) {
  SharedDynamicStringMapTestBase::TestCreate();
}

TYPED_TEST_P(SharedDynamicStringMapTestTemplate, TestAdd) {
  SharedDynamicStringMapTestBase::TestAdd();
}

TYPED_TEST_P(SharedDynamicStringMapTestTemplate, TestQuarterFull) {
  SharedDynamicStringMapTestBase::TestQuarterFull();
}

TYPED_TEST_P(SharedDynamicStringMapTestTemplate, TestFillSingleThread) {
  SharedDynamicStringMapTestBase::TestFillSingleThread();
}

TYPED_TEST_P(SharedDynamicStringMapTestTemplate,
             TestFillMultipleNonOverlappingThreads) {
  SharedDynamicStringMapTestBase::TestFillMultipleNonOverlappingThreads();
}

TYPED_TEST_P(SharedDynamicStringMapTestTemplate,
             TestFillMultipleOverlappingThreads) {
  SharedDynamicStringMapTestBase::TestFillMultipleOverlappingThreads();
}

REGISTER_TYPED_TEST_CASE_P(SharedDynamicStringMapTestTemplate, TestSimple,
                           TestCreate, TestAdd, TestQuarterFull,
                           TestFillSingleThread,
                           TestFillMultipleNonOverlappingThreads,
                           TestFillMultipleOverlappingThreads);

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_SHAREDMEM_SHARED_DYNAMIC_STRING_MAP_TEST_BASE_H_
