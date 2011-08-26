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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MOCK_SCHEDULER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MOCK_SCHEDULER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/scheduler.h"

namespace net_instaweb {

class MockTimer;
class ThreadSystem;

// Implements a Scheduler where time is virtualized, and TimedWait
// blocks the thread until mock-time is advanced.
//
// The TimedWait implemention employs the worker's idle_callback to
// signal the underlying condition variable when the requested time
// has passed.
class MockScheduler : public Scheduler {
 public:
  MockScheduler(ThreadSystem* thread_system, QueuedWorkerPool::Sequence* worker,
                MockTimer* timer);
  virtual ~MockScheduler();

 protected:
  virtual void AwaitWakeup(int64 wakeup_time_us);

 private:
  MockTimer* timer_;
  QueuedWorkerPool::Sequence* worker_;

  DISALLOW_COPY_AND_ASSIGN(MockScheduler);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MOCK_SCHEDULER_H_
