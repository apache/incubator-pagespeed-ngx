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
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

// Many tests cribbed from mock_timer_test, only without the mockery.  This
// actually restricts the timing dependencies we can detect, though not in a
// terrible way.
class SchedulerTest : public WorkerTestBase {
 protected:
  SchedulerTest()
      : thread_system_(ThreadSystem::CreateThreadSystem()),
        timer_(),
        scheduler_(thread_system_.get(), &timer_) { }

  int Compare(const Scheduler::Alarm* a, const Scheduler::Alarm* b) const {
    Scheduler::CompareAlarms comparator;
    return comparator(a, b);
  }

  void LockAndProcessAlarms(int64 timeout_us) {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.ProcessAlarms(timeout_us);
  }

  void QuiesceAlarms(int64 timeout_us) {
    ScopedMutex lock(scheduler_.mutex());
    int64 now_us = timer_.NowUs();
    int64 end_us = now_us + timeout_us;
    while (now_us < end_us && !scheduler_.NoPendingAlarms()) {
      scheduler_.ProcessAlarms(end_us - now_us);
      now_us = timer_.NowUs();
    }
  }

  scoped_ptr<ThreadSystem> thread_system_;
  GoogleTimer timer_;
  Scheduler scheduler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerTest);
};

namespace {

const int64 kDsUs = Timer::kSecondUs / 10;
const int64 kYearUs = Timer::kYearMs * Timer::kMsUs;

TEST_F(SchedulerTest, AlarmsGetRun) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  // Note that we give this test extra time (50ms) to start up so that
  // we don't attempt to compare already-run (and thus deleted) alarms
  // when running under valgrind.
  Scheduler::Alarm* alarm1 =
      scheduler_.AddAlarm(start_us + 52 * Timer::kMsUs,
                          new CountFunction(&counter));
  Scheduler::Alarm* alarm2 =
      scheduler_.AddAlarm(start_us + 54 * Timer::kMsUs,
                          new CountFunction(&counter));
  Scheduler::Alarm* alarm3 =
      scheduler_.AddAlarm(start_us + 53 * Timer::kMsUs,
                          new CountFunction(&counter));
  if (counter == 0) {
    // In rare cases under Valgrind, we run over the 50ms limit and the
    // callbacks get run and freed.  We skip these checks in that case.
    // Ordinarily these can be observed to run (change the above test to true ||
    // and run under valgrind to observe this).
    EXPECT_FALSE(Compare(alarm1, alarm1));
    EXPECT_FALSE(Compare(alarm2, alarm2));
    EXPECT_FALSE(Compare(alarm3, alarm3));
    EXPECT_TRUE(Compare(alarm1, alarm2));
    EXPECT_TRUE(Compare(alarm1, alarm3));
    EXPECT_FALSE(Compare(alarm2, alarm1));
    EXPECT_FALSE(Compare(alarm2, alarm3));
    EXPECT_FALSE(Compare(alarm3, alarm1));
    EXPECT_TRUE(Compare(alarm3, alarm2));
  }
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.BlockingTimedWait(55);  // Never signaled, should time out.
  }
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(3, counter);
  EXPECT_LT(start_us + 55 * Timer::kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + Timer::kMinuteUs, end_us);
};

TEST_F(SchedulerTest, MidpointBlock) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  scheduler_.AddAlarm(start_us + 2 * Timer::kMsUs, new CountFunction(&counter));
  scheduler_.AddAlarm(start_us + 6 * Timer::kMsUs, new CountFunction(&counter));
  scheduler_.AddAlarm(start_us + 3 * Timer::kMsUs, new CountFunction(&counter));
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.BlockingTimedWait(4);  // Never signaled, should time out.
  }
  int64 mid_us = timer_.NowUs();
  EXPECT_LT(start_us + 4 * Timer::kMsUs, mid_us);
  EXPECT_LE(2, counter);
  QuiesceAlarms(Timer::kMinuteUs);
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(3, counter);
  EXPECT_LT(start_us + 6 * Timer::kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + Timer::kMinuteUs, end_us);
};

