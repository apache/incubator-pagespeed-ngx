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

// Unit test the ThreadSafeLockManager.
//
// TODO(jmarantz): factor out common code between this and
// mem_lock_manager_test.cc.

#include "pagespeed/kernel/util/threadsafe_lock_manager.h"

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/named_lock_tester.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/thread/mock_scheduler.h"
#include "pagespeed/kernel/thread/scheduler.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/lock_manager_spammer.h"

namespace net_instaweb {

namespace {

const char kLock1[] = "lock1";
const char kLock2[] = "lock2";

const int64 kStealMs = 50000;
const int64 kWaitMs = 10000;

class ThreadsafeLockManagerTest : public testing::Test {
 public:
  void DeleteManager() {
    lock_manager_.reset(NULL);
  }

 protected:
  ThreadsafeLockManagerTest()
      : thread_system_(Platform::CreateThreadSystem()),
        timer_(thread_system_->NewMutex(), 0),
        scheduler_(new MockScheduler(thread_system_.get(), &timer_)),
        lock_manager_(new ThreadSafeLockManager(scheduler_.get())),
        tester_(thread_system_.get()) {
    tester_.set_quiesce(MakeFunction(this,
                                     &ThreadsafeLockManagerTest::Quiesce));
  }

  NamedLock* MakeLock(const StringPiece& name) {
    return lock_manager_->CreateNamedLock(name);
  }

  void AllLocksFail(const scoped_ptr<NamedLock>& lock) {
    scoped_ptr<NamedLock> lock2(MakeLock(lock->name()));
    EXPECT_FALSE(TryLock(lock2));
    EXPECT_FALSE(TryLockStealOld(kStealMs, lock2));
    EXPECT_FALSE(LockTimedWaitStealOld(kWaitMs, kStealMs, lock2));
    EXPECT_FALSE(LockTimedWait(kWaitMs, lock2));
  }

  MockTimer* timer() {
    return &timer_;
  }

  bool TryLock(const scoped_ptr<NamedLock>& lock) {
    return tester_.TryLock(lock.get());
  }

  bool TryLockStealOld(int64 steal_ms, const scoped_ptr<NamedLock>& lock) {
    return tester_.LockTimedWaitStealOld(0 /* wait_ms */, steal_ms, lock.get());
  }

  bool LockTimedWaitStealOld(int64 wait_ms, int64 steal_ms,
                             const scoped_ptr<NamedLock>& lock) {
    return tester_.LockTimedWaitStealOld(wait_ms, steal_ms, lock.get());
  }

  bool LockTimedWait(int64 wait_ms, const scoped_ptr<NamedLock>& lock) {
    return tester_.LockTimedWait(wait_ms, lock.get());
  }

  void Quiesce() {
    int64 wakeup_us;
    bool ran_alarms;
    ScopedMutex lock(scheduler_->mutex());
    while ((wakeup_us = scheduler_->RunAlarms(&ran_alarms)) != 0) {
      timer_.SetTimeUs(wakeup_us);
    }
  }

  void LogGrant(const char* name) {
    StrAppend(&log_, name, "(grant) ");
  }

  void LogDeny(const char* name) {
    StrAppend(&log_, name, "(deny) ");
  }

  Function* LogFunction(const char* name) {
    return MakeFunction(this,
                        &ThreadsafeLockManagerTest::LogGrant,
                        &ThreadsafeLockManagerTest::LogDeny,
                        name);
  }

 protected:
  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;
  scoped_ptr<Scheduler> scheduler_;
  GoogleMessageHandler handler_;
  scoped_ptr<ThreadSafeLockManager> lock_manager_;
  NamedLockTester tester_;
  GoogleString log_;
};

TEST_F(ThreadsafeLockManagerTest, LockUnlock) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  // Just do pairs of matched lock / unlock, making sure
  // we can't lock while the lock is held.
  EXPECT_TRUE(TryLock(lock1));
  EXPECT_TRUE(lock1->Held());
  AllLocksFail(lock1);

  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());

