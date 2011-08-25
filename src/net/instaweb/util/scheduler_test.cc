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
// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

namespace {

const int64 kDsUs = Timer::kSecondUs / 10;
const int64 kMsUs = Timer::kSecondUs / Timer::kSecondMs;
const int64 kMinuteUs = Timer::kMinuteMs * kMsUs;
const int64 kYearUs = Timer::kYearMs * kMsUs;

// Many tests cribbed from mock_timer_test, only without the mockery.  This
// actually restricts the timing dependencies we can detect, though not in a
// terrible way.

class CountAlarm : public Scheduler::Alarm {
 public:
  explicit CountAlarm(int* variable) : variable_(variable) { }
  virtual void Run() {
    ++*variable_;
  }
  virtual void Cancel() {
    *variable_ -= 100;
  }
 private:
  int *variable_;
  DISALLOW_COPY_AND_ASSIGN(CountAlarm);
};

class SchedulerTest : public WorkerTestBase {
 protected:
  SchedulerTest()
      : thread_system_(ThreadSystem::CreateThreadSystem()),
        timer_(),
        scheduler_(thread_system_.get(), &timer_) { }

  scoped_ptr<ThreadSystem> thread_system_;
  GoogleTimer timer_;
  Scheduler scheduler_;
 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerTest);
};

TEST_F(SchedulerTest, AlarmsGetRun) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  CountAlarm alarm1(&counter);
  CountAlarm alarm2(&counter);
  CountAlarm alarm3(&counter);
  scheduler_.AddAlarm(start_us + 2 * kMsUs, &alarm1);
  scheduler_.AddAlarm(start_us + 4 * kMsUs, &alarm2);
  scheduler_.AddAlarm(start_us + 3 * kMsUs, &alarm3);
  EXPECT_EQ(0, alarm1.Compare(&alarm1));
  EXPECT_EQ(0, alarm2.Compare(&alarm2));
  EXPECT_EQ(0, alarm3.Compare(&alarm3));
  EXPECT_GT(0, alarm1.Compare(&alarm2));
  EXPECT_GT(0, alarm1.Compare(&alarm3));
  EXPECT_LT(0, alarm2.Compare(&alarm1));
  EXPECT_LT(0, alarm2.Compare(&alarm3));
  EXPECT_LT(0, alarm3.Compare(&alarm1));
  EXPECT_GT(0, alarm3.Compare(&alarm2));
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.BlockingTimedWait(5);  // Never signaled, should time out.
  }
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(3, counter);
  EXPECT_LT(start_us + 5 * kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + kMinuteUs, end_us);
};

TEST_F(SchedulerTest, MidpointBlock) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  CountAlarm alarm1(&counter);
  CountAlarm alarm2(&counter);
  CountAlarm alarm3(&counter);
  scheduler_.AddAlarm(start_us + 2 * kMsUs, &alarm1);
  scheduler_.AddAlarm(start_us + 6 * kMsUs, &alarm2);
  scheduler_.AddAlarm(start_us + 3 * kMsUs, &alarm3);
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.BlockingTimedWait(4);  // Never signaled, should time out.
  }
  int64 mid_us = timer_.NowUs();
  EXPECT_LT(start_us + 4 * kMsUs, mid_us);
  EXPECT_LE(2, counter);
  scheduler_.ProcessAlarms(kMinuteUs);
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(3, counter);
  EXPECT_LT(start_us + 6 * kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + kMinuteUs, end_us);
};

TEST_F(SchedulerTest, AlarmInPastRuns) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  CountAlarm alarm1(&counter);
  CountAlarm alarm2(&counter);
  scheduler_.AddAlarm(start_us - 2 * kMsUs, &alarm1);  // Should run
  scheduler_.AddAlarm(start_us + kMinuteUs, &alarm2);  // Should not
  scheduler_.ProcessAlarms(0);  // Don't block!
  EXPECT_EQ(1, counter);
};

TEST_F(SchedulerTest, MidpointCancellation) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  CountAlarm alarm1(&counter);
  CountAlarm alarm2(&counter);
  CountAlarm alarm3(&counter);
  scheduler_.AddAlarm(start_us + 3 * kMsUs, &alarm1);
  scheduler_.AddAlarm(start_us + 2 * kMsUs, &alarm2);
  scheduler_.AddAlarm(start_us + kMinuteUs, &alarm3);
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.BlockingTimedWait(4);  // Never signaled, should time out.
  }
  int64 mid_us = timer_.NowUs();
  EXPECT_LT(start_us + 4 * kMsUs, mid_us);
  EXPECT_EQ(2, counter);
  scheduler_.CancelAlarm(&alarm1);
  scheduler_.CancelAlarm(&alarm2);
  scheduler_.CancelAlarm(&alarm3);
  scheduler_.ProcessAlarms(kMinuteUs);
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(-98, counter);
  EXPECT_LT(start_us + 3 * kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + kMinuteUs, end_us);
};

