/*
 * Copyright 2016 Google Inc.
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

#include "pagespeed/controller/popularity_contest_schedule_rewrite_controller.h"

#include <algorithm>
#include <queue>

#include "base/logging.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"

using testing::Eq;
using testing::Gt;
using testing::IsEmpty;

namespace net_instaweb {

namespace {

const int kMaxRewrites = 2;
const int kMaxQueueLength = 5;

class TrackCallsFunction : public Function {
 public:
  TrackCallsFunction() : run_called_(false), cancel_called_(false) {
    set_delete_after_callback(false);
  }
  virtual ~TrackCallsFunction() { }

  void Run() override { run_called_ = true; }
  void Cancel() override { cancel_called_ = true; }

  bool run_called_;
  bool cancel_called_;
};

class RecordKeyFunction : public Function {
 public:
  RecordKeyFunction(const GoogleString& key, std::queue<GoogleString>* queue)
      : key_(key), queue_(queue) {
  }

  void Run() override {
    queue_->push(key_);
  }

  void Cancel() override {
    CHECK(false) << "Cancel called for key '" << key_ << "'";
  }

 private:
  const GoogleString key_;
  std::queue<GoogleString>* queue_;
};

}  // namespace

class PopularityContestScheduleRewriteControllerTest : public testing::Test {
 public:
  PopularityContestScheduleRewriteControllerTest()
      : thread_system_(Platform::CreateThreadSystem()),
        stats_(thread_system_.get()),
        timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms) {
    PopularityContestScheduleRewriteController::InitStats(&stats_);
    ResetController(kMaxRewrites, kMaxQueueLength);
  }

 protected:
  void ResetController(int max_rewrites, int max_queue) {
    controller_.reset(new PopularityContestScheduleRewriteController(
        thread_system_.get(), &stats_, &timer_, max_rewrites, max_queue));
  }

  // Schedule a rewrite from the Run() method of a Function. Useful for testing
  // re-entrancy safety. Whether the controller calls Run or Cancel for
  // bootstrap_key, arranges to invoke ScheduleRewrite for
  // (main_key,main_callback). If Run() is invoked for bootstrap_key, also runs
  // NotifyRewriteComplete(bootstrap_key).
  void ScheduleRewriteFromCallback(const GoogleString& bootstrap_key,
                                   const GoogleString& main_key,
                                   Function* main_callback) {
    // Make a function that will attempt to schedule another rewrite when it is
    // run. This will deadlock (ie: Timeout) if controller doesn't correctly
    // support this.
    Function* bootstrap_callback = MakeFunction(this,
        &PopularityContestScheduleRewriteControllerTest::
            ScheduleRewriteCallback,
        &PopularityContestScheduleRewriteControllerTest::CancelRewriteCallback,
        bootstrap_key, main_key, main_callback);
    // Schedule the function.
    controller_->ScheduleRewrite(bootstrap_key, bootstrap_callback);
  }

  // Explicitly not passing by const reference (here and below), because
  // MakeFunction doesn't handle that properly.
  void ScheduleRewriteCallback(GoogleString bootstrap_key,
                               GoogleString main_key,
                               Function* main_callback) {
    controller_->ScheduleRewrite(main_key, main_callback);
    controller_->NotifyRewriteComplete(bootstrap_key);
  }

  void CancelRewriteCallback(GoogleString bootstrap_key,
                             GoogleString main_key,
                             Function* main_callback) {
    controller_->ScheduleRewrite(main_key, main_callback);
  }

  void ScheduleRewriteAndAdvanceClock(const GoogleString& key, Function* cb) {
    controller_->ScheduleRewrite(key, cb);
    timer_.AdvanceMs(1);
  }

  void NotifyCompleteAndAdvanceClock(const GoogleString& key) {
    controller_->NotifyRewriteComplete(key);
    timer_.AdvanceMs(1);
  }

  void NotifyFailedAndAdvanceClock(const GoogleString& key) {
    controller_->NotifyRewriteFailed(key);
    timer_.AdvanceMs(1);
  }

  // TODO(cheesy): This is a bridge to avoid changing all call sites for
  // CheckStats. Consider removing it in a follow-up change.
  void CheckStats(int expected_num_requests, int expected_num_success,
                  int expected_num_failed, int expected_rejected_queue_size,
                  int expected_rejected_in_progress, int expected_queue_size,
                  int expected_running) {
    CheckStats(expected_num_requests, expected_num_success, expected_num_failed,
               expected_rejected_queue_size, expected_rejected_in_progress,
               expected_queue_size, expected_running,
               -1 /* expected_waiting_retry */);
  }

  void CheckStats(int expected_num_requests, int expected_num_success,
                  int expected_num_failed, int expected_rejected_queue_size,
                  int expected_rejected_in_progress, int expected_queue_size,
                  int expected_running, int expected_waiting_retry) {
    EXPECT_THAT(
        TimedVariableTotal(
            PopularityContestScheduleRewriteController::kNumRewritesRequested),
        Eq(expected_num_requests));
    EXPECT_THAT(
        TimedVariableTotal(
            PopularityContestScheduleRewriteController::kNumRewritesSucceeded),
        Eq(expected_num_success));
    EXPECT_THAT(
        TimedVariableTotal(
            PopularityContestScheduleRewriteController::kNumRewritesFailed),
        Eq(expected_num_failed));
    EXPECT_THAT(TimedVariableTotal(PopularityContestScheduleRewriteController::
                                       kNumRewritesRejectedQueueSize),
                Eq(expected_rejected_queue_size));
    EXPECT_THAT(TimedVariableTotal(PopularityContestScheduleRewriteController::
                                       kNumRewritesRejectedInProgress),
                Eq(expected_rejected_in_progress));
    EXPECT_THAT(
        CounterValue(
            PopularityContestScheduleRewriteController::kRewriteQueueSize),
        Eq(expected_queue_size));
    EXPECT_THAT(
        CounterValue(
            PopularityContestScheduleRewriteController::kNumRewritesRunning),
        Eq(expected_running));
    if (expected_waiting_retry >= 0) {
      EXPECT_THAT(CounterValue(PopularityContestScheduleRewriteController::
                                   kNumRewritesAwaitingRetry),
                  Eq(expected_waiting_retry));
    }
  }

  int64 TimedVariableTotal(const GoogleString& name) {
    return stats_.GetTimedVariable(name)->Get(TimedVariable::START);
  }

  int64 CounterValue(const GoogleString& name) {
    return stats_.GetUpDownCounter(name)->Get();
  }

  // Only use this if you absolutely need to resize the controller while it
  // contains items. Mostly you should be using ResetController instead.
  void SetMaxQueueSize(int size) {
    controller_->SetMaxQueueSizeForTesting(size);
  }

  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;
  MockTimer timer_;
  scoped_ptr<PopularityContestScheduleRewriteController> controller_;
};