  EXPECT_TRUE(TryLock(lock1));
  EXPECT_TRUE(lock1->Held());
  AllLocksFail(lock1);

  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());

  EXPECT_TRUE(LockTimedWait(kWaitMs, lock1));
  EXPECT_TRUE(lock1->Held());
  AllLocksFail(lock1);

  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());

  EXPECT_TRUE(TryLockStealOld(kStealMs, lock1));
  EXPECT_TRUE(lock1->Held());
  AllLocksFail(lock1);

  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());

  EXPECT_TRUE(LockTimedWaitStealOld(kWaitMs, kStealMs, lock1));
  EXPECT_TRUE(lock1->Held());
  AllLocksFail(lock1);

  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
}

TEST_F(ThreadsafeLockManagerTest, DoubleLockUnlock) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock11(MakeLock(kLock1));
  // Just do pairs of matched lock / unlock, but make sure
  // we hold a separate lock object with the same lock name.
  EXPECT_TRUE(TryLock(lock1));
  EXPECT_TRUE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
  AllLocksFail(lock11);
  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
  EXPECT_FALSE(lock11->Held());

  EXPECT_TRUE(TryLock(lock1));
  AllLocksFail(lock11);
  EXPECT_TRUE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
  EXPECT_FALSE(lock11->Held());

  EXPECT_TRUE(LockTimedWait(kWaitMs, lock1));
  EXPECT_TRUE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
  AllLocksFail(lock11);
  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
  EXPECT_FALSE(lock11->Held());

  EXPECT_TRUE(TryLockStealOld(kStealMs, lock1));
  EXPECT_TRUE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
  AllLocksFail(lock11);
  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
  EXPECT_FALSE(lock11->Held());

  EXPECT_TRUE(LockTimedWaitStealOld(kWaitMs, kStealMs, lock1));
  EXPECT_TRUE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
  AllLocksFail(lock11);
  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
}

// From this point, we assume all the locking routines hold
// the lock in equivalent ways.  Now we just need to check that
// their timeout behaviors are correct.

TEST_F(ThreadsafeLockManagerTest, UnlockOnDestruct) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  {
    scoped_ptr<NamedLock> lock11(MakeLock(kLock1));
    EXPECT_TRUE(TryLock(lock11));
    EXPECT_FALSE(TryLock(lock1));
    // Should implicitly unlock on lock11 destructor call.
  }
  EXPECT_TRUE(TryLock(lock1));
}

TEST_F(ThreadsafeLockManagerTest, LockIndependence) {
  // Differently-named locks are different.
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock1a(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock2(MakeLock(kLock2));
  scoped_ptr<NamedLock> lock2a(MakeLock(kLock2));
  EXPECT_TRUE(TryLock(lock1));
  EXPECT_TRUE(TryLock(lock2));
  EXPECT_FALSE(TryLock(lock1a));
  EXPECT_FALSE(TryLock(lock2a));
  lock2->Unlock();
  EXPECT_FALSE(TryLock(lock1a));
  EXPECT_TRUE(TryLock(lock2));
}

TEST_F(ThreadsafeLockManagerTest, TimeoutFail) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(TryLock(lock1));
  EXPECT_TRUE(lock1->Held());
  int64 start_ms = timer()->NowMs();
  scoped_ptr<NamedLock> lock1a(MakeLock(kLock1));  // Same name, new object.
  EXPECT_FALSE(LockTimedWait(kWaitMs, lock1a));
  EXPECT_TRUE(lock1->Held());  // was never unlocked...
  int64 end_ms = timer()->NowMs();
  EXPECT_LE(start_ms + kWaitMs, end_ms);
}

TEST_F(ThreadsafeLockManagerTest, StealOld) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock1a(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock1b(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock1c(MakeLock(kLock1));
  EXPECT_TRUE(TryLock(lock1));
  // Now we can't steal the lock until after >kStealMs has elapsed.
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1a));
  timer()->AdvanceMs(kStealMs);
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1a));
  // But 1ms longer than kStealMs and we can steal the lock.
  EXPECT_TRUE(lock1->Held());
  timer()->AdvanceMs(1);
  EXPECT_TRUE(TryLockStealOld(kStealMs, lock1a));
  EXPECT_FALSE(lock1->Held());  // Stealing turns releases the lock.
  // After steal the timer should reset.
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1b));
  timer()->AdvanceMs(kStealMs);
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1b));
  EXPECT_FALSE(lock1->Held());
  EXPECT_TRUE(lock1a->Held());   // 1a still has the lock
  EXPECT_FALSE(lock1b->Held());  // 1b doesn't have it yet.
  // But again expire after >kStealMs elapses.
  timer()->AdvanceMs(1);
  EXPECT_TRUE(TryLockStealOld(kStealMs, lock1c));
  EXPECT_FALSE(lock1b->Held());
  EXPECT_TRUE(lock1c->Held());
}

