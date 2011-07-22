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

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/shared_circular_buffer_test_base.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {
const int kBufferSize = 10;
const char kPrefix[] = "/prefix/";
const char kPostfix[] = "postfix";
const char kString[] = "012";
}  // namespace

SharedCircularBufferTestBase::SharedCircularBufferTestBase(
    SharedMemTestEnv* test_env)
    : test_env_(test_env),
      shmem_runtime_(test_env->CreateSharedMemRuntime()) {
}

bool SharedCircularBufferTestBase::CreateChild(TestMethod method) {
  Function* callback =
      new MemberFunction0<SharedCircularBufferTestBase>(method, this);
  return test_env_->CreateChild(callback);
}

SharedCircularBuffer* SharedCircularBufferTestBase::ChildInit() {
  SharedCircularBuffer* buff =
      new SharedCircularBuffer(shmem_runtime_.get(), kBufferSize, kPrefix,
                               kPostfix);
  buff->InitSegment(false, &handler_);
  return buff;
}

SharedCircularBuffer* SharedCircularBufferTestBase::ParentInit() {
  SharedCircularBuffer* buff =
      new SharedCircularBuffer(shmem_runtime_.get(), kBufferSize, kPrefix,
                               kPostfix);
  buff->InitSegment(true, &handler_);
  return buff;
}

// Basic initialization/writing/cleanup test
void SharedCircularBufferTestBase::TestCreate() {
  // Create buffer from root Process.
  scoped_ptr<SharedCircularBuffer> buff(ParentInit());
  buff->Write("parent");
  EXPECT_EQ("parent", buff->ToString(&handler_));
  ASSERT_TRUE(CreateChild(&SharedCircularBufferTestBase::TestCreateChild));
  test_env_->WaitForChildren();
  // After the child process writes to buffer,
  // the content should be updated.
  EXPECT_EQ("parentkid", buff->ToString(&handler_));
  buff->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedCircularBufferTestBase::TestCreateChild() {
  scoped_ptr<SharedCircularBuffer> buff(ChildInit());
  // Child writes to buffer.
  if (!buff->Write("kid")) {
    test_env_->ChildFailed();
  }
}

void SharedCircularBufferTestBase::TestAdd() {
  // Every child process writes "012" to buffer.
  scoped_ptr<SharedCircularBuffer> buff(ParentInit());
  for (int i = 0; i < 2; ++i) {
    ASSERT_TRUE(CreateChild(&SharedCircularBufferTestBase::TestAddChild));
  }
  test_env_->WaitForChildren();
  EXPECT_EQ("012012", buff->ToString(&handler_));

  buff->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedCircularBufferTestBase::TestAddChild() {
  scoped_ptr<SharedCircularBuffer> buff(ChildInit());
  buff->Write("012");
}

void SharedCircularBufferTestBase::TestClear() {
  // We can clear things from the child
  scoped_ptr<SharedCircularBuffer> buff(ParentInit());
  // Write a string to buffer.
  buff->Write("012");
  EXPECT_EQ("012", buff->ToString(&handler_));
  ASSERT_TRUE(CreateChild(&SharedCircularBufferTestBase::TestClearChild));
  test_env_->WaitForChildren();
  // Now the buffer should be empty as the child cleared it.
  EXPECT_EQ("", buff->ToString(&handler_));
  buff->GlobalCleanup(&handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedCircularBufferTestBase::TestClearChild() {
  scoped_ptr<SharedCircularBuffer> buff(ChildInit());
  buff->InitSegment(false, &handler_);
  buff->Clear();
}

void SharedCircularBufferTestBase::TestChildWrite() {
  scoped_ptr<SharedCircularBuffer> buff(ChildInit());
  buff->InitSegment(false, &handler_);
  buff->Write(message_);
}

void SharedCircularBufferTestBase::TestChildBuff() {
  scoped_ptr<SharedCircularBuffer> buff(ChildInit());
  buff->InitSegment(false, &handler_);
  // Check if buffer content is correct.
  if (expected_result_ != buff->ToString(&handler_)) {
    test_env_->ChildFailed();
  }
}

// Check various operations, and wraparound, with multiple processes.
void SharedCircularBufferTestBase::TestCircular() {
  scoped_ptr<SharedCircularBuffer> parent(ParentInit());
  parent->Clear();
  // Write in parent process.
  parent->Write("012345");
  EXPECT_EQ("012345", parent->ToString(&handler_));
  // Write in a child process.
  message_ = "67";
  ASSERT_TRUE(CreateChild(
      &SharedCircularBufferTestBase::TestChildWrite));
  test_env_->WaitForChildren();
  EXPECT_EQ("01234567", parent->ToString(&handler_));
  // Write in parent process.
  parent->Write("89");
  // Check buffer content in a child process.
  // Buffer size is 10. It should be filled exactly so far.
  expected_result_ = "0123456789";
  ASSERT_TRUE(CreateChild(
      &SharedCircularBufferTestBase::TestChildBuff));
  test_env_->WaitForChildren();
  // Lose the first char.
  parent->Write("a");
  EXPECT_EQ("123456789a", parent->ToString(&handler_));
  // Write a message with length larger than buffer.
  parent->Write("bcdefghijkl");
  EXPECT_EQ("cdefghijkl", parent->ToString(&handler_));
  parent->GlobalCleanup(&handler_);
}

}  // namespace net_instaweb