namespace {

// Verify that a request placed into an empty popularlity contest is run
// immediately.
TEST_F(PopularityContestScheduleRewriteControllerTest, EmptyRunsImmediately) {
  CheckStats(0 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);

  TrackCallsFunction f;
  EXPECT_THAT(f.run_called_, Eq(false));
  EXPECT_THAT(f.cancel_called_, Eq(false));

  controller_->ScheduleRewrite("key1", &f);
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);
  EXPECT_THAT(f.run_called_, Eq(true));
  EXPECT_THAT(f.cancel_called_, Eq(false));

  controller_->NotifyRewriteComplete("key1");
  CheckStats(1 /* total */, 1 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Verify that its OK to call ScheduleRewrite in your callback.
TEST_F(PopularityContestScheduleRewriteControllerTest, ReentrantSchedule) {
  TrackCallsFunction f;
  ScheduleRewriteFromCallback("key1_bootstrap", "key1", &f);
  EXPECT_THAT(f.run_called_, Eq(true));
  CheckStats(2 /* total */, 1 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Drain queue.
  controller_->NotifyRewriteComplete("key1");
  CheckStats(2 /* total */, 2 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Verify that its OK to call ScheduleRewrite in a callback that runs in
// response to NotifyRewriteComplete. See also ReentrantScheduleAfterFailure.
TEST_F(PopularityContestScheduleRewriteControllerTest,
       ReentrantScheduleAfterComplete) {
  ResetController(1 /* max_rewrites */, kMaxQueueLength);

  // Schedule a rewrite to "plug up" the queue.
  TrackCallsFunction f;
  controller_->ScheduleRewrite("key1", &f);
  EXPECT_THAT(f.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Schedule a second rewrite that will call ScheduleRewrite when run.
  TrackCallsFunction f3;  // Associated with key3, not key2.
  ScheduleRewriteFromCallback("key2", "key3", &f3);
  EXPECT_THAT(f3.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(false));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Mark key1 as done. This should:
  //  - Trigger key2.
  //  - key2 will insert key3.
  //  - key2 will be mark itself done.
  //  - key3 will be triggered.
  // It will instead deadlock/timeout if re-entrancy is wrong.
  controller_->NotifyRewriteComplete("key1");
  EXPECT_THAT(f3.run_called_, Eq(true));
  CheckStats(3 /* total */, 2 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Drain queue.
  controller_->NotifyRewriteComplete("key3");
  CheckStats(3 /* total */, 3 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Verify that the callback for an older request is always Cancel()ed in favour
// of the newer one.
TEST_F(PopularityContestScheduleRewriteControllerTest, NewReplacesOld) {
  ResetController(1 /* max_rewrites */, kMaxQueueLength);

  // Schedule a rewrite so nothing else can be run.
  TrackCallsFunction f;
  controller_->ScheduleRewrite("key1", &f);
  EXPECT_THAT(f.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Queue a call for "key2". It should not run.
  TrackCallsFunction f2;
  controller_->ScheduleRewrite("key2", &f2);
  EXPECT_THAT(f2.run_called_, Eq(false));
  EXPECT_THAT(f2.cancel_called_, Eq(false));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Queue another call for "key2". It should also not run, but the old request
  // (f2) should be canceled.
  TrackCallsFunction f3;
  controller_->ScheduleRewrite("key2", &f3);
  EXPECT_THAT(f3.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(false));
  EXPECT_THAT(f2.cancel_called_, Eq(true));
  CheckStats(3 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Drain queue.
  controller_->NotifyRewriteComplete("key1");
  CheckStats(3 /* total */, 1 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  controller_->NotifyRewriteComplete("key2");
  CheckStats(3 /* total */, 2 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// As NewReplacesOld, but attempt to schedule something else when the old
// callback is canceled. This tests the locking/re-entrancy behaviour of that
// code path.
TEST_F(PopularityContestScheduleRewriteControllerTest,
       NewReplacesOldReentrant) {
  ResetController(1 /* max_rewrites */, kMaxQueueLength);

  // Schedule a rewrite so nothing else can be run.
  TrackCallsFunction f;
  controller_->ScheduleRewrite("key1", &f);
  EXPECT_THAT(f.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Queue a call for "key2" that will attempt to schedule "key3" when
  // Run/Canceled. It should not run.
  TrackCallsFunction f2;
  ScheduleRewriteFromCallback("key2", "key3", &f2);
  EXPECT_THAT(f2.run_called_, Eq(false));
  EXPECT_THAT(f2.cancel_called_, Eq(false));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Queue another call for "key2". It should also not run, but the old request
  // should be canceled. This should result in queueing "key3". If the
  // re-entrancy is wrong, this will deadlock/timeout.
  TrackCallsFunction f3;
  controller_->ScheduleRewrite("key2", &f3);
  EXPECT_THAT(f3.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(false));
  EXPECT_THAT(f2.run_called_, Eq(false));  // These trigger when "key3" runs.
  EXPECT_THAT(f2.cancel_called_, Eq(false));
  CheckStats(4 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Drain queue.
  controller_->NotifyRewriteComplete("key1");
  EXPECT_THAT(f2.run_called_, Eq(false));
  CheckStats(4 /* total */, 1 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  controller_->NotifyRewriteComplete("key2");
  EXPECT_THAT(f2.run_called_, Eq(true));
  CheckStats(4 /* total */, 2 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  controller_->NotifyRewriteComplete("key3");
  CheckStats(4 /* total */, 3 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Verify that if a rewrite is requested for a key that is currently running,
// it will immediately be rejected.
TEST_F(PopularityContestScheduleRewriteControllerTest, DuplicatesRejected) {
  // Start processing key1.
  TrackCallsFunction f;
  controller_->ScheduleRewrite("key1", &f);
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);
  EXPECT_THAT(f.run_called_, Eq(true));

  // Now try to process key1 while it's already running. It should be rejected.
  TrackCallsFunction f2;
  controller_->ScheduleRewrite("key1", &f2);
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 1 /* queue_size */, 1 /* running */);
  EXPECT_THAT(f2.cancel_called_, Eq(true));

  // Finish up key1.
  controller_->NotifyRewriteComplete("key1");
  CheckStats(2 /* total */, 1 /* success */, 0 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 0 /* queue_size */, 0 /* running */);

  // Now try key1 again and make sure it runs.
  TrackCallsFunction f3;
  controller_->ScheduleRewrite("key1", &f3);
  CheckStats(3 /* total */, 1 /* success */, 0 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 1 /* queue_size */, 1 /* running */);
  EXPECT_THAT(f3.run_called_, Eq(true));

  // Now try the same thing again, but try to start a rewrite from a callback.
  // Here we are trying to verify correct re-entrancy on this failure path.
  TrackCallsFunction f4;
  ScheduleRewriteFromCallback("key1", "key2", &f4);
  EXPECT_THAT(f4.run_called_, Eq(true));
  CheckStats(5 /* total */, 1 /* success */, 0 /* fail */, 0 /* queue_full */,
             2 /* already_running */, 2 /* queue_size */, 2 /* running */);

  // Dain queue.
  controller_->NotifyRewriteComplete("key1");
  CheckStats(5 /* total */, 2 /* success */, 0 /* fail */, 0 /* queue_full */,
             2 /* already_running */, 1 /* queue_size */, 1 /* running */);

  controller_->NotifyRewriteComplete("key2");
  CheckStats(5 /* total */, 3 /* success */, 0 /* fail */, 0 /* queue_full */,
             2 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Verify "NotifyRewriteFailed" path correctly reports statistics and
// runs subsequent jobs.
TEST_F(PopularityContestScheduleRewriteControllerTest, BasicFailure) {
  ResetController(1 /* max_rewrites */, kMaxQueueLength);

  // Start "key1" runnining immediately.
  TrackCallsFunction f;
  controller_->ScheduleRewrite("key1", &f);
  EXPECT_THAT(f.run_called_, Eq(true));
  EXPECT_THAT(f.cancel_called_, Eq(false));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Add "key2" to the queue, to be run later.
  TrackCallsFunction f2;
  controller_->ScheduleRewrite("key2", &f2);
  EXPECT_THAT(f2.run_called_, Eq(false));
  EXPECT_THAT(f2.cancel_called_, Eq(false));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Report failure of key1. Verify key2 is now run and the statistics are
  // correct. Note that key1 remains in the queue because of the failure.
  controller_->NotifyRewriteFailed("key1");
  EXPECT_THAT(f2.run_called_, Eq(true));
  CheckStats(2 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Drain queue.
  controller_->NotifyRewriteComplete("key2");
  CheckStats(2 /* total */, 1 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 0 /* running */);
}

// Verify that its OK to call ScheduleRewrite in a callback that runs in
// response to NotifyRewriteFailed. Counterpart to
// ReentrantScheduleAfterComplete.
TEST_F(PopularityContestScheduleRewriteControllerTest,
       ReentrantScheduleAfterFailure) {
  ResetController(1 /* max_rewrites */, kMaxQueueLength);

  // Schedule a rewrite to "plug up" the queue.
  TrackCallsFunction f;
  controller_->ScheduleRewrite("key1", &f);
  EXPECT_THAT(f.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Schedule a second rewrite that will call ScheduleRewrite when run.
  TrackCallsFunction f3;  // Associated with key3, not key2.
  ScheduleRewriteFromCallback("key2", "key3", &f3);
  EXPECT_THAT(f3.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(false));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Mark key1 as failed. This should:
  //  - Trigger key2.
  //  - key2 will insert key3.
  //  - key2 will be mark itself done.
  //  - key3 will be triggered.
  // It will instead deadlock/timeout if re-entrancy is wrong.
  // Note that "key1" remains in the popularity contest after this.
  controller_->NotifyRewriteFailed("key1");
  EXPECT_THAT(f3.run_called_, Eq(true));
  CheckStats(3 /* total */, 1 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Drain queue.
  controller_->NotifyRewriteComplete("key3");
  CheckStats(3 /* total */, 2 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 0 /* running */);
}

// Verify keys are run in order of popularity.
TEST_F(PopularityContestScheduleRewriteControllerTest, BasicPopularity) {
  ResetController(1 /* max_rewrites */, kMaxQueueLength);

  TrackCallsFunction f1;
  controller_->ScheduleRewrite("k1", &f1);  // k1 => 1 (run).
  EXPECT_THAT(f1.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  TrackCallsFunction f2;
  controller_->ScheduleRewrite("k2", &f2);  // k1 => 1 (run), k2 => 1.
  EXPECT_THAT(f2.run_called_, Eq(false));
  EXPECT_THAT(f2.cancel_called_, Eq(false));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  TrackCallsFunction f3;
  controller_->ScheduleRewrite("k3", &f3);  // k1 => 1 (run), k2 => 1, k3 => 1.
  EXPECT_THAT(f3.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(false));
  CheckStats(3 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // k2 should now have been raised above k3.
  TrackCallsFunction f2a;
  controller_->ScheduleRewrite("k2", &f2a);  // k1 => 1 (run), k2 => 2, k3 => 1.
  EXPECT_THAT(f2a.run_called_, Eq(false));
  EXPECT_THAT(f2a.cancel_called_, Eq(false));
  // The old one should have been canceled.
  EXPECT_THAT(f2.cancel_called_, Eq(true));
  CheckStats(4 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Mark k1 as complete and verify k2 runs, not k3 which was added first.
  controller_->NotifyRewriteComplete("k1");  // k2 => 2 (run), k3 => 1.
  EXPECT_THAT(f2a.run_called_, Eq(true));
  CheckStats(4 /* total */, 1 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  controller_->NotifyRewriteComplete("k2");  // k3 => 1 (run).
  EXPECT_THAT(f3.run_called_, Eq(true));
  CheckStats(4 /* total */, 2 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  controller_->NotifyRewriteComplete("k3");
  CheckStats(4 /* total */, 3 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Priorities should be preserved for keys that report failure
// (NotifyRewriteFailed). Verify that the priority of a key is actually
// preserved.
TEST_F(PopularityContestScheduleRewriteControllerTest,
       FailurePreservesPriority) {
  ResetController(1 /* max_rewrites */, kMaxQueueLength);

  // Start running "k1".
  TrackCallsFunction f1;
  controller_->ScheduleRewrite("k1", &f1);  // k1 => 1 (run).
  EXPECT_THAT(f1.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Put another couple of keys into the popularity contest, each at priority 1.
  TrackCallsFunction f2;
  controller_->ScheduleRewrite("k2", &f2);  // k1 => 1 (run), k2 => 1.
  EXPECT_THAT(f2.run_called_, Eq(false));
  EXPECT_THAT(f2.cancel_called_, Eq(false));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  TrackCallsFunction f3;
  controller_->ScheduleRewrite("k3", &f3);  // k1 => 1 (run), k2 => 1, k3 => 1.
  EXPECT_THAT(f3.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(false));
  CheckStats(3 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Mark k1 as having failed (NotifyRewriteFailed). It should remain in the
  // queue with its previous priority (1), which is reflected in the queue size.
  controller_->NotifyRewriteFailed("k1");  // k2 => 2 (run), k3 => 1, k1 => 1.
  EXPECT_THAT(f2.run_called_, Eq(true));
  CheckStats(3 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Put "k1" back in the queue. It should be inserted at its previous priority
  // plus 1.
  TrackCallsFunction f1a;
  controller_->ScheduleRewrite("k1", &f1a);  // k2 => 1 (run), k1 => 2, k3 => 1.
  EXPECT_THAT(f1a.run_called_, Eq(false));
  CheckStats(4 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Now verify that when k2 completes we run the callback for k1 and not k3.
  // (Compare this to SuccessForgetsPriority).
  controller_->NotifyRewriteComplete("k2");  // k3 => 1 (run), k1 => 1.
  EXPECT_THAT(f1a.run_called_, Eq(true));
  EXPECT_THAT(f3.run_called_, Eq(false));
  CheckStats(4 /* total */, 1 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Drain out the queue.
  controller_->NotifyRewriteComplete("k1");
  EXPECT_THAT(f3.run_called_, Eq(true));
  CheckStats(4 /* total */, 2 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  controller_->NotifyRewriteComplete("k3");
  CheckStats(4 /* total */, 3 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Counterpart to FailurePreservesPriority; Verify that for a successful
// rewrite (NotifyRewriteComplete), priority is not remembered across runs.
TEST_F(PopularityContestScheduleRewriteControllerTest, SuccessForgetsPriority) {
  ResetController(1 /* max_rewrites */, kMaxQueueLength);

  // Start running "k1".
  TrackCallsFunction f1;
  controller_->ScheduleRewrite("k1", &f1);  // k1 => 1 (run).
  EXPECT_THAT(f1.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Put another couple of keys into the popularity contest, each at priority 1.
  TrackCallsFunction f2;
  controller_->ScheduleRewrite("k2", &f2);  // k1 => 1 (run), k2 => 1.
  EXPECT_THAT(f2.run_called_, Eq(false));
  EXPECT_THAT(f2.cancel_called_, Eq(false));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  TrackCallsFunction f3;
  controller_->ScheduleRewrite("k3", &f3);  // k1 => 1 (run), k2 => 1, k3 => 1.
  EXPECT_THAT(f3.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(false));
  CheckStats(3 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Mark k1 as having succeeded (NotifyRewriteComplete). It should be
  // completely removed from the queue.
  controller_->NotifyRewriteComplete("k1");  // k2 => 2 (run), k3 => 1.
  EXPECT_THAT(f2.run_called_, Eq(true));
  CheckStats(3 /* total */, 1 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Put "k1" back in the queue. It should be inserted at priority 1 and not
  // "inherit" its previous priority.
  TrackCallsFunction f1a;
  controller_->ScheduleRewrite("k1", &f1a);  // k2 => 1 (run), k3 => 1, k1 => 1.
  EXPECT_THAT(f1a.run_called_, Eq(false));
  CheckStats(4 /* total */, 1 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Now verify that when k2 completes we run the callback for k3 and not k1.
  // (Compare this to FailurePreservesPriority).
  controller_->NotifyRewriteComplete("k2");  // k3 => 1 (run), k1 => 1.
  EXPECT_THAT(f3.run_called_, Eq(true));
  EXPECT_THAT(f1a.run_called_, Eq(false));
  CheckStats(4 /* total */, 2 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Drain out the queue.
  controller_->NotifyRewriteComplete("k3");
  EXPECT_THAT(f1a.run_called_, Eq(true));
  CheckStats(4 /* total */, 3 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  controller_->NotifyRewriteComplete("k1");
  CheckStats(4 /* total */, 4 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Counterpart to FailurePreservesPriority; Verify that priorities for failing
// keys are preserved even if the queue drains completely. Also verifies that
// the popularity contest doesn't try to do anything if it only contains
// a "hold-over" priority.
TEST_F(PopularityContestScheduleRewriteControllerTest,
       FailedPriorityIsRememberedAcrossEmptyQueue) {
  ResetController(1 /* max_rewrites */, kMaxQueueLength);

  // Start running "k1".
  TrackCallsFunction f1;
  controller_->ScheduleRewrite("k1", &f1);  // k1 => 1 (run).
  EXPECT_THAT(f1.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Mark k1 as having failed. The popularity contest should now contain only a
  // single entry for k1, that isn't runnable.
  controller_->NotifyRewriteFailed("k1");  // (k1 => 1).
  CheckStats(1 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 0 /* running */);

  // Start running "k4".
  TrackCallsFunction f4;
  controller_->ScheduleRewrite("k4", &f4);  // k4 => 1 (run), (k1 => 1).
  EXPECT_THAT(f4.run_called_, Eq(true));
  CheckStats(2 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Mark "k4" as done. Verifies that the popularity contest will not try to run
  // "k1".
  controller_->NotifyRewriteComplete("k4");  // (k1 => 1).
  CheckStats(2 /* total */, 1 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 0 /* running */);

  // Start running "k2".
  TrackCallsFunction f2;
  controller_->ScheduleRewrite("k2", &f2);  // k2 => 1 (run), (k1 => 1).
  EXPECT_THAT(f2.run_called_, Eq(true));
  CheckStats(3 /* total */, 1 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Put "k3" into the queue.
  TrackCallsFunction f3;
  controller_->ScheduleRewrite("k3", &f3);  // k2 => 1 (run), k3 => 1, (k1 => 1)
  EXPECT_THAT(f3.run_called_, Eq(false));
  CheckStats(4 /* total */, 1 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Put "k1" back in the queue. It should be inserted at its previous priority
  // plus 1.
  TrackCallsFunction f1a;
  controller_->ScheduleRewrite("k1", &f1a);  // k2 => 1 (run), k1 => 2, k3 => 1.
  EXPECT_THAT(f1a.run_called_, Eq(false));
  CheckStats(5 /* total */, 1 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Now verify that when k2 completes we run the callback for k1 and not k3
  // (as per SuccessForgetsPriority).
  controller_->NotifyRewriteComplete("k2");  // k1 => 2 (run), k3 => 1.
  EXPECT_THAT(f1a.run_called_, Eq(true));
  EXPECT_THAT(f3.run_called_, Eq(false));
  CheckStats(5 /* total */, 2 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Drain out the queue.
  controller_->NotifyRewriteComplete("k1");  // k3 => 1 (run).
  EXPECT_THAT(f3.run_called_, Eq(true));
  CheckStats(5 /* total */, 3 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  controller_->NotifyRewriteComplete("k3");
  CheckStats(5 /* total */, 4 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Combination of FailurePreservesPriority and DuplicatesRejected; If a key
// is requested during run and then subsequently fails, verify that the request
// was rejected but still caused the priority to increase, applying to a re-run.
TEST_F(PopularityContestScheduleRewriteControllerTest,
       PriorityIsIncrementedDuringRun) {
  ResetController(1 /* max_rewrites */, kMaxQueueLength);

  // Start running "k1".
  TrackCallsFunction f1;
  controller_->ScheduleRewrite("k1", &f1);  // k1 => 1 (run).
  EXPECT_THAT(f1.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Attempt another run for "k1". It should be rejected, but the priority
  // should still be increased.
  TrackCallsFunction f1a;
  controller_->ScheduleRewrite("k1", &f1a);  // k1 => 2 (run).
  EXPECT_THAT(f1a.cancel_called_, Eq(true));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Put another couple of keys on the queue, at priority 1.
  TrackCallsFunction f2;
  controller_->ScheduleRewrite("k2", &f2);  // k1 => 2 (run), k2 => 1.
  EXPECT_THAT(f2.run_called_, Eq(false));
  EXPECT_THAT(f2.cancel_called_, Eq(false));
  CheckStats(3 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 2 /* queue_size */, 1 /* running */);

  TrackCallsFunction f3;
  controller_->ScheduleRewrite("k3", &f3);  // k1 => 2 (run), k2 => 1, k3 => 1.
  EXPECT_THAT(f3.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(false));
  CheckStats(4 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Mark k1 as having failed. It should remain in the queue with its
  // incremented priority.
  controller_->NotifyRewriteFailed("k1");  // k2 => 2 (run), k3 => 1, (k1 => 2).
  EXPECT_THAT(f2.run_called_, Eq(true));
  CheckStats(4 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Raise the priority of k3 to 2.
  TrackCallsFunction f3a;
  controller_->ScheduleRewrite("k3", &f3a);  // k2 => 2(run), k3 => 2, (k1 => 2)
  EXPECT_THAT(f3a.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(true));
  CheckStats(5 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Put "k1" back in the queue. It should be inserted at its previous priority
  // plus 1, which is 3 in this case.
  TrackCallsFunction f1b;
  controller_->ScheduleRewrite("k1", &f1b);  // k2 => 1 (run), k1 => 3, k3 => 2.
  EXPECT_THAT(f1b.run_called_, Eq(false));
  CheckStats(6 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */);

  // Now verify that when k2 completes we run the callback for k1 and not k3.
  controller_->NotifyRewriteComplete("k2");  // k1 => 3 (run), k3 => 2.
  EXPECT_THAT(f1b.run_called_, Eq(true));
  EXPECT_THAT(f3a.run_called_, Eq(false));
  CheckStats(6 /* total */, 1 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 2 /* queue_size */, 1 /* running */);

  // Drain queue.
  controller_->NotifyRewriteComplete("k1");
  EXPECT_THAT(f3a.run_called_, Eq(true));
  CheckStats(6 /* total */, 2 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 1 /* queue_size */, 1 /* running */);

  controller_->NotifyRewriteComplete("k3");
  CheckStats(6 /* total */, 3 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Verify that, if the queue is full, a queued retry will be dropped to make
// room for another rewrite.
TEST_F(PopularityContestScheduleRewriteControllerTest, OldRetryEviction) {
  ResetController(1 /* max_rewrites */, 3 /* max_queue_length */);

  // Start running "k1".
  TrackCallsFunction f1;
  ScheduleRewriteAndAdvanceClock("k1", &f1);  // k1 => 1 (run).
  EXPECT_THAT(f1.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  // Try and run k1 again, to increment the priority.
  TrackCallsFunction f1a;
  ScheduleRewriteAndAdvanceClock("k1", &f1a);  // k1 => 2 (run).
  EXPECT_THAT(f1a.cancel_called_, Eq(true));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 1 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  // Mark "k1" as failed. This should queue it for retry.
  NotifyFailedAndAdvanceClock("k1");  // (k1 => 2).
  CheckStats(2 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 1 /* queue_size */, 0 /* running */,
             1 /* queued_retries */);

  // Start running "k2".
  TrackCallsFunction f2;
  ScheduleRewriteAndAdvanceClock("k2", &f2);  // k2 => 1 (run), (k1 => 2).
  EXPECT_THAT(f2.run_called_, Eq(true));
  CheckStats(3 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 2 /* queue_size */, 1 /* running */,
             1 /* queued_retries */);

  // Queue "k3".
  TrackCallsFunction f3;
  ScheduleRewriteAndAdvanceClock("k3", &f3);  // k2 =>1(run), k3 => 1, (k1 => 2)
  EXPECT_THAT(f3.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(false));
  CheckStats(4 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */,
             1 /* queued_retries */);

  // Queue "k4". This should push out k1.
  TrackCallsFunction f4;
  ScheduleRewriteAndAdvanceClock("k4", &f4);  // k2 => 1(run), k3 => 1, k4 => 1.
  EXPECT_THAT(f4.run_called_, Eq(false));
  EXPECT_THAT(f4.cancel_called_, Eq(false));
  CheckStats(5 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  // Mark k2 as done.
  NotifyCompleteAndAdvanceClock("k2");  // k3 => 1 (run), k4 => 1.
  EXPECT_THAT(f3.run_called_, Eq(true));
  CheckStats(5 /* total */, 1 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 2 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  // Re-add k1. It should be queued with a priority of 1. It would be 3 if it
  // had not been flushed above.
  TrackCallsFunction f1b;
  ScheduleRewriteAndAdvanceClock("k1", &f1b);  // k3 => 1(run), k4 => 1, k1 => 1
  EXPECT_THAT(f1b.run_called_, Eq(false));
  EXPECT_THAT(f1b.cancel_called_, Eq(false));
  CheckStats(6 /* total */, 1 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  // Mark k3 as having succeeded. k4 (*not* k1) should run.
  NotifyCompleteAndAdvanceClock("k3");  // k4 => 1 (run), k1 => 1
  EXPECT_THAT(f4.run_called_, Eq(true));
  EXPECT_THAT(f1b.run_called_, Eq(false));
  CheckStats(6 /* total */, 2 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 2 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  // Drain queue.
  NotifyCompleteAndAdvanceClock("k4");
  EXPECT_THAT(f1b.run_called_, Eq(true));
  CheckStats(6 /* total */, 3 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 1 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  NotifyCompleteAndAdvanceClock("k1");
  CheckStats(6 /* total */, 4 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 0 /* queue_size */, 0 /* running */,
             0 /* queued_retries */);
}

// Verify that when the queue fills, queued retries are dropped in order of
// oldest first, not order of priority.
TEST_F(PopularityContestScheduleRewriteControllerTest,
       RetryEvictionAgeBeforePriority) {
  ResetController(1 /* max_rewrites */, 3 /* max_queue_length */);

  // Start running "k1".
  TrackCallsFunction f1;
  ScheduleRewriteAndAdvanceClock("k1", &f1);  // k1 => 1 (run).
  EXPECT_THAT(f1.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  // Try and run k1 again, to increment the priority.
  TrackCallsFunction f1a;
  ScheduleRewriteAndAdvanceClock("k1", &f1a);  // k1 => 2 (run).
  EXPECT_THAT(f1a.cancel_called_, Eq(true));
  CheckStats(2 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 1 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  // Mark "k1" as failed. This should queue it for retry.
  NotifyFailedAndAdvanceClock("k1");  // (k1 => 2).
  CheckStats(2 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 1 /* queue_size */, 0 /* running */,
             1 /* queued_retries */);

  // Start running "k2".
  TrackCallsFunction f2;
  ScheduleRewriteAndAdvanceClock("k2", &f2);  // k2 => 1, (k1 => 2).
  EXPECT_THAT(f2.run_called_, Eq(true));
  CheckStats(3 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 2 /* queue_size */, 1 /* running */,
             1 /* queued_retries */);

  // Mark "k2" as failed. This should queue it for retry, too.
  NotifyFailedAndAdvanceClock("k2");  // (k1 => 2), (k2 => 1).
  CheckStats(3 /* total */, 0 /* success */, 2 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 2 /* queue_size */, 0 /* running */,
             2 /* queued_retries */);

  // Start running k3. The queue is now full.
  TrackCallsFunction f3;
  ScheduleRewriteAndAdvanceClock("k3", &f3);  // k3=>1 (run), (k1=>2), (k2=>1).
  EXPECT_THAT(f3.run_called_, Eq(true));
  CheckStats(4 /* total */, 0 /* success */, 2 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */,
             2 /* queued_retries */);

  // Queue k4. This should push out k1.
  TrackCallsFunction f4;
  ScheduleRewriteAndAdvanceClock("k4", &f4);  // k3=>1(run), k4=1, (k2=>1).
  EXPECT_THAT(f4.run_called_, Eq(false));
  EXPECT_THAT(f4.cancel_called_, Eq(false));
  CheckStats(5 /* total */, 0 /* success */, 2 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */,
             1 /* queued_retries */);

  // Mark k3 done. This should start k4.
  NotifyCompleteAndAdvanceClock("k3");  // k4=1 (run), (k2=>1).
  EXPECT_THAT(f4.run_called_, Eq(true));
  CheckStats(5 /* total */, 1 /* success */, 2 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 2 /* queue_size */, 1 /* running */,
             1 /* queued_retries */);

  // Now we enqueue k1 and k2 again.
  TrackCallsFunction f1b;
  ScheduleRewriteAndAdvanceClock("k1", &f1b);  // k4=1 (run), k1 => 1, (k2=>1).
  EXPECT_THAT(f1b.run_called_, Eq(false));
  EXPECT_THAT(f1b.cancel_called_, Eq(false));
  CheckStats(6 /* total */, 1 /* success */, 2 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */,
             1 /* queued_retries */);

  TrackCallsFunction f2a;
  ScheduleRewriteAndAdvanceClock("k2", &f2a);  // k4=1 (run), k2 => 2, k1 => 1.
  EXPECT_THAT(f2a.run_called_, Eq(false));
  EXPECT_THAT(f2a.cancel_called_, Eq(false));
  CheckStats(7 /* total */, 1 /* success */, 2 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 3 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  // Now, mark k4 as done. k2 should be executed because it had a saved value
  // of 2. If k1 was not ejected, it would be run now instead.
  NotifyCompleteAndAdvanceClock("k4");  // k2 => 2 (run), k1 => 1.
  EXPECT_THAT(f1b.run_called_, Eq(false));
  EXPECT_THAT(f2a.run_called_, Eq(true));
  CheckStats(7 /* total */, 2 /* success */, 2 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 2 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  // Drain queue.
  NotifyCompleteAndAdvanceClock("k2");  // k1 => 1 (run).
  EXPECT_THAT(f1b.run_called_, Eq(true));
  CheckStats(7 /* total */, 3 /* success */, 2 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 1 /* queue_size */, 1 /* running */,
             0 /* queued_retries */);

  NotifyCompleteAndAdvanceClock("k1");
  CheckStats(7 /* total */, 4 /* success */, 2 /* fail */, 0 /* queue_full */,
             1 /* already_running */, 0 /* queue_size */, 0 /* running */,
             0 /* queued_retries */);
}

// Verify the two queue bounds (max running, max queued).
TEST_F(PopularityContestScheduleRewriteControllerTest, QueueFills) {
  // The test is agnostic to the values of kMaxRewrites and kMaxQueueLength, but
  // kMaxQueueLength must be > kMaxRewrites for a full test, so that's asserted.
  ASSERT_THAT(kMaxQueueLength, Gt(kMaxRewrites));

  // Since they are equally weighted, the keys come out of the popularity
  // contest in undefined order. This is important later, because if you call
  // NotifyRewriteComplete on a key that's not been triggered, the popularity
  // contest will CHECK-fail. So, we use RecordKeyFunction to keep a track of
  // which keys were triggered.
  std::queue<GoogleString> pending_rewrites;

  // Queue kMaxQueueLength functions.
  for (int i = 1; i <= kMaxQueueLength; ++i) {
    const GoogleString key = IntegerToString(i);
    controller_->ScheduleRewrite(key,
                                 new RecordKeyFunction(key, &pending_rewrites));

    // After kMaxRewrites, the functions should stop running and start queueing.
    int expected_running = std::min(i, kMaxRewrites);

    CheckStats(i /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
               0 /* already_running */, i /* queue_size */,
               expected_running /* running */);
    EXPECT_THAT(pending_rewrites.size(), Eq(expected_running));
  }

  // Double-check that the number of "running" rewrites matches the constant.
  EXPECT_THAT(pending_rewrites.size(), Eq(kMaxRewrites));

  // Now check that we can't queue another rewrite.
  TrackCallsFunction f;
  controller_->ScheduleRewrite("unused", &f);
  EXPECT_THAT(f.cancel_called_, Eq(true));
  CheckStats(kMaxQueueLength + 1 /* total */, 0 /* success */, 0 /* fail */,
             1 /* queue_full */, 0 /* already_running */,
             kMaxQueueLength /* queue_size */, kMaxRewrites /* running */);

  // Now run through all the operations and process them.
  for (int i = 1; i <= kMaxQueueLength; ++i) {
    // Mark the most recently initiated work as complete.
    ASSERT_THAT(pending_rewrites.size(), Gt(0));
    controller_->NotifyRewriteComplete(pending_rewrites.front());
    pending_rewrites.pop();

    int expected_running = std::min(kMaxQueueLength - i, kMaxRewrites);
    EXPECT_THAT(pending_rewrites.size(), Eq(expected_running));
    CheckStats(kMaxQueueLength + 1 /* total */, i /* success */, 0 /* fail */,
               1 /* queue_full */, 0 /* already_running */,
               kMaxQueueLength - i /* queue_size */,
               expected_running /* running */);
  }

  // Double check that nothing is left "running".
  EXPECT_THAT(pending_rewrites, IsEmpty());

  // Double check that the contest reports empty.
  CheckStats(kMaxQueueLength + 1 /* total */, kMaxQueueLength /* success */,
             0 /* fail */, 1 /* queue_full */, 0 /* already_running */,
             0 /* queue_size */, 0 /* running */);
}

TEST_F(PopularityContestScheduleRewriteControllerTest, QueueFullReentrancy) {
  ResetController(1 /* max_rewrites */, 1 /* max_queue_length */);

  // Schedule a rewrite to "plug up" the queue.
  TrackCallsFunction f;
  controller_->ScheduleRewrite("key1", &f);
  EXPECT_THAT(f.run_called_, Eq(true));
  CheckStats(1 /* total */, 0 /* success */, 0 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Attempt to schedule a second rewrite, which should fail because the queue
  // is full. This should:
  //  - Immediately reject key2.
  //  - key2's Cancel will attempt to insert key3.
  //  - key3's Cancel will be run.
  // It will instead deadlock/timeout if re-entrancy is wrong.
  TrackCallsFunction f3;  // Associated with key3, not key2.
  ScheduleRewriteFromCallback("key2", "key3", &f3);
  EXPECT_THAT(f3.run_called_, Eq(false));
  EXPECT_THAT(f3.cancel_called_, Eq(true));
  CheckStats(3 /* total */, 0 /* success */, 0 /* fail */, 2 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 1 /* running */);

  // Drain queue.
  controller_->NotifyRewriteComplete("key1");
  CheckStats(3 /* total */, 1 /* success */, 0 /* fail */, 2 /* queue_full */,
             0 /* already_running */, 0 /* queue_size */, 0 /* running */);
}

// Run a series of rewrites to verify that the secondary queue for expired
// rewrites functions as expected. This test puts a number of failures into
// the retry queue and then pushes them out one by one. It is an exercise of
// the retry queue but also verifies that the retry queue prioritises time
// enqueued over priority.
TEST_F(PopularityContestScheduleRewriteControllerTest, OldRetryTortureTest) {
  const int kNumFailuresToTest = 100;
  const int kQueueSize = kNumFailuresToTest + 1;
  ResetController(1 /* max_rewrites */, kQueueSize);

  // Preload a failure for "0" so it winds up with a priority of 2, not 1.
  {
    TrackCallsFunction f;
    ScheduleRewriteAndAdvanceClock("0", &f);
    EXPECT_THAT(f.run_called_, Eq(true));
    NotifyFailedAndAdvanceClock("0");
  }

  CheckStats(1 /* total */, 0 /* success */, 1 /* fail */, 0 /* queue_full */,
             0 /* already_running */, 1 /* queue_size */, 0 /* running */,
             1 /* queued_retries */);

  std::queue<GoogleString> active_rewrites;
  int cumulative_successes = 0;
  int cumulative_failures = 1;
  for (int i = 0; i < kNumFailuresToTest; ++i) {
    // We want to engineer it so that there is a queued rewrite for key "i"
    // with has a higher priority than all the other keys, but is first to be
    // removed from the queue due to age. The previous iteration (or bootstrap)
    // incremented the priority of the queued retry key for "i", which will have
    // brought it to the top. So, now we engineer a failure for all keys, in
    // order. This makes sure that "i" was touched the longest ago, which should
    // put it first in line to be dropped.
    for (int j = i; j < kNumFailuresToTest; ++j) {
      const GoogleString key = IntegerToString(j);
      TrackCallsFunction f;
      ScheduleRewriteAndAdvanceClock(key, &f);
      NotifyFailedAndAdvanceClock(key);
      ++cumulative_failures;
    }

    CheckStats(
        1 + i * (kNumFailuresToTest + 3) + kNumFailuresToTest /* total */,
        cumulative_successes /* success */,
        cumulative_failures /* fail */, 0 /* queue_full */,
        0 /* already_running */, kNumFailuresToTest - i /* queue_size */,
        0 /* running */, kNumFailuresToTest - i /* queued_retries */);

    // Plug up the head of the queue.
    const GoogleString queue_block_key("block");
    TrackCallsFunction queue_block_callback;
    ScheduleRewriteAndAdvanceClock(queue_block_key, &queue_block_callback);

    // Schedule something that should already exist in the retry queue.
    const GoogleString hi_priority_key(IntegerToString(i + 1));
    TrackCallsFunction hi_priority_callback;
    ScheduleRewriteAndAdvanceClock(hi_priority_key, &hi_priority_callback);
    EXPECT_THAT(hi_priority_callback.run_called_, Eq(false));
    EXPECT_THAT(hi_priority_callback.cancel_called_, Eq(false));

    // We know that there are kNumFailuresToTest - i queued retries right now.
    // We want to queue just enough rewrites to push out i, but not i + 1.
    // This value is clamped, otherwise the terminal case will over-fill the
    // queue.
    int num_dummies_to_insert = std::min(i + 1, kNumFailuresToTest - 1);
    for (int j = 0; j < num_dummies_to_insert; ++j) {
      const GoogleString key("dummy-" + IntegerToString(j));
      ScheduleRewriteAndAdvanceClock(
          key, new RecordKeyFunction(key, &active_rewrites));
    }

    // Verify that the queue is actually full (queue_size == kQueueSize).
    CheckStats(3 + i * (kNumFailuresToTest + 3) + kNumFailuresToTest +
                   num_dummies_to_insert /* total */,
               cumulative_successes /* success */,
               cumulative_failures /* fail */, 0 /* queue_full */,
               0 /* already_running */, kQueueSize /* queue_size */,
               1 /* running */,
               kNumFailuresToTest - i - 2 /* queued_retries */);

    // Now that we have pushed key "i" out from the retry queue, we need to
    // re-insert it into the main queue. Unfortunately, we pushed "i" out by
    // filling the queue, so we can no longer insert it. We also can't remove
    // one of the dummy entries, because that is not supported by the popularity
    // contest API. So, we cheat and temporarily raise the limit of the queue.
    SetMaxQueueSize(kQueueSize + 1);

    const GoogleString i_as_string(IntegerToString(i));
    ScheduleRewriteAndAdvanceClock(
        i_as_string, new RecordKeyFunction(i_as_string, &active_rewrites));

    // Mark the head entry as complete. This will now run the highest priority
    // entry. That should be the one for i + 1 (hi_priority) and not the one
    // for i.
    NotifyCompleteAndAdvanceClock(queue_block_key);
    ++cumulative_successes;
    ASSERT_THAT(hi_priority_callback.run_called_, Eq(true));
    EXPECT_THAT(active_rewrites, IsEmpty());

    // Restore size back to normal.
    SetMaxQueueSize(kQueueSize);

    CheckStats(4 + i * (kNumFailuresToTest + 3) + kNumFailuresToTest +
                   num_dummies_to_insert /* total */,
               cumulative_successes /* success */,
               cumulative_failures /* fail */, 0 /* queue_full */,
               0 /* already_running */, kQueueSize /* queue_size */,
               1 /* running */,
               kNumFailuresToTest - i - 2 /* queued_retries */);

    // This results in a queued retry with an incremented priority.
    NotifyFailedAndAdvanceClock(hi_priority_key);
    ++cumulative_failures;

    // Run all the outstanding rewrites, making sure that we did actually
    // run the one for "i".
    bool saw_rewrite_for_i = false;
    for (int j = 0; j < num_dummies_to_insert + 1; ++j) {
      ASSERT_THAT(active_rewrites.size(), Gt(0));
      const GoogleString& front = active_rewrites.front();
      if (front == i_as_string) {
        saw_rewrite_for_i = true;
      }
      NotifyCompleteAndAdvanceClock(front);
      active_rewrites.pop();
      ++cumulative_successes;
    }

    EXPECT_THAT(saw_rewrite_for_i, Eq(true));
    ASSERT_THAT(active_rewrites, IsEmpty());

    // This never hits zero because we always fail the i'th value.
    int expected_queued_items =
        (i == kNumFailuresToTest - 1) ? 1 : kNumFailuresToTest - i - 1;
    CheckStats(4 + i * (kNumFailuresToTest + 3) + kNumFailuresToTest +
                   num_dummies_to_insert /* total */,
               cumulative_successes /* success */,
               cumulative_failures /* fail */, 0 /* queue_full */,
               0 /* already_running */,
               expected_queued_items /* queue_size */, 0 /* running */,
               expected_queued_items /* queued_retries */);
  }
}

}  // namespace
}  // namespace net_instaweb
