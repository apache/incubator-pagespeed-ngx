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
// Author: morlovich@google.com (Maksim Orlovich)
//
// Unit tests for SchedulerThread

#include "net/instaweb/util/public/scheduler_thread.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

namespace {

class SchedulerThreadTest : public WorkerTestBase {
 protected:
  SchedulerThreadTest()
      : thread_system_(ThreadSystem::CreateThreadSystem()),
        timer_(),
        scheduler_(thread_system_.get(), &timer_),
        scheduler_thread_(
            new SchedulerThread(thread_system_.get(), &scheduler_)) {}

  scoped_ptr<ThreadSystem> thread_system_;
  GoogleTimer timer_;
  Scheduler scheduler_;
  SchedulerThread* scheduler_thread_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerThreadTest);
};

TEST_F(SchedulerThreadTest, BasicOperation) {
  // Make sure that the thread actually dispatches an event,
  // and cleanups safely.
  ASSERT_TRUE(scheduler_thread_->Start());
  SyncPoint sync(thread_system_.get());
  int64 start_us = timer_.NowUs();
  scheduler_.AddAlarm(start_us + 25 * Timer::kMsUs,
                      new NotifyRunFunction(&sync));
  sync.Wait();
  int64 end_us = timer_.NowUs();
  EXPECT_LT(start_us + 24 * Timer::kMsUs, end_us);
  EXPECT_GT(start_us + Timer::kMinuteUs, end_us);
  scheduler_thread_->MakeDeleter()->CallRun();
};

}  // namespace

}  // namespace net_instaweb
