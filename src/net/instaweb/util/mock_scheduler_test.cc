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
#include "net/instaweb/util/public/closure.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/queued_worker.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Make the simulated times be very long just to show that we are in
// mock time and don't need to wait a century for this test to finish.
const int64 kDelayMs = 50 * Timer::kYearMs;
const int64 kWaitMs = 100 * Timer::kYearMs;

class Alarm : public Closure {
 public:
  Alarm() {}
  virtual void Run() { }
};

}  // namespace

class MockSchedulerTest : public testing::Test {
 protected:
  MockSchedulerTest()
      : timer_(0),
        thread_system_(ThreadSystem::CreateThreadSystem()),
        worker_(thread_system_.get()),
        scheduler_(thread_system_.get(), &worker_, &timer_) {
  }

 protected:
  MockTimer timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  QueuedWorker worker_;
  MockScheduler scheduler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSchedulerTest);
};

TEST_F(MockSchedulerTest, WakeupOnAdvancementOfSimulatedTime) {
  ASSERT_TRUE(worker_.Start());
  timer_.AddAlarm(1000 * kDelayMs, new Alarm());
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.TimedWait(kWaitMs);

    // The idle-callback will get run after 50 years, but TimedWait
    // won't actually return until the full 100 years has passed.
    EXPECT_EQ(kWaitMs, timer_.NowMs());
  }
}

}  // namespace net_instaweb
