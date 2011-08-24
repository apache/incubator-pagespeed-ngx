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

#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

namespace {

// This is an alarm implementation which adds new alarms and optionally advances
// time in its callback.
class ChainedAlarm : public Function {
 public:
  ChainedAlarm(MockTimer* timer, int* count, bool advance)
    : timer_(timer),
      count_(count) ,
      advance_(advance) {}

  virtual void Run() {
    if (--*count_ > 0) {
      timer_->AddAlarm(timer_->NowUs() + 100,
                       new ChainedAlarm(timer_, count_, advance_));
      if (advance_) {
        timer_->AdvanceMs(100);
      }
    }
  }

 private:
  MockTimer* timer_;
  int* count_;
  bool advance_;
};

}  // namespace

class MockTimerTest : public testing::Test {
 protected:
  MockTimerTest()
      : timer_(new MockTimer(0)),
        was_run_(false),
        was_cancelled_(false) {}

  MockTimer::Alarm* AddTask(int64 wakeup_time_us, char c) {
    Function* append_char = new MemberFunction1<GoogleString, char>(
        &GoogleString::push_back, &string_, c);
    return timer_->AddAlarm(wakeup_time_us, append_char);
  }

  void Run() {
    was_run_ = true;
  }

  void Cancel() {
    was_cancelled_ = true;
  }

  MockTimer::Alarm* AddRunCancelAlarm(int64 timeout_ms) {
    MockTimerTest* mock_timer_test = this;  // Implicit upcast.
    return timer_->AddAlarm(timeout_ms, new MemberFunction0<MockTimerTest>(
        &MockTimerTest::Run, &MockTimerTest::Cancel, mock_timer_test));
  }

 protected:
  scoped_ptr<MockTimer> timer_;
  GoogleString string_;
  bool was_run_;
  bool was_cancelled_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTimerTest);
};

TEST_F(MockTimerTest, ScheduleOrdering) {
  AddTask(1, '1');
  AddTask(3, '3');
  AddTask(2, '2');
  timer_->AdvanceUs(3);  // runs all 3 tasks
  EXPECT_EQ("123", string_);
}

TEST_F(MockTimerTest, SchedulePartial) {
  AddTask(5, '5');
  AddTask(5, '6');  // same wakeup time, but order is preserved.
  AddTask(6, '7');
  AddTask(3, '3');
  AddTask(2, '2');
  AddTask(4, '4');
  AddTask(1, '1');
  timer_->AdvanceUs(3);  // runs first 3 tasks
  EXPECT_EQ("123", string_);
  string_.clear();
  timer_->AdvanceUs(3);  // runs next 4 tasks
  EXPECT_EQ("4567", string_);
}

TEST_F(MockTimerTest, Cancellation) {
  AddTask(1, '1');
  MockTimer::Alarm* alarm_to_cancel = AddTask(3, '3');
  AddTask(2, '2');
  AddTask(4, '4');
  timer_->CancelAlarm(alarm_to_cancel);
  timer_->AdvanceUs(4);  // runs the 3 tasks not canceled.
  EXPECT_EQ("124", string_);
}

// Verifies that we can add a new alarm from an Alarm::Run() method.
TEST_F(MockTimerTest, ChainedAlarms) {
  int count = 10;
  timer_->AddAlarm(100, new ChainedAlarm(timer_.get(), &count, false));
  timer_->AdvanceMs(1000);
  EXPECT_EQ(0, count);
}

// Verifies that we can advance time from an Alarm::Run() method.
TEST_F(MockTimerTest, AdvanceFromRun) {
  int count = 10;
  timer_->AddAlarm(100, new ChainedAlarm(timer_.get(), &count, true));
  timer_->AdvanceMs(100);
  EXPECT_EQ(0, count);
}

TEST_F(MockTimerTest, RunNotCancelled) {
  // First, let the alarm run normally.
  AddRunCancelAlarm(100);
  timer_->AdvanceUs(200);
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
}

TEST_F(MockTimerTest, CancelledExplicitly) {
  // Next cancel the alarm explicitly before it runs.
  MockTimer::Alarm* alarm = AddRunCancelAlarm(500);
  timer_->CancelAlarm(alarm);
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
}

TEST_F(MockTimerTest, CancelledDueToMockTimerDestruction) {
  // Finally, let the alarm be implicitly cancelled by deleting the timer.
  AddRunCancelAlarm(500);
  timer_.reset(NULL);
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
}

}  // namespace net_instaweb
