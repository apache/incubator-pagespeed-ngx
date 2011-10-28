/*
 * Copyright 2011 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)

// Unit-test for QueuedAlarm

#include "net/instaweb/util/public/queued_alarm.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/scheduler_thread.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {
namespace {

class QueuedAlarmTest : public WorkerTestBase {
 public:
  QueuedAlarmTest()
      : thread_system_(ThreadSystem::CreateThreadSystem()),
        sequence_(NULL),
        done_(false),
        cancel_(false) {
  }

  virtual ~QueuedAlarmTest() {
    ClearSequence();
  }

  MockScheduler* SetupWithMockScheduler() {
    MockTimer* timer = new MockTimer(0);
    MockScheduler* scheduler = new MockScheduler(thread_system_.get(), timer);
    timer_.reset(timer);
    scheduler_.reset(scheduler);
    SetupWorker();
    return scheduler;
  }

  void SetupWithRealScheduler() {
    timer_.reset(new GoogleTimer);
    scheduler_.reset(new Scheduler(thread_system_.get(), timer_.get()));
    SetupWorker();
  }

  void MakeSequence() {
    if (sequence_ == NULL) {
      sequence_ = worker_->NewSequence();
      // Take advantage of mock scheduler's quiescence detection.
      scheduler_->RegisterWorker(sequence_);
    }
  }

  void ClearSequence() {
    if (sequence_ != NULL) {
      scheduler_->UnregisterWorker(sequence_);
      worker_->FreeSequence(sequence_);
      sequence_ = NULL;
    }
  }

  void MarkDone() { done_ = true; }
  void MarkCancel() { cancel_ = true; }

  Scheduler* scheduler() { return scheduler_.get(); }
  QueuedWorkerPool::Sequence* sequence() { return sequence_; }
  Timer* timer() { return timer_.get(); }

 protected:
  void SetupWorker() {
    worker_.reset(new QueuedWorkerPool(2, thread_system_.get()));
    MakeSequence();
  }

  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<Timer> timer_;
  scoped_ptr<Scheduler> scheduler_;
  scoped_ptr<QueuedWorkerPool> worker_;
  QueuedWorkerPool::Sequence* sequence_;
  bool done_, cancel_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueuedAlarmTest);
};

// A class that keeps track of an alarm object and runs ops on it in a sequence.
class TestAlarmHandler {
 public:
  TestAlarmHandler(QueuedAlarmTest* fixture, WorkerTestBase::SyncPoint* sync)
      : fixture_(fixture),
        sync_(sync),
        alarm_(NULL),
        fired_(false) {}

  void StartAlarm() {
    fixture_->sequence()->Add(
        MakeFunction(this, &TestAlarmHandler::StartAlarmImpl));
  }

  void CancelAlarm() {
    fixture_->sequence()->Add(
        MakeFunction(this, &TestAlarmHandler::CancelAlarmImpl));
  }

  void FireAlarm() {
    // Should not have run CancelAlarm if we got here!
    EXPECT_TRUE(alarm_ != NULL);

    // Nothing more to cancel.
    alarm_ = NULL;
    fired_ = true;
  }

  bool fired() const { return fired_; }

 private:
  void StartAlarmImpl() {
    alarm_ =
        new QueuedAlarm(fixture_->scheduler(),
                        fixture_->sequence(),
                        fixture_->timer()->NowUs(),
                        MakeFunction(this, &TestAlarmHandler::FireAlarm));
  }

  void CancelAlarmImpl() {
    if (alarm_ != NULL) {
      alarm_->CancelAlarm();
      alarm_ = NULL;
    }

    // Note that we notify here, as this method will always run. In particular:
    // 1) If we get here first, FireAlarm will not fire, so we're fine.
    // 2) If we get here second, FireAlarm already ran, so we're fine to
    // cleanup, too.
    // (It makes sense to talk about us running before or after FireAlarm
    //  because it runs in the same sequence).
    sync_->Notify();
  }

  QueuedAlarmTest* fixture_;
  WorkerTestBase::SyncPoint* sync_;
  QueuedAlarm* alarm_;
  bool fired_;
  DISALLOW_COPY_AND_ASSIGN(TestAlarmHandler);
};

TEST_F(QueuedAlarmTest, BasicOperation) {
  const int kDelayUs = Timer::kMsUs;

  MockScheduler* scheduler = SetupWithMockScheduler();

  // Tests to make sure the alarm actually runs.
  new QueuedAlarm(scheduler, sequence_,
                  timer_->NowUs() + kDelayUs,
                  MakeFunction(static_cast<QueuedAlarmTest*>(this),
                               &QueuedAlarmTest::MarkDone,
                               &QueuedAlarmTest::MarkCancel));
  {
    ScopedMutex lock(scheduler->mutex());
    scheduler->ProcessAlarms(kDelayUs);
  }

  // Make sure to let the work threads complete.
  scheduler->AwaitQuiescence();
  EXPECT_TRUE(done_);
  EXPECT_FALSE(cancel_);
}

TEST_F(QueuedAlarmTest, BasicCancel) {
  const int kDelayUs = Timer::kMsUs;
  MockScheduler* scheduler = SetupWithMockScheduler();

  // Tests to make sure that we can cancel it.
  QueuedAlarm* alarm =
      new QueuedAlarm(scheduler, sequence_,
                      timer_->NowUs() + kDelayUs,
                      MakeFunction(static_cast<QueuedAlarmTest*>(this),
                                   &QueuedAlarmTest::MarkDone,
                                   &QueuedAlarmTest::MarkCancel));
  alarm->CancelAlarm();
  {
    ScopedMutex lock(scheduler->mutex());
    scheduler->ProcessAlarms(kDelayUs);
  }

  // Make sure to let the work threads complete.
  scheduler->AwaitQuiescence();
  EXPECT_FALSE(done_);
  EXPECT_TRUE(cancel_);
}

TEST_F(QueuedAlarmTest, RacingCancel) {
  const int kRuns = 1000;
  int fired = 0;

  // Test to make sure cases where cancel and alarm execution may be racing
  // are handled safely, without crashes or check failures.
  SetupWithRealScheduler();
  SchedulerThread* scheduler_thread =
      new SchedulerThread(thread_system_.get(), scheduler_.get());
  scheduler_thread->Start();

  for (int run = 0; run < kRuns; ++run) {
    SyncPoint sync(thread_system_.get());
    MakeSequence();
    TestAlarmHandler t(this, &sync);
    t.StartAlarm();

    // Unfortunately w/o a sleep here, the race is consistently won by
    // the cancellation.
    usleep(1);
    t.CancelAlarm();
    sync.Wait();
    ClearSequence();

    if (t.fired()) {
      ++fired;
    }
  }
  scheduler_thread->MakeDeleter()->CallRun();
  LOG(ERROR) << "Alarm fired in: " << fired  << "/" << kRuns;
}

}  // namespace
}  // namespace net_instaweb
