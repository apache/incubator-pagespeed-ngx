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

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cstdlib>
#include <vector>

#include "net/instaweb/util/public/shared_mem_test_base.h"
#include "net/instaweb/util/public/pthread_shared_mem.h"


namespace net_instaweb {

namespace {

// This test is parametrized on whether to test with processes or threads,
// since we want PthreadSharedMem to be usable with both.
class PthreadSharedMemTest
    : public SharedMemTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  PthreadSharedMemTest()
      : SharedMemTestBase(new PthreadSharedMem),
        use_threads_(GetParam()) {
  }

 protected:
  virtual bool CreateChild(TestMethod method) {
    if (use_threads_) {
      Closure* closure = new Closure;
      closure->base = this;
      closure->method = method;
      pthread_t thread;
      if (pthread_create(&thread, NULL, Closure::Invoke, closure) != 0) {
        return false;
      }
      child_threads_.push_back(thread);
      return true;
    } else {
      pid_t ret = fork();
      if (ret == -1) {
        // Failure
        return false;
      } else if (ret == 0) {
        // Child.
        (this->*method)();
        std::exit(0);
      } else {
        // Parent.
        child_processes_.push_back(ret);
        return true;
      }
    }
  }

  virtual void WaitForChildren() {
    for (size_t i = 0; i < child_processes_.size(); ++i) {
      int status;
      EXPECT_EQ(child_processes_[i], waitpid(child_processes_[i], &status, 0));
      EXPECT_TRUE(WIFEXITED(status)) << "Child did not exit cleanly";
      EXPECT_EQ(0, WEXITSTATUS(status)) << "Child reported failure";
    }
    child_processes_.clear();

    for (size_t i = 0; i < child_threads_.size(); ++i) {
      void* result = this;  // non-NULL -> failure.
      EXPECT_EQ(0, pthread_join(child_threads_[i], &result));
      EXPECT_EQ(NULL, result) << "Child reported failure";
    }
    child_threads_.clear();
  }

  virtual void ShortSleep() {
    usleep(1000);
  }

  virtual void ChildFailed() {
    if (use_threads_) {
      pthread_exit(this);
    } else {
      exit(-1);
    }
  }

 private:
  // Information we need to invoke the method passed to CreateChild
  // inside a thread
  struct Closure {
    PthreadSharedMemTest* base;
    TestMethod method;

    static void* Invoke(void* instance) {
      static_cast<Closure*>(instance)->DoInvoke();
      return NULL;  // null -> success
    }

    void DoInvoke() {
      (base->*method)();
      delete this;
    }
  };

  std::vector<pid_t> child_processes_;
  std::vector<pthread_t> child_threads_;
  bool use_threads_;

  DISALLOW_COPY_AND_ASSIGN(PthreadSharedMemTest);
};

TEST_P(PthreadSharedMemTest, TestRewrite) {
  TestReadWrite(false);
}

TEST_P(PthreadSharedMemTest, TestRewriteReattach) {
  TestReadWrite(true);
}

TEST_P(PthreadSharedMemTest, TestLarge) {
  TestLarge();
}

TEST_P(PthreadSharedMemTest, TestDistinct) {
  TestDistinct();
}

TEST_P(PthreadSharedMemTest, TestDestroy) {
  TestDestroy();
}

TEST_P(PthreadSharedMemTest, TestCreateTwice) {
  TestCreateTwice();
}

TEST_P(PthreadSharedMemTest, TestTwoKids) {
  TestTwoKids();
}

TEST_P(PthreadSharedMemTest, TestMutex) {
  TestMutex();
}

INSTANTIATE_TEST_CASE_P(PthreadSharedMemTestInstance,
                        PthreadSharedMemTest,
                        ::testing::Bool());

}  // namespace

}  // namespace net_instaweb
