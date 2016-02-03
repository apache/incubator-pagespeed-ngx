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
 */
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test for Scheduler::Sequence.

#include "pagespeed/kernel/thread/scheduler_sequence.h"

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/thread/mock_scheduler.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"
#include "pagespeed/kernel/thread/scheduler.h"
#include "pagespeed/kernel/thread/worker_test_base.h"

namespace net_instaweb {
namespace {

const int kCancelReduction = 100;
const int kTimeoutMs = 1000;  // Mock time, so it does not matter.

// A function that, without protection of a mutex, increments a shared
// integer.  The intent is that the Scheduler::Sequence is
// enforcing the sequentiality on our behalf so we don't have to worry
// about mutexing in here.
class Increment : public Function {
 public:
  Increment(int expected_value, int* count)
      : expected_value_(expected_value),
        count_(count) {
  }

 protected:
  void Run() override {
    ++*count_;
    EXPECT_EQ(expected_value_, *count_);
  }
  void Cancel() override {
    *count_ -= kCancelReduction;
    EXPECT_EQ(expected_value_, *count_);
  }

 private:
  int expected_value_;
  int* count_;

  DISALLOW_COPY_AND_ASSIGN(Increment);
};

class SchedulerSequenceTest: public WorkerTestBase {
 public:
  SchedulerSequenceTest()
      : count_(0),
        done_(false),
        timer_(thread_runtime_->NewMutex(), MockTimer::kApr_5_2010_ms),
        scheduler_(thread_runtime_.get(), &timer_),
        sync1_(thread_runtime_.get()),
        sequence_(scheduler_.NewSequence()) {
  }

  void SetDone() {
    done_ = true;
  }

  void Add10Tasks() {
    for (int i = 0; i < 10; ++i) {
      sequence_->Add(new Increment(i + 1, &count_));
    }
  }

  void AddSetDoneTask() {
    sequence_->Add(MakeFunction(this, &SchedulerSequenceTest::SetDone));
  }

 protected:
  // Blocks mainline until a sequence completes all outstanding tasks.
  void WaitUntilSequenceCompletes(Sequence* sequence) {
    SyncPoint done(thread_runtime_.get());
    sequence->Add(new NotifyRunFunction(&done));
    done.Wait();
  }

  int count_;
  bool done_;
  MockTimer timer_;
  MockScheduler scheduler_;
  SyncPoint sync1_;
  scoped_ptr<Scheduler::Sequence> sequence_;
};

TEST_F(SchedulerSequenceTest, RunOnRequestThread) {
  Add10Tasks();
  AddSetDoneTask();

  // We've added some tasks but nothing was run yet.
  EXPECT_EQ(0, count_);

  sequence_->Add(new Increment(  // This task will be cancelled.
      10 - kCancelReduction, &count_));
  {
    ScopedMutex lock(scheduler_.mutex());
    EXPECT_TRUE(sequence_->RunTasksUntil(kTimeoutMs, &done_));
  }
  EXPECT_EQ(10, count_);
  EXPECT_TRUE(done_);
  sequence_.reset();  // Cancels outstanding task.
  EXPECT_EQ(10 - kCancelReduction, count_);
}

TEST_F(SchedulerSequenceTest, Timeout) {
  Add10Tasks();

  // We've added some tasks but nothing was run yet.
  EXPECT_EQ(0, count_);
  {
    ScopedMutex lock(scheduler_.mutex());
    EXPECT_FALSE(sequence_->RunTasksUntil(kTimeoutMs, &done_));
  }
  EXPECT_EQ(10, count_);
  EXPECT_FALSE(done_);
}

TEST_F(SchedulerSequenceTest, RunOnRequestThreadThenSwitch) {
  Add10Tasks();
  AddSetDoneTask();
  sequence_->Add(new Increment(11, &count_));
  {
    ScopedMutex lock(scheduler_.mutex());
    EXPECT_TRUE(sequence_->RunTasksUntil(kTimeoutMs, &done_));
  }

  // SetDone got called before the callback that increments count to 11, so the
  // count is still at 10.
  EXPECT_EQ(10, count_);
  EXPECT_TRUE(done_);

  // Now forward the sequence, including the outstanding increment to 11, and
  // any new tasks, to a pool-based sequence that runs in a separate thread.
  scoped_ptr<QueuedWorkerPool> pool(new QueuedWorkerPool(
      2, "queued_worker_pool_test", thread_runtime_.get()));
  Sequence* pool_sequence = pool->NewSequence();
  {
    ScopedMutex lock(scheduler_.mutex());
    sequence_->ForwardToSequence(pool_sequence);
  }
  sequence_->Add(new Increment(12, &count_));
  WaitUntilSequenceCompletes(pool_sequence);
  EXPECT_EQ(12, count_);
}

}  // namespace

}  // namespace net_instaweb