TEST_F(ThreadsafeLockManagerTest, BlockingStealOld) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock1a(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock1b(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock1c(MakeLock(kLock1));
  EXPECT_TRUE(TryLock(lock1));
  // Now a call to LockTimedWaitStealOld should block until kStealMs has
  // elapsed.
  int64 start_ms = timer()->NowMs();
  EXPECT_TRUE(LockTimedWaitStealOld(kStealMs * 100, kStealMs, lock1a));
  int64 end_ms = timer()->NowMs();
  EXPECT_EQ(start_ms + kStealMs, end_ms);
  EXPECT_GT(start_ms + kStealMs * 100, end_ms);
  // Again the timer should reset after the lock is obtained.
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1b));
  timer()->AdvanceMs(kStealMs);
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1b));
  timer()->AdvanceMs(1);
  EXPECT_TRUE(TryLockStealOld(kStealMs, lock1c));
}

TEST_F(ThreadsafeLockManagerTest, WaitStealOld) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(TryLock(lock1));
  int64 start_ms = timer()->NowMs();
  // If we start now, we'll time out with time to spare.
  scoped_ptr<NamedLock> lock1a(MakeLock(kLock1));  // Same name, new object.
  EXPECT_FALSE(LockTimedWaitStealOld(kWaitMs, kStealMs, lock1a));
  int64 end_ms = timer()->NowMs();
  EXPECT_LE(start_ms + kWaitMs, end_ms);
  EXPECT_GT(start_ms + kStealMs, end_ms);
  // Advance time so that the lock timeout is within the wait time.
  int64 time_ms = start_ms + kStealMs - kWaitMs / 2;
  timer()->SetTimeUs(1000 * time_ms);
  start_ms = timer()->NowMs();
  EXPECT_TRUE(LockTimedWaitStealOld(kWaitMs, kStealMs, lock1a));
  end_ms = timer()->NowMs();
  EXPECT_GT(start_ms + kWaitMs, end_ms);
}

TEST_F(ThreadsafeLockManagerTest, NoWaitUnlockDeletesOld) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(TryLock(lock1));
  NamedLock* lock1a = MakeLock(kLock1);  // Same name, new object.
  EXPECT_TRUE(tester_.UnlockWithDelete(lock1.release(), lock1a));
}

TEST_F(ThreadsafeLockManagerTest, NoWaitStealDeletesOld) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(TryLock(lock1));
  NamedLock* lock1a = MakeLock(kLock1);  // Same name, new object.
  timer()->AdvanceMs(kStealMs + 1);
  EXPECT_TRUE(tester_.StealWithDelete(kStealMs, lock1.release(), lock1a));
}

TEST_F(ThreadsafeLockManagerTest, MultipleLocksSameTimeouts) {
  scoped_ptr<NamedLock> a(MakeLock(kLock1));
  scoped_ptr<NamedLock> b(MakeLock(kLock1));
  scoped_ptr<NamedLock> c(MakeLock(kLock1));
  scoped_ptr<NamedLock> d(MakeLock(kLock2));
  scoped_ptr<NamedLock> e(MakeLock(kLock2));
  a->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("a"));
  b->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("b"));
  c->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("c"));
  d->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("d"));
  e->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("e"));
  Quiesce();

  // Because kStealMs (50k) is larger than kWaitMs (10k), both b and
  // c will be cancelled before they steal the lock from a, ane e
  // will be canceled before i steals the lock from d.
  EXPECT_STREQ("a(grant) d(grant) b(deny) c(deny) e(deny) ", log_);
  EXPECT_EQ(kWaitMs, timer()->NowMs());
  log_.clear();

  // However, if we add some more delay (to 45k ms) and try to lock b and c
  // again, then the Steal should occur for b, but c will timeout.
  timer()->SetTimeMs(kStealMs - (kWaitMs / 2) /* 45000 */);
  b->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("b"));
  c->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("c"));
  Quiesce();
  EXPECT_STREQ("b(grant) c(deny) ", log_);
}

