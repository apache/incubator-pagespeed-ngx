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
 */
// Author: jmarantz@google.com (Joshua Marantz)

// Helper class for sending concurrent traffic to a cache during unit tests.

#ifndef PAGESPEED_KERNEL_UTIL_LOCK_MANAGER_SPAMMER_H_
#define PAGESPEED_KERNEL_UTIL_LOCK_MANAGER_SPAMMER_H_

#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/thread/scheduler.h"

namespace net_instaweb {

class ThreadSafeLockManager;

// Test helper class for blasting a lock-manager with concurrent lock/unlock
// requests.
class LockManagerSpammer : public ThreadSystem::Thread {
 public:
  virtual ~LockManagerSpammer();

  // num_threads indicates how many threads will run in parallel.
  // num_iters indicates how many times each thread will run a big loop.
  // num_names sets the number of different lock-names are locked and
  // unlocked in the loop.
  static void RunTests(int num_threads, int num_iters, int num_names,
                       bool expecting_denials, bool delay_unlocks,
                       ThreadSafeLockManager* lock_manager,
                       Scheduler* scheduler);

  // Called when a lock is granted/denied.
  void Granted(NamedLock* lock);
  void UnlockAfterGrant(NamedLock* lock);
  void Denied(NamedLock* lock);

 protected:
  virtual void Run();

 private:
  class CountDown {
   public:
    CountDown(Scheduler* scheduler, int initial_value);
    ~CountDown();

    void Decrement();
    void RunAlarmsTillThreadsComplete();

   private:
    Scheduler* scheduler_;
    int value_ GUARDED_BY(scheduler_->mutex());
  };

  typedef std::vector<NamedLock*> LockVector;

  LockManagerSpammer(Scheduler* scheduler,
                     ThreadSystem::ThreadFlags flags,
                     const StringVector& lock_names,
                     ThreadSafeLockManager* lock_manager,
                     bool expecting_denials,
                     bool delay_unlocks,
                     int index,
                     int num_iters,
                     int num_names,
                     CountDown* pending_threads);

  Scheduler* scheduler_;
  const StringVector& lock_names_;
  ThreadSafeLockManager* lock_manager_;
  bool expecting_denials_;
  bool delay_unlocks_;
  int index_;
  int num_iters_;
  int num_names_;
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> condvar_;
  int grants_ GUARDED_BY(mutex_);
  int denials_ GUARDED_BY(mutex_);
  LockVector queued_unlocks_ GUARDED_BY(mutex_);
  CountDown* pending_threads_;

  DISALLOW_COPY_AND_ASSIGN(LockManagerSpammer);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_LOCK_MANAGER_SPAMMER_H_
