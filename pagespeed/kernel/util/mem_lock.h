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

#ifndef PAGESPEED_KERNEL_BASE_MEM_LOCK_H_
#define PAGESPEED_KERNEL_BASE_MEM_LOCK_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class Function;
class MemLockState;

class MemLock : public NamedLock {
 public:
  static const int64 kDoNotSteal = -1;
  static const int64 kNotHeld = -1;
  static const int64 kNotPending = -1;

  virtual void LockTimedWait(int64 wait_ms, Function* callback);
  virtual void LockTimedWaitStealOld(int64 wait_ms, int64 steal_ms,
                                     Function* callback);
  virtual void Unlock();
  virtual GoogleString name() const;
  virtual bool Held() { return grant_time_ms_ != kNotHeld; }

  // Entry-point for the lock-manager to wake up this lock, stealing it or
  // canceling as needed.
  void Wakeup();

  // Returns the currently scheduled time of the next required wakeup for this
  // lock, based on the earlier cancel timeout and time it gets to steal the
  // lock that is currently held.  Note that this value can be changed by
  // CalculateWakeupTime,  This value is in absolute time since 1970.
  int64 wakeup_time_ms() const { return wakeup_time_ms_; }

  // Computes a stable ordering for multiple locks with the same
  // time-based criteria based primarily on the name, and secondarily on
  // the sequence number.
  //
  // Returns -1 if this is less than that.
  // Returns 1 if this is greater than that.
  // Check-fails if they compare as the same.
  int StableCompare(const MemLock* that) const;

 private:
  friend class MemLockState;

  // Only MemLockState can construct MemLocks.
  MemLock(int64 sequence, MemLockState* lock_state);
  virtual ~MemLock();

  // The time this lock was granted, or kNotHeld if not held.
  int64 grant_time_ms() const { return grant_time_ms_; }

  // Returns the number of maximum number of milliseconds that the current
  // lock can hold prior to this lock stealing it.  This value is relative
  // to the start of the holding lock.
  int64 steal_ms() const { return steal_ms_; }

  // Grants a lock, calling the callback's Run method.  It is invalid
  // to attempt to grant a lock when it is already held, or is not pending.
  void Grant(int64 grant_time_ms);

  // Denies a lock, calling the callback's Cancel methods.  It is
  // invalid to attempt to Deny a lock when it is already held, or is
  // not pending.
  void Deny();

  // Determines what the next wakeup time for this lock is, which is the
  // earlier of the cancel timeout and the steal-time based on the time
  // since the currently holding lock was granted.
  //
  // Note that if a lock is in a time-ordered set this is invalid to call,
  // and will likely cause a crash.
  //
  // If no there is no holding lock, held_lock_grant_time_ms should be specified
  // as kNotHeld.
  void CalculateWakeupTime(int64 held_lock_grant_time_ms);

  // Determines whether this lock is configured to steal from others.
  bool CanSteal() const { return steal_ms_ != kDoNotSteal; }

  // Determines whether this lock has been requested, but not yet granted.
  bool IsPending() const { return wakeup_time_ms_ != kNotPending; }

  // Clears out all lock state.  This is invalid to call when the lock is
  // in any kind of set.
  void Clear();

  // Determines whether this lock should be cancelled if it is woken
  // up before it is granted.
  bool ShouldCancelOnWakeup() const {
    return cancel_time_ms_ == wakeup_time_ms_;
  }

  MemLockState* lock_state_;
  Function* callback_;

  int64 cancel_time_ms_;  // Absolute (MS since 1970)
  int64 grant_time_ms_;   // Absolute time lock was granted, or kNotHeld.
  int64 sequence_;        // Lock index assigned by MemLockManager.

  // Only mutate steal_ms_ and wakeup_time_ms_ when !lock_state_->IsPending()
  // since that would corrupt maps in lock_state_ and lock_state_->manager_.
  int64 steal_ms_;        // Relative.
  int64 wakeup_time_ms_;  // Absolute, and may be canceling or stealing.
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_MEM_LOCK_H_
