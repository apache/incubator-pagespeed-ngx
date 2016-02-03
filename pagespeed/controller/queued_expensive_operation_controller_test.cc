/*
 * Copyright 2015 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: cheesy@google.com (Steve Hill)
 */

#include "pagespeed/controller/queued_expensive_operation_controller.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"

namespace net_instaweb {

namespace {

class TrackCallsFunction : public Function {
 public:
  TrackCallsFunction() : run_called_(false), cancel_called_(false) {
    set_delete_after_callback(false);
  }
  virtual ~TrackCallsFunction() { }

  virtual void Run() { run_called_ = true; }
  virtual void Cancel() { cancel_called_ = true; }

  bool run_called_;
  bool cancel_called_;
};

class QueuedExpensiveOperationTest : public testing::Test {
 public:
  QueuedExpensiveOperationTest()
      : thread_system_(Platform::CreateThreadSystem()),
        stats_(thread_system_.get()) {
    QueuedExpensiveOperationController::InitStats(&stats_);
    InitQueueWithSize(1);
  }

  void InitQueueWithSize(int size) {
    controller_.reset(new QueuedExpensiveOperationController(
        size, thread_system_.get(), &stats_));
  }

  int64 active_operations() {
    return stats_
        .GetUpDownCounter(
            QueuedExpensiveOperationController::kActiveExpensiveOperations)
        ->Get();
  }

  int64 queued_operations() {
    return stats_
        .GetUpDownCounter(
            QueuedExpensiveOperationController::kQueuedExpensiveOperations)
        ->Get();
  }

 protected:
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;
  scoped_ptr<QueuedExpensiveOperationController> controller_;
};

TEST_F(QueuedExpensiveOperationTest, EmptyScheduleImmediately) {
  TrackCallsFunction f;
  EXPECT_FALSE(f.run_called_);
  EXPECT_FALSE(f.cancel_called_);
  controller_->ScheduleExpensiveOperation(&f);
  EXPECT_EQ(1, active_operations());
  EXPECT_EQ(0, queued_operations());
  EXPECT_TRUE(f.run_called_);
  EXPECT_FALSE(f.cancel_called_);
  controller_->NotifyExpensiveOperationComplete();
  EXPECT_EQ(0, active_operations());
  EXPECT_EQ(0, queued_operations());
}

TEST_F(QueuedExpensiveOperationTest, ActuallyLimits) {
  TrackCallsFunction f1;
  TrackCallsFunction f2;

  controller_->ScheduleExpensiveOperation(&f1);
  EXPECT_EQ(1, active_operations());
  EXPECT_EQ(0, queued_operations());
  EXPECT_TRUE(f1.run_called_);
  EXPECT_FALSE(f1.cancel_called_);

  controller_->ScheduleExpensiveOperation(&f2);
  EXPECT_EQ(1, active_operations());
  EXPECT_EQ(1, queued_operations());
  EXPECT_TRUE(f1.run_called_);
  EXPECT_FALSE(f1.cancel_called_);
  EXPECT_FALSE(f2.run_called_);
  EXPECT_FALSE(f2.cancel_called_);

  controller_->NotifyExpensiveOperationComplete();
  EXPECT_EQ(1, active_operations());
  EXPECT_EQ(0, queued_operations());
  EXPECT_TRUE(f2.run_called_);
  EXPECT_FALSE(f2.cancel_called_);

  controller_->NotifyExpensiveOperationComplete();
  EXPECT_EQ(0, active_operations());
  EXPECT_EQ(0, queued_operations());
}

TEST_F(QueuedExpensiveOperationTest, QueueOrder) {
  TrackCallsFunction f1;
  TrackCallsFunction f2;
  TrackCallsFunction f3;

  controller_->ScheduleExpensiveOperation(&f1);
  EXPECT_EQ(1, active_operations());
  EXPECT_EQ(0, queued_operations());
  EXPECT_TRUE(f1.run_called_);

  controller_->ScheduleExpensiveOperation(&f2);
  EXPECT_EQ(1, active_operations());
  EXPECT_EQ(1, queued_operations());
  EXPECT_FALSE(f2.run_called_);
  EXPECT_FALSE(f2.cancel_called_);

  controller_->ScheduleExpensiveOperation(&f3);
  EXPECT_EQ(1, active_operations());
  EXPECT_EQ(2, queued_operations());
  EXPECT_FALSE(f3.run_called_);
  EXPECT_FALSE(f3.cancel_called_);

  controller_->NotifyExpensiveOperationComplete();
  EXPECT_EQ(1, active_operations());
  EXPECT_EQ(1, queued_operations());
  EXPECT_TRUE(f2.run_called_);
  EXPECT_FALSE(f3.run_called_);

  controller_->NotifyExpensiveOperationComplete();
  EXPECT_EQ(1, active_operations());
  EXPECT_EQ(0, queued_operations());
  EXPECT_TRUE(f3.run_called_);

  controller_->NotifyExpensiveOperationComplete();
  EXPECT_EQ(0, active_operations());
  EXPECT_EQ(0, queued_operations());
}

TEST_F(QueuedExpensiveOperationTest, QueueSize2) {
  InitQueueWithSize(2);

  TrackCallsFunction f1;
  TrackCallsFunction f2;
  TrackCallsFunction f3;

  controller_->ScheduleExpensiveOperation(&f1);
  EXPECT_EQ(1, active_operations());
  EXPECT_EQ(0, queued_operations());
  EXPECT_TRUE(f1.run_called_);
  EXPECT_FALSE(f1.cancel_called_);

  controller_->ScheduleExpensiveOperation(&f2);
  EXPECT_EQ(2, active_operations());
  EXPECT_EQ(0, queued_operations());
  EXPECT_TRUE(f2.run_called_);
  EXPECT_FALSE(f2.cancel_called_);

  controller_->ScheduleExpensiveOperation(&f3);
  EXPECT_EQ(2, active_operations());
  EXPECT_EQ(1, queued_operations());
  EXPECT_FALSE(f3.run_called_);
  EXPECT_FALSE(f3.cancel_called_);

  controller_->NotifyExpensiveOperationComplete();
  EXPECT_EQ(2, active_operations());
  EXPECT_EQ(0, queued_operations());
  EXPECT_TRUE(f3.run_called_);
  EXPECT_FALSE(f3.cancel_called_);

  controller_->NotifyExpensiveOperationComplete();
  controller_->NotifyExpensiveOperationComplete();
  EXPECT_EQ(0, active_operations());
  EXPECT_EQ(0, queued_operations());
}

}  // namespace
}  // namespace net_instaweb
