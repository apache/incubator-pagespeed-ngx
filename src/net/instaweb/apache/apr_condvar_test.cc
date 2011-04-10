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

#include "net/instaweb/apache/apr_condvar.h"

#include "apr_pools.h"
#include "apr_thread_proc.h"
#include "apr_timer.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apr_mutex.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/condvar_test_base.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class AprCondvarTest : public CondvarTestBase {
 protected:
  AprCondvarTest() { }

  virtual void SetUp() {
    CondvarTestBase::SetUp();
    apr_initialize();
    atexit(apr_terminate);
    apr_pool_create(&pool_, NULL);
    apr_mutex_.reset(new AprMutex(pool_));
    apr_startup_condvar_.reset(new AprCondvar(apr_mutex_.get()));
    apr_condvar_.reset(new AprCondvar(apr_mutex_.get()));
    Init(apr_mutex_.get(), apr_startup_condvar_.get(), apr_condvar_.get());
  }

  virtual void TearDown() {
    CondvarTestBase::TearDown();
    apr_condvar_.reset();
    apr_startup_condvar_.reset();
    apr_mutex_.reset();
    helper_thread_ = NULL;
    apr_pool_destroy(pool_);
  }

  static void* AprHelperThread(apr_thread_t* /*me*/, void* data) {
    return HelperThread(data);
  }

  virtual void CreateHelper() {
    apr_thread_create(&helper_thread_, NULL,
                      &AprHelperThread, this, pool_);
  }

  virtual void FinishHelper() {
    apr_status_t ignored_retval;
    apr_thread_join(&ignored_retval, helper_thread_);
  }

  virtual Timer* timer() { return &timer_; }

  apr_pool_t* pool_;
  scoped_ptr<AprMutex> apr_mutex_;
  scoped_ptr<AprCondvar> apr_startup_condvar_;
  scoped_ptr<AprCondvar> apr_condvar_;
  apr_thread_t* helper_thread_;
  AprTimer timer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AprCondvarTest);
};

TEST_F(AprCondvarTest, TestStartup) {
  StartupTest();
}

TEST_F(AprCondvarTest, BlindSignals) {
  BlindSignalsTest();
}

TEST_F(AprCondvarTest, BroadcastBlindSignals) {
  signal_method_ = &AbstractCondvar::Broadcast;
  BlindSignalsTest();
}

TEST_F(AprCondvarTest, TestPingPong) {
  PingPongTest();
}

TEST_F(AprCondvarTest, BroadcastTestPingPong) {
  signal_method_ = &AbstractCondvar::Broadcast;
  PingPongTest();
}

TEST_F(AprCondvarTest, TestTimeout) {
  TimeoutTest();
}

TEST_F(AprCondvarTest, TestLongTimeout300) {
  // We pick a reasonable value because our API into APR does
  // not have a special-case at 1 second.
  LongTimeoutTest(300);
}

TEST_F(AprCondvarTest, TimeoutPingPong) {
  TimeoutPingPongTest();
}

TEST_F(AprCondvarTest, BroadcastTimeoutPingPong) {
  signal_method_ = &AbstractCondvar::Broadcast;
  TimeoutPingPongTest();
}

}  // namespace net_instaweb
