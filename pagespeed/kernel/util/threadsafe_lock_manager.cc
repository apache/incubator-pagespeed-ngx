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

#include "pagespeed/kernel/util/threadsafe_lock_manager.h"

#include <cstddef>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/thread/scheduler.h"
#include "pagespeed/kernel/util/mem_lock_manager.h"

namespace net_instaweb {

// The NamedLockManager API allows locks to outlive the
// NamedLockManager.  To allow this to happen sanely, we need to
// ref-count the guts of the ThreadSafeLockManager's state, in
// particular access to the scheduler mutex, so that we can safely
// serialize the shutdown flow.
class ThreadSafeLockManager::LockHolder : public RefCounted<LockHolder> {
  class ScopedLockRunningDelayedCallbacks;
  friend class ScopedLockRunningDelayedCallbacks;
  friend class Lock;  // Needed by thread annotation for mutex_.
  typedef std::set<Lock*> LockSet;
  typedef std::pair<Function*, bool> DelayedCall;
  typedef std::vector<DelayedCall> DelayedCalls;

 public:
  explicit LockHolder(Scheduler* scheduler);
  ~LockHolder();

  static const int64 kWakeupNotSet = -1;

  NamedLock* CreateNamedLock(const StringPiece& name)
      LOCKS_EXCLUDED(mutex_);

  // Called when the ThreadSafeLockManager is destructed.  This results
  // in instant-destruction of the owned MemLockManager, but the rest of
  // the state in LockHolder stays around until the last lock is destructed.
  void ManagerDestroyed();

  // Reschedules any outstanding alarms if the wakeup time has changed.
  void UpdateAlarmMutexHeldAndRelease() UNLOCK_FUNCTION() {
    int64 wakeup_time_us = kWakeupNotSet;
    if (manager_.get() != NULL) {
      int64 wakeup_time_ms = manager_->NextWakeupTimeMs();
      if (wakeup_time_ms != MemLockManager::kNoWakeupsPending) {
        wakeup_time_us = wakeup_time_ms * Timer::kMsUs;
      }
    }

    mutex_->Unlock();
    {
      ScopedMutex lock(scheduler_->mutex());
      if (wakeup_time_us != alarm_time_us_) {
        CancelAlarmSchedulerLockHeld();
        alarm_time_us_ = wakeup_time_us;
        if (wakeup_time_us != kWakeupNotSet) {
          alarm_ = scheduler_->AddAlarmAtUsMutexHeld(
              alarm_time_us_, MakeFunction(this, &LockHolder::Wakeup));
        }
      }
    }
  }

  // Runs any pending events (cancels, steals) for any pending locks.
  void Wakeup() LOCKS_EXCLUDED(mutex_);

  // Called by Lock when it's deleted, so we can stop tracking it.
  void RemoveLock(Lock* lock) EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    int erased = locks_.erase(lock);
    CHECK_EQ(1, erased);
  }

  void CancelAlarmSchedulerLockHeld()
      EXCLUSIVE_LOCKS_REQUIRED(scheduler_->mutex());