TEST_F(SchedulerTest, AlarmInPastRuns) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  scheduler_.AddAlarm(start_us - 2 * Timer::kMsUs, new CountFunction(&counter));
  Scheduler::Alarm* alarm2 =
      scheduler_.AddAlarm(start_us + Timer::kMinuteUs,
                          new CountFunction(&counter));
  LockAndProcessAlarms(0);  // Don't block!
  EXPECT_EQ(1, counter);
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.CancelAlarm(alarm2);
  }
  int64 end_us = timer_.NowUs();
  EXPECT_LT(start_us, end_us);
  EXPECT_GT(start_us + Timer::kMinuteUs, end_us);
};

TEST_F(SchedulerTest, MidpointCancellation) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  scheduler_.AddAlarm(start_us + 3 * Timer::kMsUs, new CountFunction(&counter));
  scheduler_.AddAlarm(start_us + 2 * Timer::kMsUs, new CountFunction(&counter));
  Scheduler::Alarm* alarm3 =
      scheduler_.AddAlarm(start_us + Timer::kMinuteUs,
                          new CountFunction(&counter));
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.BlockingTimedWait(4);  // Never signaled, should time out.
  }
  int64 mid_us = timer_.NowUs();
  EXPECT_LT(start_us + 4 * Timer::kMsUs, mid_us);
  EXPECT_EQ(2, counter);
  // No longer safe to cancel first two alarms.
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.CancelAlarm(alarm3);
  }
  QuiesceAlarms(Timer::kMinuteUs);
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(-98, counter);
  EXPECT_LT(start_us + 3 * Timer::kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + Timer::kMinuteUs, end_us);
};

TEST_F(SchedulerTest, SimultaneousAlarms) {
  int64 start_us = timer_.NowUs();
  int counter = 0;
  scheduler_.AddAlarm(start_us + 2 * Timer::kMsUs, new CountFunction(&counter));
  scheduler_.AddAlarm(start_us + 2 * Timer::kMsUs, new CountFunction(&counter));
  scheduler_.AddAlarm(start_us + 2 * Timer::kMsUs, new CountFunction(&counter));
  QuiesceAlarms(Timer::kMinuteUs);
  int64 end_us = timer_.NowUs();
  EXPECT_EQ(3, counter);
  EXPECT_LT(start_us + 2 * Timer::kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + Timer::kMinuteUs, end_us);
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
  EXPECT_LT(start_us + 5 * Timer::kMsUs, end_us);
  // Note: we assume this will terminate within 1 min., and will have hung
  // noticeably if it didn't.
  EXPECT_GT(start_us + Timer::kMinuteUs, end_us);
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
  EXPECT_GT(start_us + Timer::kMinuteUs, end_us);
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
  EXPECT_GT(start_us + Timer::kMinuteUs, end_us);
};

// Function that retries a TimedWait when invoked until 10ms have passed.
class RetryWaitFunction : public Function {
 public:
  RetryWaitFunction(Timer* timer, int64 start_ms, Scheduler* scheduler,
                    int* counter)
      : timer_(timer), start_ms_(start_ms),
        scheduler_(scheduler), counter_(counter) {
  }

  virtual ~RetryWaitFunction() {}

  virtual void Run() {
    ++*counter_;
    if ((timer_->NowMs() - start_ms_) < 10) {
      // Note that we want the retry delay here to place us later than
      // the original timeout the first invocation had, as that will
      // place us later inside the wait queue ordering. In the past,
      // that would cause Signal() to instantly detect us in the queue
      // and run us w/o returning control.
      scheduler_->TimedWait(
          10, new RetryWaitFunction(timer_, start_ms_, scheduler_, counter_));
    }
  }

 private:
  Timer* timer_;
  int64 start_ms_;
  Scheduler* scheduler_;
  int* counter_;
  DISALLOW_COPY_AND_ASSIGN(RetryWaitFunction);
};

TEST_F(SchedulerTest, TimedWaitFromSignalWakeup) {
  int counter = 0;
  int64 start_ms = timer_.NowMs();
  {
    ScopedMutex lock(scheduler_.mutex());
    scheduler_.TimedWait(
        5, new RetryWaitFunction(&timer_, start_ms, &scheduler_, &counter));
    scheduler_.Signal();
  }
  QuiesceAlarms(20 * Timer::kMsUs);
  EXPECT_GE(2, counter);
}

}  // namespace

}  // namespace net_instaweb
