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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_TEST_BASE_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_message_handler.h"

namespace net_instaweb {

class SharedMemTestBase : public testing::Test {
 protected:
  typedef void (SharedMemTestBase::*TestMethod)();

  // Pass in a fresh copy of the runtime you're trying to test
  explicit SharedMemTestBase(AbstractSharedMem* shmem_runtime);

  // This method must be overridden to start a new process and run
  // this->*(method)() in it. Returns whether OK or not.
  virtual bool CreateChild(TestMethod method) = 0;

  // This method must be overridden to block until all processes started by
  // CreateChild exit.
  virtual void WaitForChildren() = 0;

  // Runtime-specific short sleep.
  virtual void ShortSleep() = 0;

  // Child exiting with failure
  virtual void ChildFailed() = 0;

  // Basic read/write operation test.
  void TestReadWrite(bool reattach);

  // Test with large data; also test initialization.
  void TestLarge();

  // Make sure that 2 segments don't interfere.
  void TestDistinct();

  // Make sure destruction destroys things properly...
  void TestDestroy();

  // Make sure that re-creating a segment without a Destroy is safe and
  // produces a distinct segment.
  void TestCreateTwice();

  // Make sure between two kids see the SHM as well.
  void TestTwoKids();

  // Test for mutex operation.
  void TestMutex();

 private:
  static const int kLarge = 0x1000 - 4;  // not a multiple of any page size, but
                                         // a multiple of 4.
  static const int kNumIncrements = 0xFFFFF;

  volatile int* IntPtr(AbstractSharedMemSegment* seg, int offset) {
    return reinterpret_cast<volatile int*>(&seg->Base()[offset]);
  }

  AbstractSharedMemSegment* CreateDefault();
  AbstractSharedMemSegment* AttachDefault();
  void DestroyDefault();

  // writes '1' to the default segment's base location.
  void WriteSeg1Child();

  // writes '2' to the other segment's base location.
  void WriteSeg2Child();

  void TestReadWriteChild();
  void TestLargeChild();
  void TwoKidsChild1();
  void TwoKidsChild2();

  bool IncrementStorm(AbstractSharedMemSegment* seg, size_t mutex_size);

  void MutexChild();

  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  MockMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemTestBase);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_TEST_BASE_H_
