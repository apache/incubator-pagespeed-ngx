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

#include "pagespeed/controller/work_bound_expensive_operation_controller.h"

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

class WorkBoundExpensiveOperationTest : public testing::Test {
 public:
  WorkBoundExpensiveOperationTest() :
    thread_system_(Platform::CreateThreadSystem()),
    stats_(thread_system_.get()) {
    WorkBoundExpensiveOperationController::InitStats(&stats_);
    InitControllerWithLimit(1);
  }

  void InitControllerWithLimit(int limit) {
    controller_.reset(
        new WorkBoundExpensiveOperationController(limit, &stats_));
  }

  bool TryToWork() {
    TrackCallsFunction f;
    EXPECT_FALSE(f.run_called_ || f.cancel_called_);
    controller_->ScheduleExpensiveOperation(&f);
    EXPECT_TRUE(f.run_called_ || f.cancel_called_);
    return f.run_called_;
  }

 protected:
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;
  scoped_ptr<WorkBoundExpensiveOperationController> controller_;
};

TEST_F(WorkBoundExpensiveOperationTest, EmptyScheduleImmediately) {
  EXPECT_TRUE(TryToWork());
}

TEST_F(WorkBoundExpensiveOperationTest, ActuallyLimits) {
  EXPECT_TRUE(TryToWork());
  EXPECT_FALSE(TryToWork());
}

TEST_F(WorkBoundExpensiveOperationTest, LotsOfRequests) {
  InitControllerWithLimit(1000);
  for (int i = 0; i < 1000; ++i) {
    EXPECT_TRUE(TryToWork());
  }
  EXPECT_FALSE(TryToWork());
}

TEST_F(WorkBoundExpensiveOperationTest, NotifyDone) {
  EXPECT_TRUE(TryToWork());
  EXPECT_FALSE(TryToWork());
  controller_->NotifyExpensiveOperationComplete();
  EXPECT_TRUE(TryToWork());
}

TEST_F(WorkBoundExpensiveOperationTest, LimitZeroIsUnlimited) {
  InitControllerWithLimit(0);
  EXPECT_TRUE(TryToWork());
}

TEST_F(WorkBoundExpensiveOperationTest, LimitNegativeIsUnlimited) {
  InitControllerWithLimit(-1);
  EXPECT_TRUE(TryToWork());
}

}  // namespace
}  // namespace net_instaweb
