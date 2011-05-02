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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_STATISTICS_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_STATISTICS_TEST_BASE_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"

namespace net_instaweb {
class SharedMemStatistics;

class SharedMemStatisticsTestBase : public testing::Test {
 protected:
  typedef void (SharedMemStatisticsTestBase::*TestMethod)();

  explicit SharedMemStatisticsTestBase(SharedMemTestEnv* test_env);

  bool CreateChild(TestMethod method);

  void TestCreate();
  void TestSet();
  void TestClear();
  void TestAdd();

 private:
  typedef SharedMemTestEnv::MethodCallback<SharedMemStatisticsTestBase>
      MethodCallback;

  void TestCreateChild();
  void TestSetChild();
  void TestClearChild();

  // Adds 10x +1 to variable 1, and 10x +2 to variable 2.
  void TestAddChild();

  bool AddVars(SharedMemStatistics* stats);

  SharedMemStatistics* ChildInit();
  SharedMemStatistics* ParentInit();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  MockMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemStatisticsTestBase);
};

template<typename ConcreteTestEnv>
class SharedMemStatisticsTestTemplate : public SharedMemStatisticsTestBase {
 public:
  SharedMemStatisticsTestTemplate()
      : SharedMemStatisticsTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(SharedMemStatisticsTestTemplate);

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestCreate) {
  SharedMemStatisticsTestBase::TestCreate();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestSet) {
  SharedMemStatisticsTestBase::TestSet();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestClear) {
  SharedMemStatisticsTestBase::TestClear();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestAdd) {
  SharedMemStatisticsTestBase::TestAdd();
}

REGISTER_TYPED_TEST_CASE_P(SharedMemStatisticsTestTemplate, TestCreate,
                           TestSet, TestClear, TestAdd);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_STATISTICS_TEST_BASE_H_
