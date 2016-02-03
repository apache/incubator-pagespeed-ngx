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

#ifndef PAGESPEED_KERNEL_BASE_MEM_LOCK_MANAGER_H_
#define PAGESPEED_KERNEL_BASE_MEM_LOCK_MANAGER_H_

#include <map>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/util/mem_lock.h"
#include "pagespeed/kernel/util/mem_lock_state.h"

namespace net_instaweb {

// Implements NamedLockManager using in-memory data structures.  This core
// structure has two anticipated uses:
//
//  1. A threadsafe version integrated with the Scheduler, which will
//     establish new alarms, potentially cancelling old ones, when new locks
//     are requested that cannot be immediately granted.
//  2. An RPC-server based version where these timeouts will be fed to epoll or
//     similar.
class MemLockManager : public NamedLockManager {
 public:
  static const int64 kNoWakeupsPending = -1;

  explicit MemLockManager(Timer* timer);
  virtual ~MemLockManager();
  virtual NamedLock* CreateNamedLock(const StringPiece& name);

  // Returns the absolute time (ms since 1970) of the next interesting
  // event (cancel, steal) for any lock created with this manager.
  // Returns kNoWakeupsPending if no wakeups are needed.
  //
  // Note that the wakeup time may change as a result of new locks
  // being requested or released.  Depending on the scheduling technology
  // used (e.g. epoll, Scheduler, etc) explicit support might be needed
  // to ensure a timely wakeup.
  int64 NextWakeupTimeMs() const;

  // Runs any pending events (cancels, steals) for any pending locks.
  void Wakeup();

  Timer* timer() const { return timer_; }

  // Method to determine whether this lock is in pending_locks_.  This
  // is used for debug-assertions to ensure we don't mutate ordering fields
  // in locks that are held in sets.
  bool IsHeldInOrderedSet(MemLock* lock) const;

 private:
  typedef std::map<StringPiece, MemLockState*> MemLockStateMap;

  // Helper methods accessed by MemLockState
  void RemoveLockState(MemLockState* lock_state);
  void AddPendingLock(MemLock* lock);
  void RemovePendingLock(MemLock* lock);

  friend class MemLockState;

  // All locks taken for a particular name are kept in a MemLockState
  // object, and this map manages those.
  MemLockStateMap lock_state_map_;

  // Orders all pending locks of all names across the manager.  This
  // differs from MemLockState::pending_locks_, which only contains
  // the locks for a particular name.
  MemLockState::WakeupOrderedLockSet pending_locks_;

  Timer* timer_;
  int64 sequence_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_MEM_LOCK_MANAGER_H_
