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

#include "pagespeed/kernel/thread/mock_scheduler.h"

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/thread/scheduler.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

// Make the simulated times be very long just to show that we are in
// mock time and don't need to wait a century for this test to finish.
const int64 kDelayMs = 50 * Timer::kYearMs;
const int64 kWaitMs = 100 * Timer::kYearMs;

class TestAlarm : public Function {
 public:
  TestAlarm() {}
  virtual void Run() { }
};

// This is an alarm implementation which adds new alarms and optionally advances
// time in its callback.
class ChainedAlarm : public Function {
 public:
  // Constructs a chained alarm that will run *count times with 100ms delay
  // between each alarm.  If 'advance' is set, it will advance time forward
  // by 100ms on each call so that the chain unwinds itself.
  ChainedAlarm(MockScheduler* scheduler, int* count, bool advance)
      : scheduler_(scheduler),
        count_(count),
        advance_(advance) {}

  virtual void Run() {
    if (--*count_ > 0) {
      int64 next_time_us = scheduler_->timer()->NowUs();
      scheduler_->AddAlarmAtUs(next_time_us,
                               new ChainedAlarm(scheduler_, count_, advance_));
      if (advance_) {
        scheduler_->AdvanceTimeUs(100);
      }
    }
  }

 private:
  MockScheduler* scheduler_;
  int* count_;
  bool advance_;

  DISALLOW_COPY_AND_ASSIGN(ChainedAlarm);
};

}  // namespace

class MockSchedulerTest : public testing::Test {
 protected:
  MockSchedulerTest()
      : timer_(0),
        thread_system_(Platform::CreateThreadSystem()),
        scheduler_(new MockScheduler(thread_system_.get(), &timer_)),
        was_run_(false),
        was_cancelled_(false) {}

  Scheduler::Alarm* AddTask(int64 wakeup_time_us, char c) {
    Function* append_char = MakeFunction<GoogleString, char>(
        &string_, &GoogleString::push_back, c);
    return scheduler_->AddAlarmAtUs(wakeup_time_us, append_char);
  }

  void Run() {
    was_run_ = true;
  }

  void Cancel() {
    was_cancelled_ = true;
  }

  Scheduler::Alarm* AddRunCancelAlarmUs(int64 wakeup_time_us) {
    return scheduler_->AddAlarmAtUs(wakeup_time_us, MakeFunction(
        this, &MockSchedulerTest::Run, &MockSchedulerTest::Cancel));
  }

  void AdvanceTimeMs(int64 interval_ms) {
    scheduler_->AdvanceTimeMs(interval_ms);
  }

  void AdvanceTimeUs(int64 interval_us) {
    scheduler_->AdvanceTimeUs(interval_us);
  }

 protected:
  MockTimer timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<MockScheduler> scheduler_;
  GoogleString string_;
  bool was_run_;
  bool was_cancelled_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSchedulerTest);
};

TEST_F(MockSchedulerTest, ScheduleOrdering) {
  AddTask(1, '1');
  AddTask(3, '3');
  AddTask(2, '2');
  AdvanceTimeMs(3);  // runs all 3 tasks
  EXPECT_EQ("123", string_);
}

TEST_F(MockSchedulerTest, AdvancePastLastScheduledEvent) {
  AddTask(1, '1');
  AdvanceTimeMs(3);  // runs 1 ask, but moves time 2ms further.
  EXPECT_EQ("1", string_);
  EXPECT_EQ(3, timer_.NowMs());
}

TEST_F(MockSchedulerTest, SchedulePartial) {
  AddTask(5, '5');
  AddTask(5, '6');  // same wakeup time, but order is preserved.
  AddTask(6, '7');
  AddTask(3, '3');
  AddTask(2, '2');
  AddTask(4, '4');
  AddTask(1, '1');
  AdvanceTimeUs(3);  // runs first 3 tasks
  EXPECT_EQ("123", string_);
  string_.clear();
  AdvanceTimeUs(3);  // runs next 4 tasks
  EXPECT_EQ("4567", string_);
}

TEST_F(MockSchedulerTest, Cancellation) {
  AddTask(1, '1');
  Scheduler::Alarm* alarm_to_cancel = AddTask(3, '3');
  AddTask(2, '2');
  AddTask(4, '4');
  {
    ScopedMutex lock(scheduler_->mutex());
    scheduler_->CancelAlarm(alarm_to_cancel);
  }
  AdvanceTimeUs(4);  // runs the 3 tasks not canceled.
  EXPECT_EQ("124", string_);
}

// Verifies that we can add a new alarm from an Alarm::Run() method.
TEST_F(MockSchedulerTest, ChainedAlarms) {
  int count = 10;
  scheduler_->AddAlarmAtUs(
      100, new ChainedAlarm(scheduler_.get(), &count, false));
  AdvanceTimeUs(1000);
  EXPECT_EQ(0, count);
}

// Verifies that we can advance time from an Alarm::Run() method.
TEST_F(MockSchedulerTest, AdvanceFromRun) {
  int count = 10;
  scheduler_->AddAlarmAtUs(
      100, new ChainedAlarm(scheduler_.get(), &count, true));
  AdvanceTimeUs(100);
  EXPECT_EQ(0, count);
}

TEST_F(MockSchedulerTest, RunNotCancelled) {
  // First, let the alarm run normally.
  AddRunCancelAlarmUs(100);
  AdvanceTimeUs(200);
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
}

TEST_F(MockSchedulerTest, CancelledExplicitly) {
  // Next cancel the alarm explicitly before it runs.
  Scheduler::Alarm* alarm = AddRunCancelAlarmUs(500);
  {
    ScopedMutex lock(scheduler_->mutex());
    scheduler_->CancelAlarm(alarm);
  }
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
}

#if SCHEDULER_CANCEL_OUTSTANDING_ALARMS_ON_DESTRUCTION
// Note that this test will not only fail without canceling the alarms on
// destruction, it will also leak the alarms.
TEST_F(MockSchedulerTest, CancelledDueToMockSchedulerDestruction) {
  // Finally, let the alarm be implicitly cancelled by deleting the scheduler.
  AddRunCancelAlarmUs(500);
  scheduler_.reset(NULL);
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
}
#endif

TEST_F(MockSchedulerTest, WakeupOnAdvancementOfSimulatedTime) {
  scheduler_->AddAlarmAtUs(Timer::kMsUs * kDelayMs, new TestAlarm());
  {
    AdvanceTimeMs(kWaitMs);

    // The idle-callback will get run after 50 years, but TimedWait
    // won't actually return until the full 100 years has passed.
    EXPECT_EQ(kWaitMs, timer_.NowMs());
  }
}

}  // namespace net_instaweb