TEST_F(ThreadsafeLockManagerTest, OverrideSteal) {
  scoped_ptr<NamedLock> a(MakeLock(kLock1));
  scoped_ptr<NamedLock> b(MakeLock(kLock1));
  scoped_ptr<NamedLock> c(MakeLock(kLock1));
  a->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("a"));
  timer()->SetTimeMs(kStealMs - (kWaitMs / 2) /* 45000 */);
  b->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("b"));

  // Before 'b' gets a chance to steal a's lock, c comes in with
  // a smaller steal delay (1 ms less), so it will get to steal the
  // lock and not a.
  c->LockTimedWaitStealOld(kWaitMs, kStealMs - 1, LogFunction("c"));
  Quiesce();
  EXPECT_STREQ("a(grant) c(grant) b(deny) ", log_);
}

TEST_F(ThreadsafeLockManagerTest, UnlockRevealingNewStealer) {
  scoped_ptr<NamedLock> a(MakeLock(kLock1));
  scoped_ptr<NamedLock> b(MakeLock(kLock1));
  scoped_ptr<NamedLock> c(MakeLock(kLock1));
  a->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("a"));
  b->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("b"));
  c->LockTimedWaitStealOld(2*kStealMs, kStealMs, LogFunction("c"));
  timer()->SetTimeMs(1);
  scheduler_->Wakeup();
  EXPECT_TRUE(a->Held());
  timer()->SetTimeMs(kStealMs - (kWaitMs / 2) /* 45000 */);
  a->Unlock();                                // grants B, C is stealer
  Quiesce();                                  // C steals cause it has long wait
  EXPECT_STREQ("a(grant) b(grant) c(grant) ", log_);
}

TEST_F(ThreadsafeLockManagerTest, CloseManagerWithActiveLocks) {
  scoped_ptr<NamedLock> a(MakeLock(kLock1));
  scoped_ptr<NamedLock> b(MakeLock(kLock1));
  scoped_ptr<NamedLock> c(MakeLock(kLock1));
  scoped_ptr<NamedLock> d(MakeLock(kLock1));
  a->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("a"));
  b->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("b"));
  c->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("c"));
  lock_manager_.reset(NULL);  // Locks still active.
  EXPECT_STREQ("a(grant) b(deny) c(deny) ", log_);
}

/*
 * TODO(jmarantz): deleting the schedule with outstanding locks
 * crashes the world.  Find some way to fix this.
 *
 * TEST_F(ThreadsafeLockManagerTest, CloseSchedulerWithActiveLocks) {
 *   scoped_ptr<NamedLock> a(MakeLock(kLock1));
 *   scoped_ptr<NamedLock> b(MakeLock(kLock1));
 *   scoped_ptr<NamedLock> c(MakeLock(kLock1));
 *   scoped_ptr<NamedLock> d(MakeLock(kLock1));
 *   a->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("a"));
 *   b->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("b"));
 *   c->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("c"));
 *   scheduler_.reset(NULL);  // Locks still active.
 *   EXPECT_STREQ("a(grant) b(deny) c(deny) ", log_);
 * }
*/

TEST_F(ThreadsafeLockManagerTest, DeletePendingLock) {
  scoped_ptr<NamedLock> a(MakeLock(kLock1));
  scoped_ptr<NamedLock> b(MakeLock(kLock1));
  scoped_ptr<NamedLock> c(MakeLock(kLock1));
  a->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("a"));
  b->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("b"));
  c->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("c"));
  timer()->SetTimeMs(1);
  scheduler_->Wakeup();
  EXPECT_TRUE(a->Held());
  b.reset(NULL);                              // Denies B.
  a->Unlock();                                // Grants C.
  Quiesce();
  EXPECT_STREQ("a(grant) b(deny) c(grant) ", log_);
}

