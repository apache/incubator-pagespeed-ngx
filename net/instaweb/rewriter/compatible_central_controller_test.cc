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

#include "net/instaweb/rewriter/public/compatible_central_controller.h"

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

class CompatibleCentralControllerTest : public testing::Test {
 public:
  CompatibleCentralControllerTest() :
    thread_system_(Platform::CreateThreadSystem()),
    stats_(thread_system_.get()) {
    CompatibleCentralController::InitStats(&stats_);
    central_controller_.reset(new CompatibleCentralController(1, &stats_));
  }

 protected:
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;
  scoped_ptr<CompatibleCentralController> central_controller_;
};

TEST_F(CompatibleCentralControllerTest, EmptyScheduleImmediately) {
  TrackCallsFunction f;
  EXPECT_FALSE(f.run_called_);
  EXPECT_FALSE(f.cancel_called_);
  central_controller_->ScheduleExpensiveOperation(&f);
  EXPECT_TRUE(f.run_called_);
  EXPECT_FALSE(f.cancel_called_);
}

TEST_F(CompatibleCentralControllerTest, ActuallyLimits) {
  TrackCallsFunction f1;
  TrackCallsFunction f2;
  central_controller_->ScheduleExpensiveOperation(&f1);
  central_controller_->ScheduleExpensiveOperation(&f2);
  EXPECT_TRUE(f1.run_called_);
  EXPECT_FALSE(f1.cancel_called_);
  EXPECT_FALSE(f2.run_called_);
  EXPECT_TRUE(f2.cancel_called_);
}

TEST_F(CompatibleCentralControllerTest, NotifyDone) {
  TrackCallsFunction f1;
  TrackCallsFunction f2;
  central_controller_->ScheduleExpensiveOperation(&f1);
  central_controller_->NotifyExpensiveOperationComplete();
  central_controller_->ScheduleExpensiveOperation(&f2);
  EXPECT_TRUE(f1.run_called_);
  EXPECT_FALSE(f1.cancel_called_);
  EXPECT_TRUE(f2.run_called_);
  EXPECT_FALSE(f2.cancel_called_);
}

}  // namespace
}  // namespace net_instaweb
