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
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {
class SharedMemStatisticsTestBase : public testing::Test {
 protected:
  typedef void (SharedMemStatisticsTestBase::*TestMethod)();

  explicit SharedMemStatisticsTestBase(SharedMemTestEnv* test_env);

  virtual void SetUp();
  virtual void TearDown();

  bool CreateChild(TestMethod method);

  void TestCreate();
  void TestSet();
  void TestClear();
  void TestAdd();
  void TestHistogram();
  void TestHistogramRender();
  void TestHistogramNoExtraClear();
  void TestTimedVariableEmulation();
  void TestConsoleStatisticsLogger();

 private:
  void TestCreateChild();
  void TestSetChild();
  void TestClearChild();
  void TestHistogramNoExtraClearChild();

  // Adds 10x +1 to variable 1, and 10x +2 to variable 2.
  void TestAddChild();
  bool AddVars(SharedMemStatistics* stats);
  bool AddHistograms(SharedMemStatistics* stats);
  // Helper function for TestHistogramRender().
  // Check if string html contains the pattern.
  bool Contains(const StringPiece& html, const StringPiece& pattern);

  SharedMemStatistics* ChildInit();
  void ParentInit();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  MockMessageHandler handler_;
  scoped_ptr<MockTimer> timer_;
  scoped_ptr<MemFileSystem> file_system_;
  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<SharedMemStatistics> stats_;  // (the parent process version)

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

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestHistogram) {
  SharedMemStatisticsTestBase::TestHistogram();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestHistogramRender) {
  SharedMemStatisticsTestBase::TestHistogramRender();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestHistogramNoExtraClear) {
  SharedMemStatisticsTestBase::TestHistogramNoExtraClear();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestTimedVariableEmulation) {
  SharedMemStatisticsTestBase::TestTimedVariableEmulation();
}
/*
 * TODO(bvb, sarahdw): Enable logging for tests.
 * Also add this to REGISTER below.
TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestConsoleStatisticsLogger) {
  SharedMemStatisticsTestBase::TestConsoleStatisticsLogger();
}*/

REGISTER_TYPED_TEST_CASE_P(SharedMemStatisticsTestTemplate, TestCreate,
                           TestSet, TestClear, TestAdd, TestHistogram,
                           TestHistogramRender, TestHistogramNoExtraClear,
                           TestTimedVariableEmulation);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_STATISTICS_TEST_BASE_H_
