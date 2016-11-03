// Copyright 2015 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: cheesy@google.com (Steve Hill)

#include "pagespeed/controller/named_lock_schedule_rewrite_controller.h"

#include <cstddef>

#include "base/logging.h"
#include "pagespeed/kernel/base/stl_util.h"

namespace net_instaweb {

const char NamedLockScheduleRewriteController::kLocksGranted[] =
    "named-lock-rewrite-scheduler-granted";
const char NamedLockScheduleRewriteController::kLocksDenied[] =
    "named-lock-rewrite-scheduler-denied";
const char NamedLockScheduleRewriteController::kLocksStolen[] =
    "named-lock-rewrite-scheduler-stolen";
const char NamedLockScheduleRewriteController::kLocksReleasedWhenNotHeld[] =
    "named-lock-rewrite-scheduler-released-not-held";
const char NamedLockScheduleRewriteController::kLocksCurrentlyHeld[] =
    "named-lock-rewrite-scheduler-locks-held";

const int NamedLockScheduleRewriteController::kStealMs = 30000;

NamedLockScheduleRewriteController::NamedLockScheduleRewriteController(
    NamedLockManager* lock_manager, ThreadSystem* thread_system,
    Statistics* stats)
    : mutex_(thread_system->NewMutex()),
      lock_manager_(lock_manager),
      shut_down_(false),
      locks_granted_(stats->GetTimedVariable(kLocksGranted)),
      locks_denied_(stats->GetTimedVariable(kLocksDenied)),
      locks_stolen_(stats->GetTimedVariable(kLocksStolen)),
      locks_released_when_not_held_(
          stats->GetTimedVariable(kLocksReleasedWhenNotHeld)),
      locks_currently_held_(stats->GetUpDownCounter(kLocksCurrentlyHeld)) {
}

NamedLockScheduleRewriteController::~NamedLockScheduleRewriteController() {
  // We shouldn't actually have any locks held, but free any that are.
  DCHECK(locks_.empty());
  STLDeleteValues(&locks_);
}

void NamedLockScheduleRewriteController::InitStats(Statistics* statistics) {
  statistics->AddTimedVariable(kLocksGranted, Statistics::kDefaultGroup);
  statistics->AddTimedVariable(kLocksDenied, Statistics::kDefaultGroup);
  statistics->AddTimedVariable(kLocksStolen, Statistics::kDefaultGroup);
  statistics->AddTimedVariable(kLocksReleasedWhenNotHeld,
                               Statistics::kDefaultGroup);
  statistics->AddUpDownCounter(kLocksCurrentlyHeld);
}

NamedLockScheduleRewriteController::LockInfo*
NamedLockScheduleRewriteController::GetLockInfo(const GoogleString& key) {
  LockInfo*& info = locks_[key];
  if (info == NULL) {
    info = new LockInfo();
  }
  DCHECK_GE(info->pin_count, 0);
  return info;
}

void NamedLockScheduleRewriteController::DeleteInfoIfUnused(
    LockInfo* info, const GoogleString& key) {
  DCHECK_GE(info->pin_count, 0);
  if (info->lock.get() == nullptr && info->pin_count <= 0
      && info->pending_callbacks.empty()) {
    size_t num_erased = locks_.erase(key);
    CHECK_EQ(1, num_erased);
    delete info;
  }
}

void NamedLockScheduleRewriteController::LockObtained(Function* callback,
                                                      const GoogleString key,
                                                      NamedLock* named_lock) {
  locks_granted_->IncBy(1);
  locks_currently_held_->Add(1);
  bool shut_down;
  {
    ScopedMutex mutex_lock(mutex_.get());
    shut_down = shut_down_;

    LockInfo* info = GetLockInfo(key);
    // This lock may have been held by someone else, but it isn't any more!
    if (info->lock.get() != NULL) {
      locks_stolen_->IncBy(1);
      locks_currently_held_->Add(-1);
    }
    // This function may delete a lock thats in the middle of being stolen.
    // Your NamedLock implementation must support that, see
    // NamedLockTester::StealWithDelete (and UnlockWithDelete).
    info->lock.reset(named_lock);
    info->pending_callbacks.erase(callback);
    // No point calling DeleteInfoIfUnused since we know the lock is held.
  }

  // After ::ShutDown, we are no longer responsible for invoking callbacks.
  if (!shut_down) {
    callback->CallRun();
  }
}

void NamedLockScheduleRewriteController::LockFailed(Function* callback,
                                                    const GoogleString key,
                                                    NamedLock* named_lock) {
  locks_denied_->IncBy(1);
  bool shut_down;
  {
    ScopedMutex mutex_lock(mutex_.get());
    shut_down = shut_down_;
    // named_lock is not actually held, so just delete it.
    delete named_lock;

    LockInfo* info = GetLockInfo(key);
    info->pending_callbacks.erase(callback);
    DeleteInfoIfUnused(info, key);
  }

  // After ::ShutDown, we are no longer responsible for invoking callbacks.
  if (!shut_down) {
    callback->CallCancel();
  }
}

void NamedLockScheduleRewriteController::ScheduleRewrite(
    const GoogleString& key, Function* callback) {
  bool shut_down = false;
  {
    ScopedMutex mutex_lock(mutex_.get());
    shut_down = shut_down_;
    if (!shut_down) {
      LockInfo* info = GetLockInfo(key);
      info->pending_callbacks.insert(callback);
      // Don't need DeleteInfoIfUnused() since pending_callbacks is non-empty.
    }
  }
  if (shut_down) {
    callback->CallCancel();
    return;
  }
  NamedLock* named_lock = lock_manager_->CreateNamedLock(key);
  // We can't call this with a lock on mutex_ since it may call back
  // synchronously, which would deadlock.
  named_lock->LockTimedWaitStealOld(
      0 /* wait_ms */, kStealMs,
      MakeFunction<NamedLockScheduleRewriteController, Function*,
                   const GoogleString, NamedLock*>(
          this,
          &NamedLockScheduleRewriteController::LockObtained,
          &NamedLockScheduleRewriteController::LockFailed,
          callback, key, named_lock));
}

void NamedLockScheduleRewriteController::NotifyRewriteComplete(
    const GoogleString& key) {
  // Because of lock stealing, this has the unfortunate property that if an
  // operation completes after the steal deadline, it will release someone
  // else's lock. Given that this is expected to be unlikely and the worst case
  // is redundant work, it shouldn't matter too much.
  LockInfo* info;
  scoped_ptr<NamedLock> named_lock;
  {
    ScopedMutex mutex_lock(mutex_.get());
    info = GetLockInfo(key);
    // The lock might not actually be held if it was stolen and then released.
    if (info->lock.get() == NULL) {
      locks_released_when_not_held_->IncBy(1);
      DeleteInfoIfUnused(info, key);
      return;
    }
    ++info->pin_count;
    // Steal the pointer out of the info, which marks the info as not locked.
    named_lock.reset(info->lock.release());
  }
  // Unlock could theoretically call back synchronously into one of our other
  // routines, so we must not hold mutex_ when unlocking. Holding "info" outside
  // the mutex is perfectly safe because we prevented it from being deleted by
  // incrementing pin_count above.
  locks_currently_held_->Add(-1);
  named_lock->Unlock();
  {
    ScopedMutex mutex_lock(mutex_.get());
    --info->pin_count;
    // Note that a callback from Unlock() may have re-acquired the lock.
    DeleteInfoIfUnused(info, key);
  }
}

void NamedLockScheduleRewriteController::NotifyRewriteFailed(
    const GoogleString& key) {
  // This implemenation doesn't have special failure handling, so just call
  // Complete.
  this->NotifyRewriteComplete(key);
}


void NamedLockScheduleRewriteController::ShutDown() {
  // After ShutDown, all existing callbacks will be cancelled, requests for
  // scheduling will immediately be responded to with Cancel(), and the usual
  // codepath will no longer involve callbacks.
  std::vector<Function*> callbacks;
  {
    ScopedMutex mutex_lock(mutex_.get());
    shut_down_ = true;
    for (const auto& p : locks_) {
      for (Function* f : p.second->pending_callbacks) {
        callbacks.push_back(f);
      }
    }
  }

  for (Function* f : callbacks) {
    f->CallCancel();
  }
}

}  // namespace net_instaweb
