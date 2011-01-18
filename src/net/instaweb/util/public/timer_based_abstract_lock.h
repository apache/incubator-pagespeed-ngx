/*
 * Copyright 2010 Google Inc.
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_TIMER_BASED_ABSTRACT_LOCK_H_
#define NET_INSTAWEB_UTIL_PUBLIC_TIMER_BASED_ABSTRACT_LOCK_H_

#include "base/basictypes.h"
#include "net/instaweb/util/public/named_lock_manager.h"

namespace net_instaweb {

class Timer;

// A TimerBasedAbstractLock implements a Lock by spinning on a TryLock, using a
// Timer to perform exponential sleep backoff.  Note that this means that it may
// not obtain a long-held lock in a timely fashion after it has been unlocked.
class TimerBasedAbstractLock : public AbstractLock {
 public:
  virtual ~TimerBasedAbstractLock();
  virtual void Lock();
  virtual bool LockTimedWait(int64 wait_ms);

  virtual void LockStealOld(int64 timeout_ms);
  virtual bool LockTimedWaitStealOld(int64 wait_ms, int64 timeout_ms);

 protected:
  virtual Timer* timer() const = 0;

 private:
  typedef bool (TimerBasedAbstractLock::*TryLockMethod)(int64 timeout_ms);
  bool TryLockIgnoreTimeout(int64 timeout_ignored);
  bool BusySpin(TryLockMethod try_lock, int64 timeout_ms);
  void Spin(TryLockMethod try_lock, int64 timeout_ms);
  bool SpinFor(TryLockMethod try_lock, int64 timeout_ms, int64 wait_ms);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_TIMER_BASED_ABSTRACT_LOCK_H_
