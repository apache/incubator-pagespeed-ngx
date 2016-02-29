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

#ifndef PAGESPEED_KERNEL_THREAD_MOCK_SCHEDULER_H_
#define PAGESPEED_KERNEL_THREAD_MOCK_SCHEDULER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"
#include "pagespeed/kernel/thread/scheduler.h"

namespace net_instaweb {

class MockTimer;

// Implements a Scheduler where time is virtualized, and TimedWait
// blocks the thread until mock-time is advanced.
//
// The TimedWait implemention employs the worker's idle_callback to
// signal the underlying condition variable when the requested time
// has passed.
class MockScheduler : public Scheduler {
 public:
  MockScheduler(ThreadSystem* thread_system,
                MockTimer* timer);
  virtual ~MockScheduler();

  virtual void RegisterWorker(QueuedWorkerPool::Sequence* w)
      LOCKS_EXCLUDED(mutex());
  virtual void UnregisterWorker(QueuedWorkerPool::Sequence* w)
      LOCKS_EXCLUDED(mutex());

  // Blocks until all work in registered workers is done.
  void AwaitQuiescence() LOCKS_EXCLUDED(mutex());

  // Similar to BlockingTimedWaitUs but takes the lock for convenience.
  void AdvanceTimeMs(int64 timeout_ms) LOCKS_EXCLUDED(mutex()) {
    AdvanceTimeUs(timeout_ms * Timer::kMsUs);
  }
  void AdvanceTimeUs(int64 timeout_us) LOCKS_EXCLUDED(mutex());

  // Sets the current absolute time using absolute numbers.
  void SetTimeUs(int64 time_us) LOCKS_EXCLUDED(mutex());

 protected:
  virtual void AwaitWakeupUntilUs(int64 wakeup_time_us)
      EXCLUSIVE_LOCKS_REQUIRED(mutex());

 private:
  inline void SetTimeUsMutexHeld(int64 time_us)
      EXCLUSIVE_LOCKS_REQUIRED(mutex());

  // TODO(jud): Verify if this variable should be GUARDED_BY(mutex()). The
  // CHECK_LE in SetTimeUs currently has an unlocked access to it.
  MockTimer* timer_;
  QueuedWorkerPool::SequenceSet workers_ GUARDED_BY(mutex());

  DISALLOW_COPY_AND_ASSIGN(MockScheduler);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_THREAD_MOCK_SCHEDULER_H_