  void RunWhenSchedulerUnlocked(Function* callback)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    mutex_->DCheckLocked();
    if (manager_.get() == NULL) {
      LOG(DFATAL) << "All locks are denied when manager is deleted";
      EnqueueCancel(callback);
    } else {
      EnqueueRun(callback);
    }
  }

  void CancelWhenSchedulerUnlocked(Function* callback)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    mutex_->DCheckLocked();
    EnqueueCancel(callback);
  }

  // Wraps callback's Run and Cancel methods with some helper functions that
  // queue up the user-callbacks so they can be called when the scheduler mutex
  // is dropped, rather than while holding it.
  Function* MakeDelayCallback(Function* callback) {
    return MakeFunction(
        this, &LockHolder::RunWhenSchedulerUnlocked,
        &LockHolder::CancelWhenSchedulerUnlocked, callback);
  }

  // Runs the callback once the currently-active lock has been released.
  void EnqueueRun(Function* callback)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    delayed_calls_.push_back(DelayedCall(callback, true));
  }

  // Cancels the callback once the currently-active lock has been released.
  void EnqueueCancel(Function* callback)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    delayed_calls_.push_back(DelayedCall(callback, false));
  }

 private:
  Scheduler* scheduler_;
  scoped_ptr<MemLockManager> manager_ GUARDED_BY(mutex_);
  Scheduler::Alarm* alarm_ GUARDED_BY(scheduler_->mutex());
  int64 alarm_time_us_ GUARDED_BY(scheduler_->mutex());

  // We must keep tabs on all outstanding locks so that we can
  // clear their mutexes (thus disabling them) if the factory
  // is destroyed before the locks are.
  LockSet locks_ GUARDED_BY(mutex_);
  DelayedCalls delayed_calls_ GUARDED_BY(mutex_);
  scoped_ptr<AbstractMutex> mutex_;
};

// We must call the NamedLock callbacks without holding the mutex.
// However all the operations on the MemLockManager, including its
// callbacks, are done under this lock.  So we collect all the
// callbacks for delivery to the user in a vector, and run them after
// releasing the lock.
//
// It is convenient to structure this as a ScopedMutex, so that the
// mutex-release happens on destruction, followed by calling all the
// callback Run/Cancel methods.
class SCOPED_LOCKABLE
ThreadSafeLockManager::LockHolder::ScopedLockRunningDelayedCallbacks {
 public:
  explicit ScopedLockRunningDelayedCallbacks(LockHolder* lock_holder)
      EXCLUSIVE_LOCK_FUNCTION(lock_holder->mutex_)
      : lock_holder_(lock_holder) {
    lock_holder_->mutex_->Lock();
  }

  ~ScopedLockRunningDelayedCallbacks() UNLOCK_FUNCTION() {
    DelayedCalls calls;
    calls.swap(lock_holder_->delayed_calls_);
    lock_holder_->UpdateAlarmMutexHeldAndRelease();
    // Now execute the calls we've transferred to our stack vector.
    // Note that the callbacks we are calling may wind up calling
    // another lock function that recursively instantiates another
    // ScopedLockRunningDelayedCallbacks.  That's fine because we've
    // moved over the contents of delayed_calls_ and unlocked the
    // mutex.
    for (size_t i = 0, n = calls.size(); i < n; ++i) {
      DelayedCall call = calls[i];
      if (call.second) {
        call.first->CallRun();
      } else {
        call.first->CallCancel();
      }
    }
  }

 private:
  LockHolder* lock_holder_;
};

// Implements a NamedLock wrapper that adds mutex semantics for thread safety.
class ThreadSafeLockManager::Lock : public NamedLock {
 public:
  Lock(NamedLock* lock, LockHolder* lock_holder)
      : lock_holder_(lock_holder),
        lock_(lock),
        manager_destroyed_(false) {
  }

  virtual ~Lock() {
    LockHolder::ScopedLockRunningDelayedCallbacks lock(lock_holder_.get());
    if (lock_->Held()) {
      UnlockMutexHeld();
    }
    lock_holder_->RemoveLock(this);

    // Clean up underlying MemLock* while still holding lock_holder_->mutex_.
    lock_.reset(NULL);
  }

  // API implementation:
  virtual void LockTimedWaitStealOld(int64 wait_ms, int64 steal_ms,
                                     Function* callback) {
    LockHolder::ScopedLockRunningDelayedCallbacks lock(lock_holder_.get());
    if (manager_destroyed_) {
      lock_holder_->EnqueueCancel(callback);
    } else {
      callback = lock_holder_->MakeDelayCallback(callback);
      lock_->LockTimedWaitStealOld(wait_ms, steal_ms, callback);
    }
  }

