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
// Author: jmarantz@google.com (Joshua Marantz)

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_thread_system.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/queued_worker.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/worker.h"

namespace net_instaweb {

namespace {

// Make the simulated times be very long just to show that we are in
// mock time and don't need to wait 50 years for this test to finish.
const int64 kWorkerAdvanceMs = 100 * Timer::kYearMs;
const int64 kMainlineWaitMs = 50 * Timer::kYearMs;

class AdvanceTime : public Worker::Closure {
 public:
  AdvanceTime(MockTimer* timer) : timer_(timer) {}
  virtual void Run() { timer_->AdvanceMs(kWorkerAdvanceMs); }

 private:
  MockTimer* timer_;
};

}  // namespace

class MockTimeCondvarTest : public testing::Test {
 protected:
  MockTimeCondvarTest()
      : timer_(0),
        base_thread_system_(ThreadSystem::CreateThreadSystem()),
        thread_system_(base_thread_system_.get(), &timer_),
        worker_(&thread_system_),
        mutex_(thread_system_.NewMutex()),
        condvar_(mutex_->NewCondvar()) {
  }

 protected:
  MockTimer timer_;
  scoped_ptr<ThreadSystem> base_thread_system_;
  MockThreadSystem thread_system_;
  QueuedWorker worker_;
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> condvar_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTimeCondvarTest);
};

TEST_F(MockTimeCondvarTest, WakeupOnAdvancementOfSimulatedTime) {
  ASSERT_TRUE(worker_.Start());
  worker_.RunInWorkThread(new AdvanceTime(&timer_));
  ScopedMutex lock(mutex_.get());
  condvar_->TimedWait(kMainlineWaitMs);
}

}  // namespace net_instaweb
