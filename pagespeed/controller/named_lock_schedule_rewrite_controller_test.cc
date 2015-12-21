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

#include "pagespeed/controller/named_lock_schedule_rewrite_controller.h"

#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/named_lock_tester.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/mem_lock_manager.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"

namespace net_instaweb {

namespace {

const char kKey1[] = "key1";
const char kKey2[] = "key2";

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

class NamedLockScheduleRewriteControllerTest : public testing::Test {
 public:
  NamedLockScheduleRewriteControllerTest()
      : thread_system_(Platform::CreateThreadSystem()),
        stats_(thread_system_.get()),
        timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms),
        lock_tester_(thread_system_.get()),
        lock_manager_(&timer_) {
    NamedLockScheduleRewriteController::InitStats(&stats_);
    controller_.reset(new NamedLockScheduleRewriteController(
        &lock_manager_, thread_system_.get(), &stats_));
  }

 protected:
  void CheckStats(int expected_num_granted, int expected_num_denied,
                  int expected_num_stolen, int expected_num_released_unheld,
                  int expected_currently_held) {
    EXPECT_EQ(
        expected_num_granted,
        TimedVariableTotal(NamedLockScheduleRewriteController::kLocksGranted));
    EXPECT_EQ(
        expected_num_denied,
        TimedVariableTotal(NamedLockScheduleRewriteController::kLocksDenied));
    EXPECT_EQ(
        expected_num_stolen,
        TimedVariableTotal(NamedLockScheduleRewriteController::kLocksStolen));
    EXPECT_EQ(
        expected_num_released_unheld,
        TimedVariableTotal(
            NamedLockScheduleRewriteController::kLocksReleasedWhenNotHeld));
    EXPECT_EQ(
        expected_currently_held,
        stats_.GetUpDownCounter(
                  NamedLockScheduleRewriteController::kLocksCurrentlyHeld)
            ->Get());
  }

  int64 TimedVariableTotal(const GoogleString& name) {
    return stats_.GetTimedVariable(name)->Get(TimedVariable::START);
  }

  void CheckLocked(const GoogleString& key) {
    scoped_ptr<NamedLock> lock(lock_manager_.CreateNamedLock(key));
    EXPECT_FALSE(lock_tester_.TryLock(lock.get()));
  }

  // Verifies the key isn't locked by attempting to take a lock for it. The
  // lock is automatically released at exit via the NamedLock destructor.
  void CheckUnlocked(const GoogleString& key) {
    scoped_ptr<NamedLock> lock(lock_manager_.CreateNamedLock(key));
    EXPECT_TRUE(lock_tester_.TryLock(lock.get()));
  }