  virtual void LockTimedWait(int64 wait_ms, Function* callback) {
    LockHolder::ScopedLockRunningDelayedCallbacks lock(lock_holder_.get());
    if (manager_destroyed_) {
      lock_holder_->EnqueueCancel(callback);
    } else {
      callback = lock_holder_->MakeDelayCallback(callback);
      lock_->LockTimedWait(wait_ms, callback);
    }
  }

  virtual void Unlock() {
    LockHolder::ScopedLockRunningDelayedCallbacks lock(lock_holder_.get());
    UnlockMutexHeld();
  }

  void UnlockMutexHeld() EXCLUSIVE_LOCKS_REQUIRED(lock_holder_->mutex_) {
    if (!manager_destroyed_) {
      lock_->Unlock();
    }
  }

  virtual bool Held() {
    ScopedMutex lock(lock_holder_->mutex_.get());
    return lock_->Held();
  }

  virtual GoogleString name() const {
    ScopedMutex lock(lock_holder_->mutex_.get());
    return lock_->name();
  }

  // Helper methods for self & for the manager:

  void ManagerDestroyed()
      EXCLUSIVE_LOCKS_REQUIRED(lock_holder_->mutex_) {
    manager_destroyed_ = true;
  }

 private:
  LockHolderPtr lock_holder_;
  scoped_ptr<NamedLock> lock_ GUARDED_BY(lock_holder_->mutex_);
  bool manager_destroyed_ GUARDED_BY(lock_holder_->mutex_);
};

ThreadSafeLockManager::ThreadSafeLockManager(Scheduler* scheduler)
    : lock_holder_(new LockHolder(scheduler)) {
}


ThreadSafeLockManager::~ThreadSafeLockManager() {
  lock_holder_->ManagerDestroyed();
}

NamedLock* ThreadSafeLockManager::CreateNamedLock(const StringPiece& name) {
  return lock_holder_->CreateNamedLock(name);
}

ThreadSafeLockManager::LockHolder::LockHolder(Scheduler* scheduler)
    : scheduler_(scheduler),
      manager_(new MemLockManager(scheduler->timer())),
      alarm_(NULL),
      alarm_time_us_(kWakeupNotSet),
      mutex_(scheduler->thread_system()->NewMutex()) {
}

ThreadSafeLockManager::LockHolder::~LockHolder() {
  ScopedMutex lock(scheduler_->mutex());
  CancelAlarmSchedulerLockHeld();
}

void ThreadSafeLockManager::LockHolder::Wakeup() LOCKS_EXCLUDED(mutex_) {
  {
    ScopedMutex lock(scheduler_->mutex());
    alarm_ = NULL;
    alarm_time_us_ = kWakeupNotSet;
  }
  {
    ScopedLockRunningDelayedCallbacks lock(this);
    manager_->Wakeup();
  }
}

void ThreadSafeLockManager::LockHolder::CancelAlarmSchedulerLockHeld() {
  // Note that in scheduler.cc, FunctionAlarm calls DropMutexActAndCleanup
  // so scheduler_->mutex() doesn't protect us from accessing alarm_ after
  // deleting it.
  if (alarm_ != NULL) {
    Scheduler::Alarm* alarm = alarm_;
    alarm_ = NULL;
    scheduler_->CancelAlarm(alarm);
  }
}

void ThreadSafeLockManager::LockHolder::ManagerDestroyed() {
  {
    ScopedMutex lock(scheduler_->mutex());
    CancelAlarmSchedulerLockHeld();
  }
  {
    LockHolder::ScopedLockRunningDelayedCallbacks lock(this);
    for (LockSet::iterator p = locks_.begin(), e = locks_.end(); p != e; ++p) {
      Lock* lock = *p;
      lock->ManagerDestroyed();
    }
    manager_.reset(NULL);
  }
}

NamedLock* ThreadSafeLockManager::LockHolder::CreateNamedLock(
    const StringPiece& name) {
  ScopedMutex lock(mutex_.get());
  Lock* tlock = new Lock(manager_->CreateNamedLock(name), this);
  locks_.insert(tlock);
  return tlock;
}

}  //  namespace net_instaweb
