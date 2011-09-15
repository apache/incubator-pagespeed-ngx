// Copyright 2011 Google Inc. All Rights Reserved.
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
// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/util/public/scheduler_based_abstract_lock.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/thread.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

namespace {

static const int kShortMs = 10;
static const int kLongMs = 100;

class SchedulerBasedAbstractLockTest : public testing::Test {
 protected:
  SchedulerBasedAbstractLockTest()
      : timer_(0),
        thread_system_(ThreadSystem::CreateThreadSystem()),
        scheduler_(thread_system_.get(),
                   QueuedWorkerPool::SequenceVector(),
                   &timer_) {
  }

  MockTimer timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockScheduler scheduler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerBasedAbstractLockTest);
};

// A mock lock base class
class MockLockBase : public SchedulerBasedAbstractLock {
 public:
  explicit MockLockBase(Scheduler* scheduler) : scheduler_(scheduler) { }
  virtual ~MockLockBase() { }
  virtual Scheduler* scheduler() const { return scheduler_; }
  // None of the mock locks actually implement locking, so
  // unlocking is a no-op.
  virtual void Unlock() { }

 protected:
  Scheduler* scheduler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLockBase);
};

// A mock lock that always claims locking happened
class AlwaysLock : public MockLockBase {
 public:
  explicit AlwaysLock(Scheduler* scheduler) : MockLockBase(scheduler) { }
  virtual ~AlwaysLock() { }
  virtual bool TryLock() {
    return true;
  }
  virtual bool TryLockStealOld(int64 timeout_ms) {
    return true;
  }
  virtual GoogleString name() { return GoogleString("AlwaysLock"); }
 private:
  DISALLOW_COPY_AND_ASSIGN(AlwaysLock);
};

// A mock lock that always claims lock attempts failed
class NeverLock : public MockLockBase {
 public:
  explicit NeverLock(Scheduler* scheduler) : MockLockBase(scheduler) { }
  virtual ~NeverLock() { }
  virtual bool TryLock() {
    return false;
  }
  virtual bool TryLockStealOld(int64 timeout_ms) {
    return false;
  }
  virtual GoogleString name() { return GoogleString("NeverLock"); }
 private:
  DISALLOW_COPY_AND_ASSIGN(NeverLock);
};

// A mock lock that can only be locked by stealing after a timeout.
class StealOnlyLock : public NeverLock {
 public:
  explicit StealOnlyLock(Scheduler* scheduler)
      : NeverLock(scheduler),
        last_hold_time_ms_(scheduler_->timer()->NowMs()) {
  }
  virtual bool TryLockStealOld(int64 timeout_ms) {
    int64 timeout_time_ms = last_hold_time_ms_ + timeout_ms;
    int64 now_ms = scheduler()->timer()->NowMs();
    if (timeout_time_ms <= now_ms) {
      last_hold_time_ms_ = now_ms;
      return true;
    } else {
      return false;
    }
  }
  virtual GoogleString name() { return GoogleString("StealOnlyLock"); }
 private:
  int64 last_hold_time_ms_;

  DISALLOW_COPY_AND_ASSIGN(StealOnlyLock);
};

// Simple tests that involve either failed try or successfully obtaining lock.
// Note that we always capture start times before lock construction, to account
// for possible passage of mock time due to time queries during lock
// construction.
TEST_F(SchedulerBasedAbstractLockTest, AlwaysLock) {
  int64 start = timer_.NowMs();
  AlwaysLock always_lock(&scheduler_);
  EXPECT_TRUE(always_lock.LockTimedWait(kLongMs));

  SchedulerBlockingFunction block1(&scheduler_);
  always_lock.LockTimedWait(kLongMs, &block1);
  EXPECT_TRUE(block1.Block());

  EXPECT_TRUE(always_lock.LockTimedWaitStealOld(kLongMs, kLongMs));

  SchedulerBlockingFunction block2(&scheduler_);
  always_lock.LockTimedWaitStealOld(kLongMs, kLongMs, &block2);
  EXPECT_TRUE(block2.Block());

  // Nothing should ever have slept.
  int64 end = timer_.NowMs();
  EXPECT_EQ(0, end - start);
}