TEST_F(ThreadsafeLockManagerTest, TryLockWithDeletedManager) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  lock_manager_.reset(NULL);
  EXPECT_FALSE(TryLock(lock1));
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1));
}

TEST_F(ThreadsafeLockManagerTest, UnlockWithDeletedManager) {
  scoped_ptr<NamedLock> a(MakeLock(kLock1));
  scoped_ptr<NamedLock> b(MakeLock(kLock1));
  a->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("a"));
  b->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("b"));
  lock_manager_.reset(NULL);
  a->Unlock();
  EXPECT_STREQ("a(grant) b(deny) ", log_);
}

TEST_F(ThreadsafeLockManagerTest, TryStealLockWithDeletedManager) {
  scoped_ptr<NamedLock> a(MakeLock(kLock1));
  scoped_ptr<NamedLock> b(MakeLock(kLock1));
  a->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("a"));
  timer()->AdvanceMs(kStealMs - 2);
  ThreadsafeLockManagerTest* test = this;
  scheduler_->AddAlarmAtUs(timer()->NowUs() + 100, MakeFunction(
      test, &ThreadsafeLockManagerTest::DeleteManager));
  b->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("b"));
  Quiesce();
  EXPECT_STREQ("a(grant) b(deny) ", log_);
}

TEST_F(ThreadsafeLockManagerTest, RunStolenLockWithDeletedManager) {
  scoped_ptr<NamedLock> a(MakeLock(kLock1));
  scoped_ptr<NamedLock> b(MakeLock(kLock1));
  a->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("a"));
  timer()->AdvanceMs(kStealMs - 2);
  ThreadsafeLockManagerTest* test = this;
  b->LockTimedWaitStealOld(kWaitMs, kStealMs, LogFunction("b"));
  scheduler_->AddAlarmAtUs(timer()->NowUs() + 1, MakeFunction(
      test, &ThreadsafeLockManagerTest::DeleteManager));
  timer()->AdvanceMs(4);  // Advances past steal-time, but not past wait-time.
  Quiesce();
  EXPECT_STREQ("a(grant) b(deny) ", log_);
}

// Use a separate class for the spammer test that uses a real-time timer
// and scheduler, rather than MockTimer and MockScheduler.
//
// TODO(jmarantz): move this class and tests to lock_manager_spammer.cc,
// and rename that to lock_manager_spammer_test.cc.  I have it structured
// like this now for reviewability.
class ThreadsafeLockManagerSpammerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    timer_.reset(Platform::CreateTimer());
    thread_system_.reset(Platform::CreateThreadSystem());
    scheduler_.reset(new Scheduler(thread_system_.get(), timer_.get()));
    lock_manager_.reset(new ThreadSafeLockManager(scheduler_.get()));
  }

  scoped_ptr<Timer> timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<Scheduler> scheduler_;
  scoped_ptr<ThreadSafeLockManager> lock_manager_;
};

TEST_F(ThreadsafeLockManagerSpammerTest, NoDelays) {
  LockManagerSpammer::RunTests(10,                     // num threads
                               3,                      // num iters
                               100,                    // num names
                               false,                  // expecting_denials,
                               false,                  // delay_unlocks
                               lock_manager_.get(),
                               scheduler_.get());
}

TEST_F(ThreadsafeLockManagerSpammerTest, DelaysNoDenials) {
  // By having only 2 threads contenting for locks, even though we are delaying
  // the unlocks, the locks will be stolen before they expire.
  LockManagerSpammer::RunTests(2,                      // num threads
                               1,                      // num iters
                               100,                    // num names
                               false,                  // expecting_denials,
                               true,                   // delay_unlocks
                               lock_manager_.get(),
                               scheduler_.get());
}

TEST_F(ThreadsafeLockManagerSpammerTest, DelaysAndDenials) {
  // The cumulative delays on the lock-stealing will cause some of the later
  // thread lock requests to be denied.
  LockManagerSpammer::RunTests(10,                     // num threads
                               1,                      // num iters
                               100,                    // num names
                               true,                   // expecting_denials,
                               true,                   // delay_unlocks
                               lock_manager_.get(),
                               scheduler_.get());
}

}  // namespace

}  // namespace net_instaweb
