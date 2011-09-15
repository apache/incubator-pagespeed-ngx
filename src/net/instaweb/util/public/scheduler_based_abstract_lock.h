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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SCHEDULER_BASED_ABSTRACT_LOCK_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SCHEDULER_BASED_ABSTRACT_LOCK_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/named_lock_manager.h"

namespace net_instaweb {

class Function;
class Scheduler;

// A SchedulerBasedAbstractLock implements a Lock by blocking using the
// scheduler, using exponential sleep time backoff and polling the lock on
// wakeup.  The total time blocked on a long-held lock will be about 1.5 times
// the time between the initial call to the lock routine attempt and the time
// the lock is unlocked (ie we might wait for an extra amount of time equal to
// half the time we were forced to wait).
class SchedulerBasedAbstractLock : public NamedLock {
 public:
  virtual ~SchedulerBasedAbstractLock();
  virtual bool LockTimedWait(int64 wait_ms);
  virtual void LockTimedWait(int64 wait_ms, Function* callback);

  virtual bool LockTimedWaitStealOld(int64 wait_ms, int64 steal_ms);
  virtual void LockTimedWaitStealOld(
      int64 wait_ms, int64 steal_ms, Function* callback);

 protected:
  virtual Scheduler* scheduler() const = 0;

 private:
  typedef bool (SchedulerBasedAbstractLock::*TryLockMethod)(int64 steal_ms);
  bool TryLockIgnoreSteal(int64 steal_ignored);
  bool BusySpin(TryLockMethod try_lock, int64 steal_ms);
  void PollAndCallback(TryLockMethod try_lock, int64 steal_ms,
                       int64 wait_ms, Function* callback);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SCHEDULER_BASED_ABSTRACT_LOCK_H_