TEST_F(SchedulerBasedAbstractLockTest, TimeoutHappens) {
  int64 start = timer_.NowMs();
  NeverLock never_lock(&scheduler_);
  EXPECT_FALSE(never_lock.LockTimedWait(kShortMs));
  int64 end = timer_.NowMs();
  // At least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // But not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(SchedulerBasedAbstractLockTest, CallbackTimeoutHappens) {
  int64 start = timer_.NowMs();
  NeverLock never_lock(&scheduler_);
  SchedulerBlockingFunction block(&scheduler_);
  never_lock.LockTimedWait(kShortMs, &block);
  EXPECT_FALSE(block.Block());
  int64 end = timer_.NowMs();
  // At least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // But not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(SchedulerBasedAbstractLockTest, TimeoutHappensStealOld) {
  int64 start = timer_.NowMs();
  NeverLock never_lock(&scheduler_);
  EXPECT_FALSE(never_lock.LockTimedWaitStealOld(kShortMs, kLongMs));
  int64 end = timer_.NowMs();
  // Again at least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // But not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(SchedulerBasedAbstractLockTest, CallbackTimeoutHappensStealOld) {
  int64 start = timer_.NowMs();
  NeverLock never_lock(&scheduler_);
  SchedulerBlockingFunction block(&scheduler_);
  never_lock.LockTimedWaitStealOld(kShortMs, kLongMs, &block);
  EXPECT_FALSE(block.Block());
  int64 end = timer_.NowMs();
  // Again at least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // But not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(SchedulerBasedAbstractLockTest, TimeoutBeforeSteal) {
  int64 start = timer_.NowMs();
  StealOnlyLock steal_only_lock(&scheduler_);
  EXPECT_FALSE(steal_only_lock.LockTimedWaitStealOld(kShortMs, kLongMs));
  int64 end = timer_.NowMs();
  // Again at least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // But not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(SchedulerBasedAbstractLockTest, CallbackTimeoutBeforeSteal) {
  int64 start = timer_.NowMs();
  StealOnlyLock steal_only_lock(&scheduler_);
  SchedulerBlockingFunction block(&scheduler_);
  steal_only_lock.LockTimedWaitStealOld(kShortMs, kLongMs, &block);
  EXPECT_FALSE(block.Block());
  int64 end = timer_.NowMs();
  // Again at least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // But not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(SchedulerBasedAbstractLockTest, StealBeforeTimeout) {
  int64 start = timer_.NowMs();
  StealOnlyLock steal_only_lock(&scheduler_);
  EXPECT_TRUE(steal_only_lock.LockTimedWaitStealOld(kLongMs, kShortMs));
  int64 end = timer_.NowMs();
  // Again, at least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // And again, not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(SchedulerBasedAbstractLockTest, CallbackStealBeforeTimeout) {
  int64 start = timer_.NowMs();
  StealOnlyLock steal_only_lock(&scheduler_);
  SchedulerBlockingFunction block(&scheduler_);
  steal_only_lock.LockTimedWaitStealOld(kLongMs, kShortMs, &block);
  EXPECT_TRUE(block.Block());
  int64 end = timer_.NowMs();
  // Again, at least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // And again, not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

// A wrapper that locks before operating on the underlying timer.  This really
// only makes sense for a MockTimer, as most timers inherit any necessary
// synchronization from the underlying library and OS (where it's done far more
// efficiently).
class LockedTimer : public Timer {
 public:
  LockedTimer(Timer* timer, ThreadSystem::CondvarCapableMutex* mutex)
      : timer_(timer),
        mutex_(mutex),
        sleep_wakeup_condvar_(mutex->NewCondvar()) { }
  virtual ~LockedTimer() { }
  virtual void SleepUs(int64 us) {
    {
      ScopedMutex lock(mutex_);
      timer_->SleepUs(us);
      sleep_wakeup_condvar_->Signal();
    }
  }
  virtual int64 NowUs() const {
    ScopedMutex lock(mutex_);
    return timer_->NowUs();
  }
  // Wait for other threads to advance mock time to end_ms.  Does not itself
  // advance time; we're monitoring the activities of those other threads, which
  // aren't going to terminate (and thus can't be monitored in line).
  virtual void WaitUntilMs(int64 end_ms) {
    ScopedMutex lock(mutex_);
    while (timer_->NowMs() < end_ms) {
      sleep_wakeup_condvar_->Wait();
    }
  }

 private:
  Timer* timer_;
  ThreadSystem::CondvarCapableMutex* mutex_;
  scoped_ptr<ThreadSystem::Condvar> sleep_wakeup_condvar_;
};

class ThreadedSchedulerBasedLockTest : public SchedulerBasedAbstractLockTest {
 public:
  // Various helper threads.  We could have done this with subclasses, but it's
  // clunky for present needs (which are very simple).
  // Grumble: C++ appears to require these methods to be public
  // if we take their address in a subclass method.

  // The default is DoNothingHelper, which just sleeps a long time and
  // terminates.  The other helper threads do not terminate (and fail if they
  // try).
  void DoNothingHelper() {
    SleepMs(kLongMs);
  }
  // Attempt to lock and spin forever
  void LockHelper() {
    while (!never_lock_.LockTimedWait(10 * kLongMs) && !done_.value()) { }
    CHECK(done_.value()) << "Should not lock!";
  }
  // Attempt to Lock with a steal and spin forever.  This used to fail.
  void LockStealHelper() {
    while (!never_lock_.LockTimedWaitStealOld(10 * kLongMs, kShortMs) &&
           !done_.value()) { }
    CHECK(done_.value()) << "Shouldn't lock!";
  }

 protected:
  typedef void (ThreadedSchedulerBasedLockTest::*HelperThreadMethod)();
  ThreadedSchedulerBasedLockTest()
      : never_lock_(&scheduler_),
        startup_condvar_(scheduler_.mutex()->NewCondvar()),
        helper_thread_(NULL),
        helper_thread_method_(
            &ThreadedSchedulerBasedLockTest::DoNothingHelper) { }
  void SleepUntilMs(int64 end_ms) {
    int64 now_ms = timer_.NowMs();
    while (now_ms < end_ms) {
      scheduler_.ProcessAlarms((end_ms - now_ms) * 1000);
      now_ms = timer_.NowMs();
    }
  }
  void SleepMs(int64 sleep_ms) {
    AbstractMutex* mutex = scheduler_.mutex();
    ScopedMutex lock(mutex);
    int64 now_ms = timer_.NowMs();
    SleepUntilMs(now_ms + sleep_ms);
  }
  // Start helper, then sleep for sleep_ms and return.
  void SleepForHelper(int64 sleep_ms) {
    AbstractMutex* mutex = scheduler_.mutex();
    int64 now_ms;
    {
      ScopedMutex lock(mutex);
      now_ms = timer_.NowMs();
    }
    StartHelper();
    {
      ScopedMutex lock(mutex);
      SleepUntilMs(now_ms + sleep_ms);
    }
  }
  void StartHelper() {
    helper_thread_.reset(
        new ThreadedSchedulerBasedLockTest::HelperThread(this));
    helper_thread_->Start();
    {
      ScopedMutex lock(scheduler_.mutex());
      while (!ready_to_start_.value()) {
        startup_condvar_->Wait();
      }
      ready_to_start_.set_value(false);
      startup_condvar_->Signal();
    }
  }
  void FinishHelper() {
    helper_thread_->Join();
  }
  // If the helper thread runs forever, we need to cancel it so that
  // we can safely destruct the test objects before exit.
  void CancelHelper() {
    done_.set_value(true);
    FinishHelper();
  }
  void set_helper(HelperThreadMethod helper) {
    helper_thread_method_ = helper;
  }

  NeverLock never_lock_;

 private:
  class HelperThread : public ThreadSystem::Thread {
   public:
    explicit HelperThread(ThreadedSchedulerBasedLockTest* test)
        : ThreadSystem::Thread(test->thread_system_.get(),
                               ThreadSystem::kJoinable),
          test_(test) { }
    virtual void Run() {
      {
        ScopedMutex lock(test_->scheduler_.mutex());
        test_->ready_to_start_.set_value(true);
        test_->startup_condvar_->Signal();
        while (test_->ready_to_start_.value()) {
          test_->startup_condvar_->Wait();
        }
      }
      (test_->*(test_->helper_thread_method_))();
    }
   private:
    ThreadedSchedulerBasedLockTest* test_;
    DISALLOW_COPY_AND_ASSIGN(HelperThread);
  };

  AtomicBool ready_to_start_;
  AtomicBool done_;
  scoped_ptr<ThreadSystem::Condvar> startup_condvar_;
  scoped_ptr<HelperThread> helper_thread_;
  HelperThreadMethod helper_thread_method_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedSchedulerBasedLockTest);
};

// Meta-Test that all is well.
TEST_F(ThreadedSchedulerBasedLockTest, TestStartupHandshake) {
  SleepForHelper(kShortMs);
  FinishHelper();
}

TEST_F(ThreadedSchedulerBasedLockTest, TestLockBlock) {
  set_helper(&ThreadedSchedulerBasedLockTest::LockHelper);
  SleepForHelper(kLongMs);
  CancelHelper();
}

TEST_F(ThreadedSchedulerBasedLockTest, TestLockStealBlock) {
  set_helper(&ThreadedSchedulerBasedLockTest::LockStealHelper);
  SleepForHelper(kLongMs);
  CancelHelper();
}

}  // namespace

}  // namespace net_instaweb
