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

class SharedMemTestEnv {
 public:
  class Callback {
   public:
    // Note: unlike usual, these callbacks should not auto-cleanup themselves
    // on invocation. The responsibility is given to the TestEnv as the #
    // of copies floating around depends on how the memory sharing ends up.
    virtual void Run() = 0;
    virtual ~Callback();
  };

  // A little helper for using member functions with CreateChild.
  template<typename T>
  class MethodCallback : public Callback {
   public:
    typedef void (T::*Method)();

    MethodCallback(T* base, Method method) : base_(base), method_(method) {
    }

    virtual void Run() {
      (base_->*method_)();
    }

   private:
    T* base_;
    Method method_;
  };

  virtual ~SharedMemTestEnv();
  SharedMemTestEnv() {}

  virtual AbstractSharedMem* CreateSharedMemRuntime() = 0;

  // This method must be overridden to start a new process and invoke the
  // callback object in it. The runtime is responsible for deleting the callback
  // object properly.
  //
  // Returns whether started OK or not.
  virtual bool CreateChild(Callback* callback) = 0;

  // This method must be overridden to block until all processes/threads started
  // by CreateChild exit.
  virtual void WaitForChildren() = 0;

  // Runtime-specific short sleep.
  virtual void ShortSleep() = 0;

  // Called in a child to denote it exiting with failure
  virtual void ChildFailed() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedMemTestEnv);
};

class SharedMemTestBase : public testing::Test {
 protected:
  typedef void (SharedMemTestBase::*TestMethod)();

  explicit SharedMemTestBase(SharedMemTestEnv* test_env);

  bool CreateChild(TestMethod method);

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
  typedef SharedMemTestEnv::MethodCallback<SharedMemTestBase> MethodCallback;

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

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  MockMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemTestBase);
};

// Passes in the SharedMemTestEnv to SharedMemTestBase via a template param
// to help glue us to the test framework
template<typename ConcreteTestEnv>
class SharedMemTestTemplate : public SharedMemTestBase {
 public:
  SharedMemTestTemplate() : SharedMemTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(SharedMemTestTemplate);

TYPED_TEST_P(SharedMemTestTemplate, TestRewrite) {
  SharedMemTestBase::TestReadWrite(false);
}

TYPED_TEST_P(SharedMemTestTemplate, TestRewriteReattach) {
  SharedMemTestBase::TestReadWrite(true);
}

TYPED_TEST_P(SharedMemTestTemplate, TestLarge) {
  SharedMemTestBase::TestLarge();
}

TYPED_TEST_P(SharedMemTestTemplate, TestDistinct) {
  SharedMemTestBase::TestDistinct();
}

TYPED_TEST_P(SharedMemTestTemplate, TestDestroy) {
  SharedMemTestBase::TestDestroy();
}

TYPED_TEST_P(SharedMemTestTemplate, TestCreateTwice) {
  SharedMemTestBase::TestCreateTwice();
}

TYPED_TEST_P(SharedMemTestTemplate, TestTwoKids) {
  SharedMemTestBase::TestTwoKids();
}

TYPED_TEST_P(SharedMemTestTemplate, TestMutex) {
  SharedMemTestBase::TestMutex();
}

REGISTER_TYPED_TEST_CASE_P(SharedMemTestTemplate, TestRewrite,
                           TestRewriteReattach, TestLarge, TestDistinct,
                           TestDestroy, TestCreateTwice, TestTwoKids,
                           TestMutex);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_TEST_BASE_H_
