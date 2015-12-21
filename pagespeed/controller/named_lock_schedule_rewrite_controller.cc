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

namespace {

const char kStatisticsGroup[] = "Statistics";

}  // namespace

NamedLockScheduleRewriteController::NamedLockScheduleRewriteController(
    NamedLockManager* lock_manager, ThreadSystem* thread_system,
    Statistics* stats)
    : mutex_(thread_system->NewMutex()),
      lock_manager_(lock_manager),
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
  statistics->AddTimedVariable(kLocksGranted, kStatisticsGroup);
  statistics->AddTimedVariable(kLocksDenied, kStatisticsGroup);
  statistics->AddTimedVariable(kLocksStolen, kStatisticsGroup);
  statistics->AddTimedVariable(kLocksReleasedWhenNotHeld, kStatisticsGroup);
  statistics->AddUpDownCounter(kLocksCurrentlyHeld);
}

NamedLockScheduleRewriteController::LockInfo*
NamedLockScheduleRewriteController::GetLockInfo(const GoogleString& key) {
  LockInfo*& info = locks_[key];
  if (info == NULL) {
    info = new LockInfo();
  }
  DCHECK_GE(info->num_pending_callbacks, 0);
  return info;
}

void NamedLockScheduleRewriteController::DeleteInfoIfUnused(
    LockInfo* info, const GoogleString& key) {
  DCHECK_GE(info->num_pending_callbacks, 0);
  if (info->lock.get() == NULL && info->num_pending_callbacks <= 0) {
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
  {
    ScopedMutex mutex_lock(mutex_.get());
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
    --info->num_pending_callbacks;
    // No point calling DeleteInfoIfUnused since we know the lock is held.
  }
  callback->CallRun();
}

void NamedLockScheduleRewriteController::LockFailed(Function* callback,
                                                    const GoogleString key,
                                                    NamedLock* named_lock) {
  // named_lock is not actually held, so just delete it.
  delete named_lock;
  locks_denied_->IncBy(1);
  {
    ScopedMutex mutex_lock(mutex_.get());
    LockInfo* info = GetLockInfo(key);
    --info->num_pending_callbacks;
    DeleteInfoIfUnused(info, key);
  }
  callback->CallCancel();
}

void NamedLockScheduleRewriteController::ScheduleRewrite(
    const GoogleString& key, Function* callback) {
  {
    ScopedMutex mutex_lock(mutex_.get());
    LockInfo* info = GetLockInfo(key);
    ++info->num_pending_callbacks;
    // No point calling DeleteInfoIfUnused since num_pending_callbacks is >0.
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
    ++info->num_pending_callbacks;
    // Steal the pointer out of the info, which marks the info as not locked.
    named_lock.reset(info->lock.release());
  }
  // Unlock could theoretically call back synchronously into one of our other
  // routines, so we must not hold mutex_ when unlocking. Holding "info" outside
  // the mutex is perfectly safe because we prevented it from being deleted by
  // incrementing num_pending_callbacks above.
  locks_currently_held_->Add(-1);
  named_lock->Unlock();
  {
    ScopedMutex mutex_lock(mutex_.get());
    --info->num_pending_callbacks;
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

}  // namespace net_instaweb
