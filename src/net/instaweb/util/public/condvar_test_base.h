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
// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CONDVAR_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CONDVAR_TEST_BASE_H_

#include "net/instaweb/util/public/abstract_condvar.h"

#include "base/logging.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/timer.h"


namespace net_instaweb {

class CondvarTestBase : public testing::Test {
 protected:
  typedef void (AbstractCondvar::*SignalMethod)();

  CondvarTestBase()
      : mutex_(NULL),
        startup_condvar_(NULL),
        condvar_(NULL),
        ready_to_start_(false),
        iters_(0),
        current_iter_(0),
        signal_method_(&AbstractCondvar::Signal),
        wait_after_signal_(false),
        helper_increments_(0),
        init_called_(false) {
  }

  // Init is intended to be called from the constructor of the derived class.
  // Ownership of the objects remains with the caller.
  void Init(AbstractMutex* mutex,
            AbstractCondvar* startup_condvar, AbstractCondvar* condvar) {
    CHECK(!init_called_);
    mutex_ = mutex;
    startup_condvar_ = startup_condvar;
    condvar_ = condvar;
    init_called_ = true;
  }

  static void* HelperThread(void* data) {
    CondvarTestBase* test = static_cast<CondvarTestBase*>(data);
    test->HelperThreadMethod();
    return NULL;
  }

  // CreateHelper creates a thread that ultimately calls
  // this->HelperThreadMethod(), most simply by invoking HelperThread(this).  It
  // runs this method to completion, and then terminates gracefully.
  virtual void CreateHelper() = 0;

  // FinishHelper is called in the main thread to wait for graceful termination
  // of the thread created by CreateHelper.
  virtual void FinishHelper() = 0;

  void StartHelper() {
    CHECK(init_called_);
    CreateHelper();
    {
      ScopedMutex lock(mutex_);
      ready_to_start_ = true;
      startup_condvar_->Signal();
    }
  }

  virtual void HelperThreadMethod() {
    {
      ScopedMutex lock(mutex_);
      while (!ready_to_start_) {
        startup_condvar_->Wait();
      }
    }
    while (true) {
      ScopedMutex lock(mutex_);
      // We must hold the mutex to access the iteration count and check the loop
      // condition.
      int iter = current_iter_ + 1;
      if (iter > iters_) {
        return;
      }
      ++helper_increments_;
      current_iter_ = iter;
      (condvar_->*signal_method_)();
      while (wait_after_signal_ && current_iter_ == iter) {
        condvar_->Wait();
      }
    }
  }

  // Below are the common tests that should be run by every client.

  // Make sure we can start and stop the helper gracefully.
  void StartupTest() {
    StartHelper();
    EXPECT_TRUE(ready_to_start_);
    FinishHelper();
    EXPECT_EQ(helper_increments_, 0);
  }

  // Run the helper without interacting with it.
  // Also run with signal_method_ = &AbstractCondvar::Broadcast
  void BlindSignalsTest() {
    iters_ = 10;
    StartHelper();
    EXPECT_TRUE(ready_to_start_);
    FinishHelper();
    EXPECT_EQ(helper_increments_, 10);
  }

  // Use condvars to pass control back and forth between worker and main thread.
  // Also run with signal_method_ = &AbstractCondvar::Broadcast
  void PingPongTest() {
    iters_ = 10;
    wait_after_signal_ = true;
    StartHelper();
    int local_increments = 0;
    while (true) {
      ScopedMutex lock(mutex_);
      while ((current_iter_ % 2) == 0 && current_iter_ < iters_) {
        condvar_->Wait();
      }
      // We must hold the mutex to access the iteration count and check the loop
      // condition.
      if (current_iter_ == iters_) {
        break;
      }
      ++current_iter_;
      ++local_increments;
      condvar_->Signal();
    }
    EXPECT_EQ(local_increments, 5);
    FinishHelper();
    EXPECT_EQ(helper_increments_, 5);
  }

  // Make sure that TimedWait eventually progresses in the absence of a signal.
  void TimeoutTest() {
    iters_ = 0;
    StartHelper();
    {
      ScopedMutex lock(mutex_);
      condvar_->TimedWait(10);  // This will deadlock if we don't time out.
    }
    FinishHelper();
  }

  // Make sure that a long timeout doesn't exit too early.
  void LongTimeoutTest(int wait_ms) {
    iters_ = 0;
    StartHelper();
    int64 start_ms = timer()->NowMs();
    {
      ScopedMutex lock(mutex_);
      condvar_->TimedWait(wait_ms);
    }
    int64 end_ms = timer()->NowMs();

    // This test should not be flaky even if it runs slowly, as we are
    // not placing an *upper* bound on the lock duration.
    EXPECT_LE(wait_ms, end_ms - start_ms);
    FinishHelper();
  }

  // Use condvars to pass control back and forth between worker and main thread.
  // Final interaction will be one-sided and will time out.
  // Also run with signal_method_ = &AbstractCondvar::Broadcast
  void TimeoutPingPongTest() {
    iters_ = 10;
    wait_after_signal_ = true;
    StartHelper();
    int local_increments = 0;
    while (true) {
      ScopedMutex lock(mutex_);
      if ((current_iter_ % 2) == 0) {
        condvar_->TimedWait(1);
      }
      // We must hold the mutex to access the iteration count and check the
      // loop condition.  Note that in case of timeout we might get here with
      // current_iter_ % 2 == 0, so we might perform more local increments
      // than we expect.
      if (current_iter_ > iters_) {
        break;
      }
      ++current_iter_;
      ++local_increments;
      condvar_->Signal();
    }
    FinishHelper();
    EXPECT_LE(6, local_increments);
    EXPECT_GE(5, helper_increments_);
    EXPECT_EQ(11, local_increments + helper_increments_);
  }

  virtual Timer* timer() = 0;

  AbstractMutex* mutex_;
  AbstractCondvar* startup_condvar_;
  AbstractCondvar* condvar_;
  bool ready_to_start_;
  int iters_;
  int current_iter_;
  SignalMethod signal_method_;
  bool wait_after_signal_;
  int helper_increments_;
  bool init_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CondvarTestBase);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CONDVAR_TEST_BASE_H_
