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

#include "pagespeed/kernel/util/mem_lock_state.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/util/mem_lock.h"
#include "pagespeed/kernel/util/mem_lock_manager.h"

namespace net_instaweb {

MemLockState::MemLockState(StringPiece name, MemLockManager* manager)
    : current_owner_(NULL),
      lock_count_(0),
      name_(name.data(), name.size()),
      manager_(manager) {
}

MemLockState::~MemLockState() {
}

MemLock* MemLockState::CreateLock(int64 sequence) {
  MemLock* lock = new MemLock(sequence, this);
  ++lock_count_;
  return lock;
}

// Called from MemLock's destructor.
void MemLockState::RemoveLock(MemLock* lock) {
  if (--lock_count_ == 0) {
    CHECK(pending_locks_.empty());
    CHECK(pending_steals_.empty());
    if (manager_ != NULL) {
      manager_->RemoveLockState(this);
    }
    delete this;
  }
}

void MemLockState::Unlock() {
  CHECK(current_owner_ != NULL);
  current_owner_->Clear();
  if (!pending_locks_.empty()) {
    WakeupOrderedLockSet::iterator p = pending_locks_.begin();
    current_owner_ = *p;
    bool new_owner_can_steal = current_owner_->CanSteal();
    manager_->RemovePendingLock(current_owner_);
    pending_locks_.erase(p);
    int64 grant_time_ms = manager()->timer()->NowMs();
    if (new_owner_can_steal) {
      pending_steals_.erase(current_owner_);
      if (!pending_steals_.empty()) {
        // Establish a new potential stealer.
        MemLock* stealer = *pending_steals_.begin();
        RescheduleLock(grant_time_ms, stealer);
      }
    }
    current_owner_->Grant(grant_time_ms);
  } else {
    current_owner_ = NULL;
  }
}

// This new lock wants to steal more aggressively than
// pending_lock.  We must remove it from all maps before
// adjusting its timing to keep the map comparators sane.
void MemLockState::RescheduleLock(
    int64 held_lock_grant_time_ms, MemLock* lock) {
  UnscheduleLock(lock);
  lock->CalculateWakeupTime(held_lock_grant_time_ms);
  pending_locks_.insert(lock);
  if (lock->CanSteal()) {
    pending_steals_.insert(lock);
  }
  manager_->AddPendingLock(lock);
}

void MemLockState::MemLockManagerDestroyed() {
  CHECK(manager_ != NULL);
  manager_ = NULL;
  while (!pending_locks_.empty()) {
    MemLock* lock = *pending_locks_.begin();
    lock->Deny();
  }
}

void MemLockState::StealLock(MemLock* lock) {
  CHECK(current_owner_ != NULL);
  current_owner_->Unlock();  // We expect lock should be the first stealer.
  CHECK_EQ(current_owner_, lock);
}

bool MemLockState::GrabLock(MemLock* lock) {
  if (current_owner_ != NULL) {
    return false;
  }
  current_owner_ = lock;
  return true;
}

void MemLockState::ScheduleLock(MemLock* lock) {
  CHECK(current_owner_ != NULL);
  // Assume optimistically that this lock will displace any current
  // pending steal.  If that turns out to be false we will need to
  // recalculate its steal_time.
  CHECK(!lock->IsPending());
  lock->CalculateWakeupTime(current_owner_->grant_time_ms());

  if (lock->CanSteal()) {
    if (!pending_steals_.empty()) {
      MemLock* pending_steal = *pending_steals_.begin();
      MemLockState::StealComparator comparator;
      if (comparator(lock, pending_steal)) {
        // The new lock has a lower steal_time than the
        // lock that was previously the one with the lowest
        // steal time.  So we'll need to reschedule the pending_steal
        // as it's modeled wakeup time must ignore its steal_ms.
        RescheduleLock(MemLock::kNotHeld, pending_steal);
      } else {
        lock->CalculateWakeupTime(MemLock::kNotHeld);
      }
    }
    pending_steals_.insert(lock);
  }

  pending_locks_.insert(lock);
  manager_->AddPendingLock(lock);
  manager_->Wakeup();
}

void MemLockState::UnscheduleLock(MemLock* lock) {
  pending_locks_.erase(lock);
  pending_steals_.erase(lock);
  if (manager_ != NULL) {
    manager_->RemovePendingLock(lock);
  }
}

bool MemLockState::Comparator::operator()(
    const MemLock* a, const MemLock* b) const {
  if (a == b) {
    return false;
  }

  int cmp = Compare(a->wakeup_time_ms(), b->wakeup_time_ms());
  if (cmp == 0) {
    cmp = a->StableCompare(b);
  }
  return cmp < 0;
}

bool MemLockState::StealComparator::operator()(
    const MemLock* a, const MemLock* b) const {
  if (a == b) {
    return false;
  }

  int cmp = Compare(a->steal_ms(), b->steal_ms());
  if (cmp == 0) {
    cmp = a->StableCompare(b);
  }
  return cmp < 0;
}

bool MemLockState::IsHeldInOrderedSet(MemLock* lock) const {
  return (((manager_ != NULL) && manager_->IsHeldInOrderedSet(lock)) ||
          (pending_locks_.find(lock) != pending_locks_.end()) ||
          (pending_steals_.find(lock) != pending_steals_.end()));
}

}  // namespace net_instaweb
