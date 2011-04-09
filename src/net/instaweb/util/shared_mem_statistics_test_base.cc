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

#include "net/instaweb/util/public/shared_mem_statistics_test_base.h"

#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

const char kPrefix[] = "/prefix/";
const char kVar1[] = "v1";
const char kVar2[] = "v2";

}  // namespace

SharedMemStatisticsTestBase::SharedMemStatisticsTestBase(
    SharedMemTestEnv* test_env)
    : test_env_(test_env),
      shmem_runtime_(test_env->CreateSharedMemRuntime()) {
}

bool SharedMemStatisticsTestBase::CreateChild(TestMethod method) {
  MethodCallback* callback = new MethodCallback(this, method);
  return test_env_->CreateChild(callback);
}

bool SharedMemStatisticsTestBase::AddVars(SharedMemStatistics* stats) {
  Variable* v1 = stats->AddVariable(kVar1);
  Variable* v2 = stats->AddVariable(kVar2);
  return (v1 != NULL) && (v2 != NULL);
}

SharedMemStatistics* SharedMemStatisticsTestBase::ChildInit() {
  scoped_ptr<SharedMemStatistics> stats(
      new SharedMemStatistics(shmem_runtime_.get(), kPrefix));
  if (!AddVars(stats.get())) {
    test_env_->ChildFailed();
    return NULL;
  }

  stats->InitVariables(false, &handler_);
  return stats.release();
}

SharedMemStatistics* SharedMemStatisticsTestBase::ParentInit() {
  scoped_ptr<SharedMemStatistics> stats(
      new SharedMemStatistics(shmem_runtime_.get(), kPrefix));
  EXPECT_TRUE(AddVars(stats.get()));
  stats->InitVariables(true, &handler_);
  return stats.release();
}

void SharedMemStatisticsTestBase::TestCreate() {
  // Basic initialization/reading/cleanup test
  scoped_ptr<SharedMemStatistics> stats(ParentInit());

  Variable* v1 = stats->GetVariable(kVar1);
  Variable* v2 = stats->GetVariable(kVar2);
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v2->Get());

  ASSERT_TRUE(CreateChild(&SharedMemStatisticsTestBase::TestCreateChild));
  test_env_->WaitForChildren();

  stats->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedMemStatisticsTestBase::TestCreateChild() {
  scoped_ptr<SharedMemStatistics> stats(ChildInit());

  Variable* v1 = stats->GetVariable(kVar1);
  stats->InitVariables(false, &handler_);
  Variable* v2 = stats->GetVariable(kVar2);
  // We create one var before SHM attach, one after for test coverage.

  if (v1->Get() != 0) {
    test_env_->ChildFailed();
  }

  if (v2->Get() != 0) {
    test_env_->ChildFailed();
  }
}

void SharedMemStatisticsTestBase::TestSet() {
  // -> Set works as well, propagates right
  scoped_ptr<SharedMemStatistics> stats(ParentInit());

  Variable* v1 = stats->GetVariable(kVar1);
  Variable* v2 = stats->GetVariable(kVar2);
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v2->Get());
  v1->Set(3);
  v2->Set(17);
  EXPECT_EQ(3, v1->Get());
  EXPECT_EQ(17, v2->Get());

  ASSERT_TRUE(CreateChild(&SharedMemStatisticsTestBase::TestSetChild));
  test_env_->WaitForChildren();
  EXPECT_EQ(3*3, v1->Get());
  EXPECT_EQ(17*17, v2->Get());

  stats->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedMemStatisticsTestBase::TestSetChild() {
  scoped_ptr<SharedMemStatistics> stats(ChildInit());

  Variable* v1 = stats->GetVariable(kVar1);
  stats->InitVariables(false, &handler_);
  Variable* v2 = stats->GetVariable(kVar2);

  v1->Set(v1->Get() * v1->Get());
  v2->Set(v2->Get() * v2->Get());
}

void SharedMemStatisticsTestBase::TestClear() {
  // We can clear things from the kid
  scoped_ptr<SharedMemStatistics> stats(ParentInit());

  Variable* v1 = stats->GetVariable(kVar1);
  Variable* v2 = stats->GetVariable(kVar2);
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v2->Get());
  v1->Set(3);
  v2->Set(17);
  EXPECT_EQ(3, v1->Get());
  EXPECT_EQ(17, v2->Get());

  ASSERT_TRUE(CreateChild(&SharedMemStatisticsTestBase::TestClearChild));
  test_env_->WaitForChildren();
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v2->Get());

  stats->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedMemStatisticsTestBase::TestClearChild() {
  scoped_ptr<SharedMemStatistics> stats(ChildInit());
  stats->InitVariables(false, &handler_);
  stats->Clear();
}

void SharedMemStatisticsTestBase::TestAdd() {
  scoped_ptr<SharedMemStatistics> stats(ParentInit());

  Variable* v1 = stats->GetVariable(kVar1);
  Variable* v2 = stats->GetVariable(kVar2);
  EXPECT_EQ(0, v1->Get());
  EXPECT_EQ(0, v2->Get());
  v1->Set(3);
  v2->Set(17);
  EXPECT_EQ(3, v1->Get());
  EXPECT_EQ(17, v2->Get());

  // We will add 10x 1 to v1, and 10x 2 to v2
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(CreateChild(&SharedMemStatisticsTestBase::TestAddChild));
  }
  test_env_->WaitForChildren();
  EXPECT_EQ( 3 + 10 * 1, v1->Get());
  EXPECT_EQ(17 + 10 * 2, v2->Get());

  GoogleString dump;
  StringWriter writer(&dump);
  stats->Dump(&writer, &handler_);
  EXPECT_EQ("v1: 13\nv2: 37\n", dump);

  stats->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedMemStatisticsTestBase::TestAddChild() {
  scoped_ptr<SharedMemStatistics> stats(ChildInit());
  stats->InitVariables(false, &handler_);
  Variable* v1 = stats->GetVariable(kVar1);
  Variable* v2 = stats->GetVariable(kVar2);
  v1->Add(1);
  v2->Add(2);
}

}  // namespace net_instaweb
