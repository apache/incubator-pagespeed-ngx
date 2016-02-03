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

#include "pagespeed/kernel/util/mem_lock.h"

#include <algorithm>

#include "base/logging.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/util/mem_lock_manager.h"
#include "pagespeed/kernel/util/mem_lock_state.h"

namespace net_instaweb {

MemLock::MemLock(int64 sequence, MemLockState* lock_state)
    : lock_state_(lock_state),
      sequence_(sequence) {
  Clear();
}

MemLock::~MemLock() {
  if (IsPending()) {
    Deny();
  } else if (Held()) {
    lock_state_->Unlock();
  }
  lock_state_->RemoveLock(this);
}

void MemLock::Clear() {
  DCHECK(!lock_state_->IsHeldInOrderedSet(this));
  callback_ = NULL;
  cancel_time_ms_ = 0;
  steal_ms_ = 0;
  wakeup_time_ms_ = kNotPending;
  grant_time_ms_ = kNotHeld;
}

// Return immediately.  Wait wait_ms to take lock, invoke callback with lock
// held.  On timeout, cancel callback.
void MemLock::LockTimedWait(int64 wait_ms, Function* callback) {
  LockTimedWaitStealOld(wait_ms, kDoNotSteal, callback);
}

void MemLock::LockTimedWaitStealOld(int64 wait_ms, int64 steal_ms,
                                    Function* callback) {
  if (Held() || IsPending()) {
    LOG(DFATAL) << "Requesting lock " << name() << " when it's already "
                << (IsPending() ? "pending" : "held");
    callback->CallCancel();
  } else if (lock_state_->GrabLock(this)) {
    grant_time_ms_ = lock_state_->manager()->timer()->NowMs();
    callback->CallRun();
  } else {
    DCHECK(!lock_state_->IsHeldInOrderedSet(this));
    CHECK(callback_ == NULL);
    cancel_time_ms_ = lock_state_->manager()->timer()->NowMs() + wait_ms;
    steal_ms_ = steal_ms;
    callback_ = callback;
    lock_state_->ScheduleLock(this);
  }
}

void MemLock::Wakeup() {
  if (ShouldCancelOnWakeup()) {
    Deny();
  } else {
    CHECK(CanSteal());
    lock_state_->StealLock(this);
  }
}

GoogleString MemLock::name() const {
  return lock_state_->name();
}

void MemLock::CalculateWakeupTime(int64 held_lock_grant_time_ms) {
  DCHECK(!lock_state_->IsHeldInOrderedSet(this));
  if (!CanSteal() || (held_lock_grant_time_ms == kNotHeld)) {
    wakeup_time_ms_ = cancel_time_ms_;
  } else {
    int64 steal_time_ms = steal_ms_ + held_lock_grant_time_ms;
    wakeup_time_ms_ = std::min(cancel_time_ms_, steal_time_ms);
  }
}

void MemLock::Unlock() {
  // Locks can be stolen from the holder without notifying the owner,
  // so it is not considered an error to try to unlock a NamedLock that
  // is not held.
  if (Held()) {
    lock_state_->Unlock();
    // "this" may have been freed by this point.
  }
}

void MemLock::Grant(int64 grant_time_ms) {
  lock_state_->UnscheduleLock(this);
  Function* callback = callback_;
  CHECK(!Held());
  CHECK(IsPending());
  Clear();
  grant_time_ms_ = grant_time_ms;
  CHECK(callback);
  callback->CallRun();
}

void MemLock::Deny() {
  lock_state_->UnscheduleLock(this);
  Function* callback = callback_;
  CHECK(IsPending());
  CHECK(!Held());
  Clear();
  CHECK(callback);
  callback->CallCancel();
}

// Computes a stable ordering for multiple locks with the same
// time-based criteria based primarily on the name, and secondarily on
// the sequence number.
int MemLock::StableCompare(const MemLock* that) const {
  int cmp = 0;
  if (lock_state_ == that->lock_state_) {
    cmp = MemLockState::Compare(sequence_, that->sequence_);
  } else {
    cmp = MemLockState::Compare(lock_state_->name(), that->lock_state_->name());
  }

  // Note that if we don't get a strict ordering here between two
  // different locks we will lose one of them in the map.
  DCHECK_NE(0, cmp);
  return cmp;
}

}  // namespace net_instaweb
