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
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/shared_circular_buffer_test_base.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"

namespace net_instaweb {

namespace {
const int kBufferSize = 10;
const char kPrefix[] = "/prefix/";
const char kString[] = "012";
}  // namespace

SharedCircularBufferTestBase::SharedCircularBufferTestBase(
    SharedMemTestEnv* test_env)
    : test_env_(test_env),
      shmem_runtime_(test_env->CreateSharedMemRuntime()) {
}

bool SharedCircularBufferTestBase::CreateChild(TestMethod method) {
  MethodCallback* callback = new MethodCallback(this, method);
  return test_env_->CreateChild(callback);
}

SharedCircularBuffer* SharedCircularBufferTestBase::ChildInit() {
  SharedCircularBuffer* buff =
      new SharedCircularBuffer(kBufferSize, shmem_runtime_.get(), kPrefix);
  buff->InitSegment(false, &handler_);
  return buff;
}

SharedCircularBuffer* SharedCircularBufferTestBase::ParentInit() {
  SharedCircularBuffer* buff =
      new SharedCircularBuffer(kBufferSize, shmem_runtime_.get(), kPrefix);
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

// Check if the circular buffer works well when two processes
// write to it alternatively.
void SharedCircularBufferTestBase::TestCircular() {
  scoped_ptr<SharedCircularBuffer> parent(ParentInit());
  scoped_ptr<SharedCircularBuffer> child(ChildInit());
  parent->Write("012345");
  EXPECT_EQ("012345", parent->ToString(&handler_));
  child->Write("67");
  EXPECT_EQ("01234567", child->ToString(&handler_));
  parent->Write("89");
  // Buffer size is 10. It should be filled exactly so far.
  EXPECT_EQ("0123456789", parent->ToString(&handler_));
  // Lose the first char.
  child->Write("a");
  EXPECT_EQ("123456789a", child->ToString(&handler_));
  // Write a message with length larger than buffer.
  parent->Write("bcdefghijkl");
  EXPECT_EQ("cdefghijkl", parent->ToString(&handler_));
}

}  // namespace net_instaweb
