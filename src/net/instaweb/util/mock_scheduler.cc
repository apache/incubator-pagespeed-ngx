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
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/queued_worker.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

MockScheduler::MockScheduler(ThreadSystem* thread_system, QueuedWorker* worker,
                             MockTimer* timer)
    : Scheduler(thread_system),
      timer_(timer),
      worker_(worker) {
  // TODO(jmarantz): we currently only support one Scheduler per Worker, which
  // means that when testing we must have a distinct ResourceManager for every
  // RewriteDriver.  This does not reflect the behavior of servers so we should
  // change the idle_callback_ member variable in Worker to be a set.
  worker_->set_idle_callback(new MemberFunction0<ThreadSystem::Condvar>(
      &ThreadSystem::Condvar::Signal, condvar()));
}

MockScheduler::~MockScheduler() {
}

void MockScheduler::TimedWait(int64 timeout_ms) {
  // This TimedWait is used to effectively move simulated time forward
  // during unit tests.  Various callbacks in the test infrastructure
  // can be called as a result of Alarms firing, enabling the simulation of
  // cache/http fetches with non-zero delay, compute-bound rewrites, or
  // threaded rewrites.
  //
  // We want the timer advancement to be triggered from the mainline of the
  // test, but to be executed in the rewrite_thread, removing a variety of
  // confusing race-conditions.
  int64 timeout_us = 1000 * timeout_ms;
  worker_->RunInWorkThread(new MemberFunction1<MockTimer, int64>(
      &MockTimer::AdvanceUs, timer_, timeout_us));

  // It's possible that the worker thread may complete the timer
  // callback and all subsequent work before the Wait() call, so check
  // whether it is busy first.
  while (worker_->IsBusy()) {
    // Do the condition-variable-wait for only one second, to avoid
    // deadlocking if idle_callback gets called after the IsBusy() call,
    // but before the TimedWait() call.  The one-second sleep means we just
    // loop back to "IsBusy()" and fall through.  Most of the time we won't
    // hit this race-condition and the 1-second slowdown for tests; it's
    // just to escape from a race.
    condvar()->TimedWait(Timer::kSecondMs);
  }
}

}  // namespace net_instaweb