TEST_F(SchedulerTest, SimultaneousAlarms) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  CountAlarm alarm1(&counter);
  CountAlarm alarm2(&counter);
  CountAlarm alarm3(&counter);
  scheduler_.AddAlarm(start_us + 4 * kMsUs, &alarm1);
  scheduler_.AddAlarm(start_us + 4 * kMsUs, &alarm2);
  scheduler_.AddAlarm(start_us + 4 * kMsUs, &alarm3);
  scheduler_.ProcessAlarms(kMinuteUs);
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(3, counter);
  EXPECT_LT(start_us + 4 * kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + kMinuteUs, end_us);
};

TEST_F(SchedulerTest, FunctionMidpointBlock) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  scheduler_.AddAlarmFunction(start_us + 2 * kMsUs,
                              new CountFunction(&counter));
  scheduler_.AddAlarmFunction(start_us + 6 * kMsUs,
                              new CountFunction(&counter));
  scheduler_.AddAlarmFunction(start_us + 3 * kMsUs,
                              new CountFunction(&counter));
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.BlockingTimedWait(4);  // Never signaled, should time out.
  }
  int64 mid_us = timer_.NowUs();
  EXPECT_LT(start_us + 4 * kMsUs, mid_us);
  EXPECT_LE(2, counter);
  scheduler_.ProcessAlarms(kMinuteUs);
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(3, counter);
  EXPECT_LT(start_us + 6 * kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + kMinuteUs, end_us);
};

TEST_F(SchedulerTest, FunctionMidpointCancellation) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  scheduler_.AddAlarmFunction(
      start_us + 3 * kMsUs, new CountFunction(&counter));
  scheduler_.AddAlarmFunction(
      start_us + 2 * kMsUs, new CountFunction(&counter));
  Scheduler::Alarm* alarm3 = scheduler_.AddAlarmFunction(
      start_us + kMinuteUs, new CountFunction(&counter));
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.BlockingTimedWait(4);  // Never signaled, should time out.
  }
  int64 mid_us = timer_.NowUs();
  EXPECT_LT(start_us + 4 * kMsUs, mid_us);
  EXPECT_EQ(2, counter);
  // This provides a nice view of the distinction between an Alarm and an
  // ordinary Function used as an alarm.  If we provide our own Alarm, we can
  // manage our own memory, and thus handle cancellation after invocation.
  // Here we can't do that, as the alarm is deleted.
  // scheduler_.CancelAlarm(alarm1);  // Can't safely cancel, it's deleted!
  // scheduler_.CancelAlarm(alarm2);  // Can't safely cancel, it's deleted!
  scheduler_.CancelAlarm(alarm3);
  scheduler_.ProcessAlarms(kMinuteUs);
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(2, counter);
  EXPECT_LT(start_us + 3 * kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + kMinuteUs, end_us);
};

TEST_F(SchedulerTest, TimedWaitExpire) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.TimedWait(2, new CountFunction(&counter));
    scheduler_.TimedWait(4, new CountFunction(&counter));
    scheduler_.TimedWait(3, new CountFunction(&counter));
    scheduler_.BlockingTimedWait(5);
  }
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(3, counter);
  EXPECT_LT(start_us + 5 * kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + kMinuteUs, end_us);
};

TEST_F(SchedulerTest, TimedWaitSignal) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.TimedWait(2, new CountFunction(&counter));
    scheduler_.TimedWait(4, new CountFunction(&counter));
    scheduler_.TimedWait(3, new CountFunction(&counter));
    scheduler_.Signal();
  }
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(3, counter);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + kMinuteUs, end_us);
};

TEST_F(SchedulerTest, TimedWaitMidpointSignal) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.TimedWait(3, new CountFunction(&counter));
    scheduler_.TimedWait(2, new CountFunction(&counter));
    scheduler_.TimedWait(Timer::kYearMs, new CountFunction(&counter));
    scheduler_.BlockingTimedWait(4);  // Will time out
    EXPECT_EQ(2, counter);
    scheduler_.Signal();
  }
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(3, counter);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + kMinuteUs, end_us);
};

}  // namespace

}  // namespace net_instaweb
