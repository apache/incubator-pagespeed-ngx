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

// Author: jmarantz@google.com (Joshua Marantz)

#include "pagespeed/kernel/thread/mock_scheduler.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/thread/scheduler.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

MockScheduler::MockScheduler(
    ThreadSystem* thread_system, MockTimer* timer)
    : Scheduler(thread_system, timer),
      timer_(timer) {
}

MockScheduler::~MockScheduler() {
}

void MockScheduler::AwaitWakeupUntilUs(int64 wakeup_time_us) {
  // AwaitWakeupUntilUs is used to effectively move simulated time forward
  // during unit tests.  Various callbacks in the test infrastructure
  // can be called as a result of Alarms firing, enabling the simulation of
  // cache/http fetches with non-zero delay, compute-bound rewrites, or
  // threaded rewrites.
  //
  // To make things simple and deterministic, we simply advance the
  // time when the work threads quiesce.
  if (QueuedWorkerPool::AreBusy(workers_) || running_waiting_alarms()) {
    Scheduler::AwaitWakeupUntilUs(timer_->NowUs() + 10 * Timer::kMsUs);
  } else {
    // Can fire off alarms, so we have to be careful to have the lock
    // relinquished.

    mutex()->Unlock();
    if (wakeup_time_us >= timer_->NowUs()) {
      timer_->SetTimeUs(wakeup_time_us);
    }
    mutex()->Lock();
  }
}

void MockScheduler::AwaitQuiescence() {
  ScopedMutex lock(mutex());
  while (QueuedWorkerPool::AreBusy(workers_) || running_waiting_alarms()) {
    Scheduler::AwaitWakeupUntilUs(timer_->NowUs() + 10 * Timer::kMsUs);
  }
}

void MockScheduler::RegisterWorker(QueuedWorkerPool::Sequence* w) {
  ScopedMutex lock(mutex());
  workers_.insert(w);
}

void MockScheduler::UnregisterWorker(QueuedWorkerPool::Sequence* w) {
  ScopedMutex lock(mutex());
  workers_.erase(w);
}

void MockScheduler::AdvanceTimeUs(int64 timeout_us) {
  ScopedMutex lock(mutex());
  SetTimeUsMutexHeld(timer_->NowUs() + timeout_us);
}

void MockScheduler::SetTimeUsMutexHeld(int64 time_us) {
  // BlockingTimedWaitUs does not guarantee that it will wait all
  // the way until a timeout occurs, so we loop.
  while (timer_->NowUs() < time_us) {
    BlockingTimedWaitUs(time_us - timer_->NowUs());
  }
}

void MockScheduler::SetTimeUs(int64 time_us) {
  CHECK_LE(timer_->NowUs(), time_us)
      << "Time cannot go backward in the scheduler: "
      << "delta=" << timer_->NowUs() - time_us;
  {
    ScopedMutex lock(mutex());
    SetTimeUsMutexHeld(time_us);
  }
}

}  // namespace net_instaweb
