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

#include "net/instaweb/util/public/mock_scheduler.h"

#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

MockScheduler::MockScheduler(ThreadSystem* thread_system,
                             QueuedWorkerPool::Sequence* worker,
                             MockTimer* timer)
    : Scheduler(thread_system, timer),
      timer_(timer),
      worker_(worker) {
}

MockScheduler::~MockScheduler() {
}

void MockScheduler::AwaitWakeup(int64 wakeup_time_us) {
  // AwaitWakeup is used to effectively move simulated time forward
  // during unit tests.  Various callbacks in the test infrastructure
  // can be called as a result of Alarms firing, enabling the simulation of
  // cache/http fetches with non-zero delay, compute-bound rewrites, or
  // threaded rewrites.
  //
  // We want the timer advancement to be triggered from the mainline of the
  // test, but to be executed in the rewrite_thread, removing a variety of
  // confusing race-conditions.
  int64 now_us = timer_->NowUs();
  worker_->Add(new MemberFunction1<MockTimer, int64>(
      &MockTimer::AdvanceUs, timer_, wakeup_time_us - now_us));

  // It's possible that the worker thread may complete the timer callback and
  // all subsequent work before the Wait() call, so check whether it is busy
  // first.  We must release the scheduler lock as we do so, since the check
  // takes the worker lock.
  mutex()->Unlock();

  // TODO(jmarantz): this code is very squirrely: we are spinning on
  // worker-IsBusy, rather than spinning on whether we've woken up.
  // After the revolution, we'll use callbacks rather than TimedWait
  // and avoid this.  But it works for now.
  while (worker_->IsBusy()) {
    // Do the condition-variable-wait for only 100 ms, to avoid deadlocking if
    // Wakeup() gets called after the IsBusy() call, but before the TimedWait()
    // call.  The 100 ms sleep means we just loop back to "IsBusy()" and fall
    // through.  We seem to hit this race condition a few times each time we run
    // the tests; crank up the delay to 10s or 30s and it'll be obvious.
    mutex()->Lock();
    Scheduler::AwaitWakeup(Timer::kSecondUs / 10);
    mutex()->Unlock();
  }
  mutex()->Lock();
}

}  // namespace net_instaweb
