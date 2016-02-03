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

#include "pagespeed/kernel/util/lock_manager_spammer.h"

#include <vector>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/dynamic_annotations.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/util/threadsafe_lock_manager.h"

namespace {

int WaitMs() { return RunningOnValgrind() ? 2000 : 200; }
int StealMs() { return RunningOnValgrind() ? 1000 : 100; }

// In our test, we set the lock wait timeout at 100ms and the steal
// time to 200ms.  But we insert timing delays of 150ms so that we
// successfully steal a lock that is not released in the second round
// of lock-requests.  But for the third round of lock-requests, which
// occurs at 300ms, the timeout will expire.
//
// All the times are multipled by 10 for valgrind, since these are real-time.
// Leaving them as is results in a modest percentage of flakes in valgrind
// tests.
int DelayMs() { return (WaitMs() + StealMs()) / 2; }  // 1500ms or 150ms.

}  // namespace

namespace net_instaweb {

LockManagerSpammer::LockManagerSpammer(Scheduler* scheduler,
                                       ThreadSystem::ThreadFlags flags,
                                       const StringVector& lock_names,
                                       ThreadSafeLockManager* lock_manager,
                                       bool expecting_denials,
                                       bool delay_unlocks,
                                       int index,
                                       int num_iters,
                                       int num_names,
                                       CountDown* pending_threads)
    : Thread(scheduler->thread_system(), "lock_manager_spammer", flags),
      scheduler_(scheduler),
      lock_names_(lock_names),
      lock_manager_(lock_manager),
      expecting_denials_(expecting_denials),
      delay_unlocks_(delay_unlocks),
      index_(index),
      num_iters_(num_iters),
      num_names_(num_names),
      mutex_(scheduler->thread_system()->NewMutex()),
      condvar_(mutex_->NewCondvar()),
      grants_(0),
      denials_(0),
      pending_threads_(pending_threads) {
}

LockManagerSpammer::~LockManagerSpammer() {
}

LockManagerSpammer::CountDown::CountDown(Scheduler* scheduler,
                                         int initial_value)
    : scheduler_(scheduler),
      value_(initial_value) {
}

LockManagerSpammer::CountDown::~CountDown() {
}

void LockManagerSpammer::CountDown::RunAlarmsTillThreadsComplete() {
  ScopedMutex lock(scheduler_->mutex());
  bool running = true;
  while ((value_ != 0) || running) {
    running = scheduler_->ProcessAlarmsOrWaitUs(100);
  }
}

void LockManagerSpammer::CountDown::Decrement() {
  ScopedMutex lock(scheduler_->mutex());
  --value_;
  if (value_ == 0) {
    scheduler_->Signal();
  }
}

void LockManagerSpammer::RunTests(int num_threads,
                                  int num_iters,
                                  int num_names,
                                  bool expecting_denials,
                                  bool delay_unlocks,
                                  ThreadSafeLockManager* lock_manager,
                                  Scheduler* scheduler) {
  std::vector<LockManagerSpammer*> spammers(num_threads);
  CountDown pending_threads(scheduler, num_threads);

  StringVector lock_names;
  const char name_pattern[] = "name%d";
  for (int i = 0; i < num_names; ++i) {
    lock_names.push_back(StringPrintf(name_pattern, i));
  }

  // First, create all the threads.
  for (int i = 0; i < num_threads; ++i) {
    spammers[i] = new LockManagerSpammer(
        scheduler, ThreadSystem::kJoinable, lock_names,
        lock_manager,
        expecting_denials, delay_unlocks, i, num_iters,
        num_names,
        &pending_threads);
  }

  // Then, start them.
  for (int i = 0; i < num_threads; ++i) {
    spammers[i]->Start();
  }

  pending_threads.RunAlarmsTillThreadsComplete();

  // Finally, wait for them to complete by joining them.
  for (int i = 0; i < num_threads; ++i) {
    spammers[i]->Join();
    delete spammers[i];
  }
}

void LockManagerSpammer::Run() {
  int num_locks = num_iters_ * num_names_;
  std::vector<NamedLock*> locks;
  locks.reserve(num_locks);

  for (int i = 0; i < num_iters_; ++i) {
    for (int j = 0; j < num_names_; ++j) {
      NamedLock* lock = lock_manager_->CreateNamedLock(lock_names_[j]);
      Function* callback = MakeFunction(
          this, &LockManagerSpammer::Granted, &LockManagerSpammer::Denied,
          lock);
      lock->LockTimedWaitStealOld(WaitMs(), StealMs(), callback);
      locks.push_back(lock);
    }
  }

  {
    ScopedMutex lock(mutex_.get());
    while (denials_ + grants_ < num_locks) {
      condvar_->Wait();
      while (!queued_unlocks_.empty()) {
        LockVector locks;
        locks.swap(queued_unlocks_);
        mutex_->Unlock();
        for (int i = 0, n = locks.size(); i < n; ++i) {
          locks[i]->Unlock();
        }
        mutex_->Lock();
      }
    }
    if (!expecting_denials_) {
      EXPECT_EQ(0, denials_);
    }
  }
  for (int i = 0; i < num_locks; ++i) {
    delete locks[i];
  }
  pending_threads_->Decrement();
}

void LockManagerSpammer::Granted(NamedLock* lock) {
  if (delay_unlocks_) {
    // Schedule the unlocking for some future point in time that's
    // beyond the steal-time but smaller than the wait time.
    int64 wakeup_time_us = scheduler_->timer()->NowUs() +
        DelayMs() * Timer::kMsUs;
    Function* callback = MakeFunction(
        this, &LockManagerSpammer::UnlockAfterGrant, lock);
    scheduler_->AddAlarmAtUs(wakeup_time_us, callback);
  } else {
    lock->Unlock();
    {
      ScopedMutex mutex_lock(mutex_.get());
      ++grants_;
      condvar_->Signal();
    }
  }
}

void LockManagerSpammer::UnlockAfterGrant(NamedLock* lock) {
  ScopedMutex mutex_lock(mutex_.get());
  queued_unlocks_.push_back(lock);
  ++grants_;
  condvar_->Signal();
}

void LockManagerSpammer::Denied(NamedLock* lock) {
  ScopedMutex scoped_mutex(mutex_.get());
  ++denials_;
  condvar_->Signal();
}

}  // namespace net_instaweb
