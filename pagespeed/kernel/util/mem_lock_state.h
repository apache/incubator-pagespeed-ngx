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

#ifndef PAGESPEED_KERNEL_BASE_MEM_LOCK_STATE_H_
#define PAGESPEED_KERNEL_BASE_MEM_LOCK_STATE_H_

#include <set>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class MemLock;
class MemLockManager;

// Captures and maintains scheduling order for locks with a common name.
class MemLockState {
 public:
  // Note that the MemLockState class is completely inaccessible
  // except to MemLock and MemLockManager; there is no way to get a
  // MemLockState* out of either of those.  Thus these public methods
  // are effectively package-private.

  // Provides a stable comparison for pending Locks held in the
  // MemLockState, and in the MemLockManager, ordered by wakeup time.
  // The MemLockManager tracks all pending locks for any names so they
  // can be denied if the wakeup_time is reached, or stolen if the
  // steal_time is reached.  Note that these times are all absolute,
  // in milliseconds since 1970.
  //
  // Also note once a Lock has been entered into a pending queue,
  // its timestamps must never be changed, or it will corrupt the
  // map.
  struct Comparator {
    bool operator()(const MemLock* a, const MemLock* b) const;
  };

  // Provides a stable comparison for pending locks ordered by
  // their stealing delay.
  struct StealComparator {
    bool operator()(const MemLock* a, const MemLock* b) const;
  };

  typedef std::set<MemLock*, MemLockState::Comparator> WakeupOrderedLockSet;
  typedef std::set<MemLock*, MemLockState::StealComparator> StealOrderedLockSet;

  const GoogleString& name() const { return name_; }

  // Creates a new lock for this name, and track it.  Note that NamedLocks
  // are deleted independently even though they are created from the
  // NamedLockManager.  The MemLockState can outlive the MemLockManager,
  // but will stay alive as long as any of its MemLocks are alive.
  MemLock* CreateLock(int64 sequence);

  // Called by MemLock::~MemLock to let us know it doesn't exist anymore.
  // When the last lock is removed, the MemLockState is deleted.
  void RemoveLock(MemLock* lock);      // Called when lock is deleted

  // Removes a lock from the current schedule.  This is done when
  // lock is granted or denied.
  void UnscheduleLock(MemLock* lock);

  // Adds a lock to the schedule maps.  This happens when a lock is requested.
  // Note that this can directly call lock Run/Cancel if they are due.
  void ScheduleLock(MemLock* lock);

  // Attempts to take a lock immediately, returning false if that failed, in
  // which case the caller is expected to set up its timing constraints and
  // schedule it.  Note that this doesn't call any callbacks; the caller is
  // responsible for doing that.
  bool GrabLock(MemLock* lock);

  // Releases the current lock, wakes up the next pending lock (if any)
  // and calls its Run function.
  void Unlock();

  // Steals the current lock by unlocking it, thus handing it to the next
  // in queue, which must be lock.  lock's Run function is called as well,
  // in response to unlocking the current lock.
  void StealLock(MemLock* lock);

  MemLockManager* manager() { return manager_; }

  // Method to determine whether this lock is in any sets.  This
  // is used for debug-assertions to ensure we don't mutate lock state
  // used for ordering the maps while the lock is in one.
  bool IsHeldInOrderedSet(MemLock* lock) const;

  template<typename T>
  static int Compare(const T& a, const T& b) {
    if (a < b) {
      return -1;
    } else if (a > b) {
      return 1;
    }
    return 0;
  }

 private:
  friend class MemLockManager;  // To allow the manager to construct
                                // MemLockStates.

  // constructor and destructor are private as they should only be called
  // by MemLockManager.
  MemLockState(StringPiece name, MemLockManager* manager);
  ~MemLockState();

  // Denies all pending locks (calling their callbacks' Cancel methods).
  void MemLockManagerDestroyed();

  // Reschedules a lock, which happens when its wakeup time changes.  The
  // wakeup times can change due to the semantics of how steals work, which
  // is based on the grant-time of the currently-held lock.
  void RescheduleLock(int64 held_lock_grant_time_ms, MemLock* lock);

  MemLock* current_owner_;  // Lock that is currently held, or NULL.
  int64 lock_count_;        // Number of locks that were created with this name.
  WakeupOrderedLockSet pending_locks_;
  StealOrderedLockSet pending_steals_;
  GoogleString name_;
  MemLockManager* manager_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_MEM_LOCK_STATE_H_
