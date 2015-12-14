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

#ifndef PAGESPEED_KERNEL_BASE_NAMED_LOCK_TESTER_H_
#define PAGESPEED_KERNEL_BASE_NAMED_LOCK_TESTER_H_

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// The NamedLock class does not have any blocking operations in it, as
// they don't fit well with PageSpeed's asynchronous archicture and with
// plans to make an asynchronous lock manager.  However, in the context
// of unit tests, it is convenient to assume a blocking or schedule-based
// lock manager, and check that the call completes.
class NamedLockTester {
 public:
  explicit NamedLockTester(ThreadSystem* thread_system)
      : acquired_(false),
        failed_(false),
        mutex_(thread_system->NewMutex()) {
  }

  bool TryLock(NamedLock* lock) {
    Clear();
    lock->LockTimedWait(0, MakeFunction(
        this,
        &NamedLockTester::LockAcquired,
        &NamedLockTester::LockFailed));
    Quiesce();
    CHECK(WasCalled()) << "LockTimedWait returned lock operation completed";
    return Acquired();
  }
  bool LockTimedWaitStealOld(int64 wait_ms, int64 steal_ms, NamedLock* lock) {
    Clear();
    lock->LockTimedWaitStealOld(wait_ms, steal_ms, MakeFunction(
        this,
        &NamedLockTester::LockAcquired,
        &NamedLockTester::LockFailed));
    Quiesce();
    CHECK(WasCalled())
        << "LockTimedWaitStealOld returned lock operation completed";
    return Acquired();
  }
  bool LockTimedWait(int64 wait_ms, NamedLock* lock) {
    Clear();
    lock->LockTimedWait(wait_ms, MakeFunction(
        this,
        &NamedLockTester::LockAcquired,
        &NamedLockTester::LockFailed));
    Quiesce();
    CHECK(WasCalled()) << "LockTimedWait returned lock operation completed";
    return Acquired();
  }

  // Tests the specific case where the callback for new_lock deletes old_lock.
  // The likely failure case is a SEGV, though you must verify a true return
  // code or the test didn't run. Frees both locks.
  bool UnlockWithDelete(NamedLock* old_lock, NamedLock* new_lock) {
    scoped_ptr<NamedLock> new_lock_deleter(new_lock);
    lock_for_deletion_.reset(old_lock);
    CHECK(old_lock->Held()) << "UnlockWithDelete old_lock must be held";
    CHECK(!new_lock->Held()) << "UnlockWithDelete new_lock must not be held";
    Clear();
    new_lock->LockTimedWait(
        60000 /* wait_ms */,  // Long enough to ensure it doesn't timeout.
        MakeFunction(
            this,
            &NamedLockTester::DeleteLock,
            &NamedLockTester::LockFailed));
    old_lock->Unlock();
    Quiesce();
    CHECK(WasCalled()) << "UnlockWithDelete lock operation did not complete";
    return Acquired();
  }

  // As for UnlockWithDelete, but for a steal. Frees both locks.
  bool StealWithDelete(int64 steal_ms, NamedLock* old_lock,
                       NamedLock* new_lock) {
    scoped_ptr<NamedLock> new_lock_deleter(new_lock);
    lock_for_deletion_.reset(old_lock);
    CHECK(old_lock->Held()) << "StealWithDelete old_lock must be held";
    CHECK(!new_lock->Held()) << "StealWithDelete new_lock must not be held";
    Clear();
    new_lock->LockTimedWaitStealOld(
        0 /* wait_ms */, steal_ms,
        MakeFunction(
            this,
            &NamedLockTester::DeleteLock,
            &NamedLockTester::LockFailed));
    Quiesce();
    CHECK(WasCalled()) << "StealWithDelete lock operation did not complete";
    return Acquired();
  }

  void DeleteLock() {
    ScopedMutex lock(mutex_.get());
    acquired_ = true;
    lock_for_deletion_.reset();
  }

  void Quiesce() {
    if (quiesce_.get() != NULL) {
      quiesce_->CallRun();
      quiesce_->Reset();
    }
  }

  // Sets a function to be called after calling an asynchronous locking
  // method, prior to testing acuisition status.  Onwership of this function
  // transfers to this class.
  void set_quiesce(Function* quiesce) {
    quiesce->set_delete_after_callback(false);
    quiesce_.reset(quiesce);
  }

 private:
  void Clear() {
    ScopedMutex lock(mutex_.get());
    acquired_ = false;
    failed_ = false;
  }

  void LockAcquired() {
    ScopedMutex lock(mutex_.get());
    acquired_ = true;
  }
  void LockFailed() {
    ScopedMutex lock(mutex_.get());
    failed_ = true;
  }
  bool WasCalled() {
    ScopedMutex lock(mutex_.get());
    return acquired_ || failed_;
  }
  bool Acquired() {
    ScopedMutex lock(mutex_.get());
    return acquired_;
  }

  bool acquired_;
  bool failed_;
  scoped_ptr<AbstractMutex> mutex_;
  scoped_ptr<Function> quiesce_;
  scoped_ptr<NamedLock> lock_for_deletion_;

  DISALLOW_COPY_AND_ASSIGN(NamedLockTester);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_NAMED_LOCK_TESTER_H_
