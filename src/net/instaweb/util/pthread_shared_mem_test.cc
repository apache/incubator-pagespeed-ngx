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
#include <sys/wait.h>

#include <cstddef>
#include <cstdlib>
#include <vector>

#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/pthread_shared_mem.h"
#include "net/instaweb/util/public/shared_circular_buffer_test_base.h"
#include "net/instaweb/util/public/shared_dynamic_string_map_test_base.h"
#include "net/instaweb/util/public/shared_mem_lock_manager_test_base.h"
#include "net/instaweb/util/public/shared_mem_statistics_test_base.h"
#include "net/instaweb/util/public/shared_mem_test_base.h"

namespace net_instaweb {
class AbstractSharedMem;

namespace {

// We test operation of pthread shared memory with both thread & process
// use, which is what PthreadSharedMemThreadEnv and PthreadSharedMemProcEnv
// provide.

class PthreadSharedMemEnvBase : public SharedMemTestEnv {
 public:
  virtual AbstractSharedMem* CreateSharedMemRuntime() {
    return new PthreadSharedMem();
  }

  virtual void ShortSleep() {
    usleep(1000);
  }
};

class PthreadSharedMemThreadEnv : public PthreadSharedMemEnvBase {
 public:
  virtual bool CreateChild(Function* callback) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, InvokeCallback, callback) != 0) {
      return false;
    }
    child_threads_.push_back(thread);
    return true;
  }

  virtual void WaitForChildren() {
    for (size_t i = 0; i < child_threads_.size(); ++i) {
      void* result = this;  // non-NULL -> failure.
      EXPECT_EQ(0, pthread_join(child_threads_[i], &result));
      EXPECT_EQ(NULL, result) << "Child reported failure";
    }
    child_threads_.clear();
  }

  virtual void ChildFailed() {
    // In case of failure, we exit the thread with a non-NULL status.
    // We leak the callback object in that case, but this only gets called
    // for test failures anyway.
    pthread_exit(this);
  }

 private:
  static void* InvokeCallback(void* raw_callback_ptr) {
    Function* callback = static_cast<Function*>(raw_callback_ptr);
    callback->Run();
    delete callback;
    return NULL;  // Used to denote success
  }

  std::vector<pthread_t> child_threads_;
};

class PthreadSharedMemProcEnv : public PthreadSharedMemEnvBase {
 public:
  virtual bool CreateChild(Function* callback) {
    pid_t ret = fork();
    if (ret == -1) {
      // Failure
      delete callback;
      return false;
    } else if (ret == 0) {
      // Child.
      callback->Run();
      delete callback;
      std::exit(0);
    } else {
      // Parent.
      child_processes_.push_back(ret);
      delete callback;
      return true;
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
  }

  virtual void ChildFailed() {
    exit(-1);
  }

 private:
  std::vector<pid_t> child_processes_;
};


INSTANTIATE_TYPED_TEST_CASE_P(PthreadProc, SharedMemTestTemplate,
                              PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_CASE_P(PthreadProc, SharedMemLockManagerTestTemplate,
                              PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_CASE_P(PthreadProc, SharedMemStatisticsTestTemplate,
                              PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_CASE_P(PthreadProc, SharedCircularBufferTestTemplate,
                              PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_CASE_P(PthreadProc, SharedDynamicStringMapTestTemplate,
                              PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_CASE_P(PthreadThread, SharedMemTestTemplate,
                              PthreadSharedMemThreadEnv);
INSTANTIATE_TYPED_TEST_CASE_P(PthreadThread, SharedMemLockManagerTestTemplate,
                              PthreadSharedMemThreadEnv);
INSTANTIATE_TYPED_TEST_CASE_P(PthreadThread, SharedMemStatisticsTestTemplate,
                              PthreadSharedMemThreadEnv);
INSTANTIATE_TYPED_TEST_CASE_P(PthreadThread, SharedCircularBufferTestTemplate,
                              PthreadSharedMemThreadEnv);
INSTANTIATE_TYPED_TEST_CASE_P(PthreadThread, SharedDynamicStringMapTestTemplate,
                              PthreadSharedMemThreadEnv);

}  // namespace

}  // namespace net_instaweb
