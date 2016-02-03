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

#include "pagespeed/kernel/util/mem_lock_manager.h"

#include <utility>

#include "base/logging.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/util/mem_lock.h"
#include "pagespeed/kernel/util/mem_lock_state.h"

namespace net_instaweb {

MemLockManager::MemLockManager(Timer* timer)
    : timer_(timer),
      sequence_(0) {
}

MemLockManager::~MemLockManager() {
  // Note that we don't delete the locks here.  We just detach the
  // MemLockState objects.  The MemLockState objects need to outlive
  // all the MemLocks allocated against them, as they are dependent
  // on the MemLockState to know their own name.
  for (MemLockStateMap::iterator p = lock_state_map_.begin(),
           e = lock_state_map_.end(); p != e; ++p) {
    MemLockState* name_state = p->second;
    name_state->MemLockManagerDestroyed();
  }
}

void MemLockManager::RemoveLockState(MemLockState* name_state) {
  int erased = lock_state_map_.erase(name_state->name());
  CHECK_EQ(1, erased);
}

NamedLock* MemLockManager::CreateNamedLock(const StringPiece& name) {
  MemLockStateMap::iterator p = lock_state_map_.find(name);
  MemLockState* mem_lock_state;
  if (p != lock_state_map_.end()) {
    mem_lock_state = p->second;
  } else {
    // Make sure that the StringPiece held as the map key points to
    // the MemLockState, and not the possibly temporary char* passed
    // in as name.data().
    mem_lock_state = new MemLockState(name, this);
    lock_state_map_[mem_lock_state->name()] = mem_lock_state;
  }
  return mem_lock_state->CreateLock(++sequence_);
}

void MemLockManager::AddPendingLock(MemLock* lock) {
  pending_locks_.insert(lock);
}

void MemLockManager::RemovePendingLock(MemLock* lock) {
  pending_locks_.erase(lock);
}

int64 MemLockManager::NextWakeupTimeMs() const {
  if (pending_locks_.empty()) {
    return kNoWakeupsPending;
  }
  MemLock* next_lock = *pending_locks_.begin();
  return next_lock->wakeup_time_ms();
}

void MemLockManager::Wakeup() {
  int64 now_ms = timer_->NowMs();
  while (!pending_locks_.empty()) {
    MemLockState::WakeupOrderedLockSet::iterator p = pending_locks_.begin();
    MemLock* lock = *p;
    if (lock->wakeup_time_ms() <= now_ms) {
      lock->Wakeup();
    } else {
      return;
    }
  }
}

bool MemLockManager::IsHeldInOrderedSet(MemLock* lock) const {
  return pending_locks_.find(lock) != pending_locks_.end();
}

}  // namespace net_instaweb