  NamedLock* TakeLock(const GoogleString& key) {
    NamedLock* lock = lock_manager_.CreateNamedLock(key);
    // Should be ASSERT_TRUE, but gtest barfs.
    EXPECT_TRUE(lock_tester_.LockTimedWaitStealOld(0, 0, lock));
    return lock;
  }

  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;
  MockTimer timer_;
  NamedLockTester lock_tester_;
  MemLockManager lock_manager_;
  scoped_ptr<NamedLockScheduleRewriteController> controller_;
};

TEST_F(NamedLockScheduleRewriteControllerTest, EmptyScheduleImmediately) {
  CheckStats(0 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             0 /* current */);
  CheckUnlocked(kKey1);

  TrackCallsFunction f;
  EXPECT_FALSE(f.run_called_);
  EXPECT_FALSE(f.cancel_called_);

  controller_->ScheduleRewrite(kKey1, &f);
  CheckLocked(kKey1);
  CheckStats(1 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             1 /* current */);
  EXPECT_TRUE(f.run_called_);
  EXPECT_FALSE(f.cancel_called_);

  controller_->NotifyRewriteComplete(kKey1);
  CheckUnlocked(kKey1);
  CheckStats(1 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             0 /* current */);
}

TEST_F(NamedLockScheduleRewriteControllerTest, DuplicateKeysBlocked) {
  TrackCallsFunction f1;
  controller_->ScheduleRewrite(kKey1, &f1);
  CheckLocked(kKey1);
  CheckStats(1 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             1 /* current */);
  EXPECT_TRUE(f1.run_called_);
  EXPECT_FALSE(f1.cancel_called_);

  TrackCallsFunction f2;
  controller_->ScheduleRewrite(kKey1, &f2);
  CheckLocked(kKey1);
  CheckStats(1 /* granted */, 1 /* denied */, 0 /* stolen */, 0 /* unheld */,
             1 /* current */);
  EXPECT_FALSE(f2.run_called_);
  EXPECT_TRUE(f2.cancel_called_);

  controller_->NotifyRewriteComplete(kKey1);
  CheckUnlocked(kKey1);
  CheckStats(1 /* granted */, 1 /* denied */, 0 /* stolen */, 0 /* unheld */,
             0 /* current */);
}

// Mostly the same as DuplicateKeysBlocked, but the lock is obtained externally.
TEST_F(NamedLockScheduleRewriteControllerTest, ExternalLock) {
  scoped_ptr<NamedLock> lock(TakeLock(kKey1));

  TrackCallsFunction f;
  controller_->ScheduleRewrite(kKey1, &f);
  CheckStats(0 /* granted */, 1 /* denied */, 0 /* stolen */, 0 /* unheld */,
             0 /* current */);
  EXPECT_FALSE(f.run_called_);
  EXPECT_TRUE(f.cancel_called_);
}

TEST_F(NamedLockScheduleRewriteControllerTest, SimultaneousKeys) {
  TrackCallsFunction f1;
  controller_->ScheduleRewrite(kKey1, &f1);
  CheckLocked(kKey1);
  CheckStats(1 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             1 /* current */);
  EXPECT_TRUE(f1.run_called_);
  EXPECT_FALSE(f1.cancel_called_);

  TrackCallsFunction f2;
  controller_->ScheduleRewrite(kKey2, &f2);
  CheckLocked(kKey1);
  CheckLocked(kKey2);
  CheckStats(2 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             2 /* current */);
  EXPECT_TRUE(f2.run_called_);
  EXPECT_FALSE(f2.cancel_called_);

  controller_->NotifyRewriteComplete(kKey1);
  CheckUnlocked(kKey1);
  CheckLocked(kKey2);
  CheckStats(2 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             1 /* current */);

  controller_->NotifyRewriteComplete(kKey2);
  CheckUnlocked(kKey2);
  CheckStats(2 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             0 /* current */);
}

TEST_F(NamedLockScheduleRewriteControllerTest, Steal) {
  TrackCallsFunction f1;
  controller_->ScheduleRewrite(kKey1, &f1);
  CheckLocked(kKey1);
  CheckStats(1 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             1 /* current */);
  EXPECT_TRUE(f1.run_called_);
  EXPECT_FALSE(f1.cancel_called_);

  timer_.AdvanceMs(NamedLockScheduleRewriteController::kStealMs + 1);
  // This makes the test not crash ?? CheckUnlocked(kKey1);

  TrackCallsFunction f2;
  controller_->ScheduleRewrite(kKey1, &f2);
  CheckLocked(kKey1);
  CheckStats(2 /* granted */, 0 /* denied */, 1 /* stolen */, 0 /* unheld */,
             1 /* current */);
  EXPECT_TRUE(f2.run_called_);
  EXPECT_FALSE(f2.cancel_called_);

  controller_->NotifyRewriteComplete(kKey1);
  CheckUnlocked(kKey1);
  CheckStats(2 /* granted */, 0 /* denied */, 1 /* stolen */, 0 /* unheld */,
             0 /* current */);

  controller_->NotifyRewriteComplete(kKey1);
  CheckStats(2 /* granted */, 0 /* denied */, 1 /* stolen */, 1 /* unheld */,
             0 /* current */);
}

TEST_F(NamedLockScheduleRewriteControllerTest, StealExternalLock) {
  scoped_ptr<NamedLock> lock(TakeLock(kKey1));

  timer_.AdvanceMs(NamedLockScheduleRewriteController::kStealMs + 1);

  TrackCallsFunction f;
  controller_->ScheduleRewrite(kKey1, &f);
  CheckLocked(kKey1);
  // This doesn't report as stolen, because the controller wasn't tracking
  // the lock.
  CheckStats(1 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             1 /* current */);
  EXPECT_TRUE(f.run_called_);
  EXPECT_FALSE(f.cancel_called_);

  controller_->NotifyRewriteComplete(kKey1);
  CheckUnlocked(kKey1);
  CheckStats(1 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             0 /* current */);
}

TEST_F(NamedLockScheduleRewriteControllerTest, StolenOutFromUnder) {
  CheckStats(0 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             0 /* current */);
  CheckUnlocked(kKey1);

  TrackCallsFunction f;
  EXPECT_FALSE(f.run_called_);
  EXPECT_FALSE(f.cancel_called_);

  controller_->ScheduleRewrite(kKey1, &f);
  CheckLocked(kKey1);
  CheckStats(1 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             1 /* current */);
  EXPECT_TRUE(f.run_called_);
  EXPECT_FALSE(f.cancel_called_);

  timer_.AdvanceMs(NamedLockScheduleRewriteController::kStealMs + 1);
  scoped_ptr<NamedLock> lock(TakeLock(kKey1));

  controller_->NotifyRewriteComplete(kKey1);
  CheckLocked(kKey1);
  CheckStats(1 /* granted */, 0 /* denied */, 0 /* stolen */, 0 /* unheld */,
             0 /* current */);
}

TEST_F(NamedLockScheduleRewriteControllerTest, UnheldNoLock) {
  controller_->NotifyRewriteComplete(kKey1);
  CheckStats(0 /* granted */, 0 /* denied */, 0 /* stolen */, 1 /* unheld */,
             0 /* current */);
}

}  // namespace
}  // namespace net_instaweb
