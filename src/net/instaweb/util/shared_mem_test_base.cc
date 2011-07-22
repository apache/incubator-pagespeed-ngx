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

#include "net/instaweb/util/public/shared_mem_test_base.h"

#include <cstddef>
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"

namespace net_instaweb {

namespace {
  const char kTestSegment[] = "segment1";
  const char kOtherSegment[] = "segment2";
}  // namespace

SharedMemTestEnv::~SharedMemTestEnv() {
}

SharedMemTestBase::SharedMemTestBase(SharedMemTestEnv* test_env)
    : test_env_(test_env),
      shmem_runtime_(test_env->CreateSharedMemRuntime()) {
}

bool SharedMemTestBase::CreateChild(TestMethod method) {
  Function* callback = new MemberFunction0<SharedMemTestBase>(method, this);
  return test_env_->CreateChild(callback);
}

void SharedMemTestBase::TestReadWrite(bool reattach) {
  scoped_ptr<AbstractSharedMemSegment> seg(CreateDefault());
  ASSERT_TRUE(seg.get() != NULL);
  ASSERT_TRUE(CreateChild(&SharedMemTestBase::TestReadWriteChild));

  if (reattach) {
    seg.reset(AttachDefault());
  }

  // Wait for kid to write out stuff
  while (*seg->Base() != '1') {
    test_env_->ShortSleep();
  }

  // Write out stuff.
  *seg->Base() = '2';

  // Wait for termination.
  test_env_->WaitForChildren();
  seg.reset(NULL);
  DestroyDefault();
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedMemTestBase::TestReadWriteChild() {
  scoped_ptr<AbstractSharedMemSegment> seg(AttachDefault());

  // Write out '1', which the parent will wait for.
  *seg->Base() = '1';

  // Wait for '2' from parent
  while (*seg->Base() != '2') {
    test_env_->ShortSleep();
  }
}

void SharedMemTestBase::TestLarge() {
  scoped_ptr<AbstractSharedMemSegment> seg(
    shmem_runtime_->CreateSegment(kTestSegment, kLarge, &handler_));
  ASSERT_TRUE(seg.get() != NULL);

  // Make sure everything is zeroed
  for (int c = 0; c < kLarge; ++c) {
    EXPECT_EQ(0, seg->Base()[c]);
  }
  seg.reset(NULL);

  ASSERT_TRUE(CreateChild(&SharedMemTestBase::TestLargeChild));
  test_env_->WaitForChildren();

  seg.reset(shmem_runtime_->AttachToSegment(kTestSegment, kLarge, &handler_));
  for (int i = 0; i < kLarge; i+=4) {
    EXPECT_EQ(i, *IntPtr(seg.get(), i));
  }
}

void SharedMemTestBase::TestLargeChild() {
  scoped_ptr<AbstractSharedMemSegment> seg(
    shmem_runtime_->AttachToSegment(kTestSegment, kLarge, &handler_));
  for (int i = 0; i < kLarge; i+=4) {
    *IntPtr(seg.get(), i) = i;
  }
}

// Make sure that 2 segments don't interfere.
void SharedMemTestBase::TestDistinct() {
  scoped_ptr<AbstractSharedMemSegment> seg(CreateDefault());
  ASSERT_TRUE(seg.get() != NULL);
  scoped_ptr<AbstractSharedMemSegment> seg2(
      shmem_runtime_->CreateSegment(kOtherSegment, 4, &handler_));
  ASSERT_TRUE(seg2.get() != NULL);

  ASSERT_TRUE(CreateChild(&SharedMemTestBase::WriteSeg1Child));
  ASSERT_TRUE(CreateChild(&SharedMemTestBase::WriteSeg2Child));
  test_env_->WaitForChildren();

  EXPECT_EQ('1', *seg->Base());
  EXPECT_EQ('2', *seg2->Base());

  seg.reset(NULL);
  seg2.reset(NULL);
  DestroyDefault();
  shmem_runtime_->DestroySegment(kOtherSegment, &handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

// Make sure destruction destroys things properly...
void SharedMemTestBase::TestDestroy() {
  scoped_ptr<AbstractSharedMemSegment> seg(CreateDefault());
  ASSERT_TRUE(seg.get() != NULL);

  ASSERT_TRUE(CreateChild(&SharedMemTestBase::WriteSeg1Child));
  test_env_->WaitForChildren();
  EXPECT_EQ('1', *seg->Base());

  seg.reset(NULL);
  DestroyDefault();

  // Attach should fail now
  seg.reset(AttachDefault());
  EXPECT_EQ(NULL, seg.get());

  // Newly created one should have zeroed memory
  seg.reset(CreateDefault());
  EXPECT_EQ('\0', *seg->Base());

  DestroyDefault();
}

// Make sure that re-creating a segment without a Destroy is safe and
// produces a distinct segment
void SharedMemTestBase::TestCreateTwice() {
  scoped_ptr<AbstractSharedMemSegment> seg(CreateDefault());
  ASSERT_TRUE(seg.get() != NULL);
  ASSERT_TRUE(CreateChild(&SharedMemTestBase::WriteSeg1Child));
  test_env_->WaitForChildren();
  EXPECT_EQ('1', *seg->Base());

  seg.reset(CreateDefault());
  EXPECT_EQ('\0', *seg->Base());
}

// Make sure between two kids see the SHM as well.
void SharedMemTestBase::TestTwoKids() {
  scoped_ptr<AbstractSharedMemSegment> seg(CreateDefault());
  ASSERT_TRUE(seg.get() != NULL);
  seg.reset(NULL);

  ASSERT_TRUE(CreateChild(&SharedMemTestBase::TwoKidsChild1));
  ASSERT_TRUE(CreateChild(&SharedMemTestBase::TwoKidsChild2));
  test_env_->WaitForChildren();
  seg.reset(AttachDefault());
  EXPECT_EQ('2', *seg->Base());

  DestroyDefault();
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedMemTestBase::TwoKidsChild1() {
  scoped_ptr<AbstractSharedMemSegment> seg(AttachDefault());
  ASSERT_TRUE(seg.get() != NULL);
  // Write out '1', which the other kid will wait for.
  *seg->Base() = '1';
}

void SharedMemTestBase::TwoKidsChild2() {
  scoped_ptr<AbstractSharedMemSegment> seg(AttachDefault());
  ASSERT_TRUE(seg.get() != NULL);
  // Wait for '1'
  while (*seg->Base() != '1') {
    test_env_->ShortSleep();
  }

  *seg->Base() = '2';
}

// Test for mutex operation. This attempts to detect lack of mutual exclusion
// by hammering on a shared location (protected by a lock) with non-atomic
// increments. This test does not guarantee that it will detect a failure
// (the schedule might just end up such that things work out), but it's
// been found to be effective in practice.
void SharedMemTestBase::TestMutex() {
  size_t mutex_size = shmem_runtime_->SharedMutexSize();
  scoped_ptr<AbstractSharedMemSegment> seg(
      shmem_runtime_->CreateSegment(kTestSegment, mutex_size + 4, &handler_));
  ASSERT_TRUE(seg.get() != NULL);
  ASSERT_EQ(mutex_size, seg->SharedMutexSize());

  ASSERT_TRUE(seg->InitializeSharedMutex(0, &handler_));
  seg.reset(
      shmem_runtime_->AttachToSegment(kTestSegment, mutex_size + 4, &handler_));

  scoped_ptr<AbstractMutex> mutex(seg->AttachToSharedMutex(0));
  mutex->Lock();
  ASSERT_TRUE(CreateChild(&SharedMemTestBase::MutexChild));

  // Unblock the kid. Before that, it shouldn't have written
  EXPECT_EQ(0, *IntPtr(seg.get(), mutex_size));
  mutex->Unlock();

  mutex->Lock();
  EXPECT_TRUE(IncrementStorm(seg.get(), mutex_size));
  mutex->Unlock();

  test_env_->WaitForChildren();
  DestroyDefault();
}

void SharedMemTestBase::MutexChild() {
  size_t mutex_size = shmem_runtime_->SharedMutexSize();
  scoped_ptr<AbstractSharedMemSegment> seg(
      shmem_runtime_->AttachToSegment(kTestSegment, mutex_size + 4, &handler_));
  ASSERT_TRUE(seg.get() != NULL);

  scoped_ptr<AbstractMutex> mutex(seg->AttachToSharedMutex(0));
  mutex->Lock();
  if (!IncrementStorm(seg.get(), mutex_size)) {
    mutex->Unlock();
    test_env_->ChildFailed();
    return;
  }
  mutex->Unlock();
}

// Returns if successful
bool SharedMemTestBase::IncrementStorm(AbstractSharedMemSegment* seg,
                                       size_t mutex_size) {
  // We are either the first or second to do the increments.
  int init = *IntPtr(seg, mutex_size);
  if ((init != 0) && (init != kNumIncrements)) {
    return false;
  }

  for (int i = 0; i < kNumIncrements; ++i) {
    ++*IntPtr(seg, mutex_size);
    if (*IntPtr(seg, mutex_size) != (i + init + 1)) {
      return false;
    }
    ++*IntPtr(seg, mutex_size);
    if (*IntPtr(seg, mutex_size) != (i + init + 2)) {
      return false;
    }
    --*IntPtr(seg, mutex_size);
    if (*IntPtr(seg, mutex_size) != (i + init + 1)) {
      return false;
    }
  }

  return true;
}

void SharedMemTestBase::WriteSeg1Child() {
  scoped_ptr<AbstractSharedMemSegment> seg(AttachDefault());
  ASSERT_TRUE(seg.get() != NULL);
  *seg->Base() = '1';
}

void SharedMemTestBase::WriteSeg2Child() {
  scoped_ptr<AbstractSharedMemSegment> seg(
    shmem_runtime_->AttachToSegment(kOtherSegment, 4, &handler_));
  ASSERT_TRUE(seg.get() != NULL);
  *seg->Base() = '2';
}

AbstractSharedMemSegment* SharedMemTestBase::CreateDefault() {
  return shmem_runtime_->CreateSegment(kTestSegment, 4, &handler_);
}

AbstractSharedMemSegment* SharedMemTestBase::AttachDefault() {
  return shmem_runtime_->AttachToSegment(kTestSegment, 4, &handler_);
}

void SharedMemTestBase::DestroyDefault() {
  shmem_runtime_->DestroySegment(kTestSegment, &handler_);
}

}  // namespace net_instaweb
