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

#include "net/instaweb/util/public/timer_based_abstract_lock.h"

#include <pthread.h>

#include "base/logging.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/pthread_condvar.h"
#include "net/instaweb/util/public/pthread_mutex.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

namespace {

static const int kShortMs = 10;
static const int kLongMs = 100;

class TimerBasedLockTest : public testing::Test {
 protected:
  TimerBasedLockTest()
      : timer_(0) {
  }

  MockTimer timer_;
 private:
  DISALLOW_COPY_AND_ASSIGN(TimerBasedLockTest);
};

// A mock lock base class
class MockLockBase : public TimerBasedAbstractLock {
 public:
  explicit MockLockBase(Timer* timer) : timer_(timer) { }
  virtual ~MockLockBase() { }
  virtual Timer* timer() const { return timer_; }
  // None of the mock locks actually implement locking, so
  // unlocking is a no-op.
  virtual void Unlock() { }
 private:
  Timer* timer_;
  DISALLOW_COPY_AND_ASSIGN(MockLockBase);
};

// A mock lock that always claims locking happened
class AlwaysLock : public MockLockBase {
 public:
  explicit AlwaysLock(Timer* timer) : MockLockBase(timer) { }
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
  explicit NeverLock(Timer* timer) : MockLockBase(timer) { }
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
  explicit StealOnlyLock(Timer* timer)
      : NeverLock(timer),
        last_hold_time_ms_(timer->NowMs()) {
  }
  virtual bool TryLockStealOld(int64 timeout_ms) {
    int64 timeout_time_ms = last_hold_time_ms_ + timeout_ms;
    int64 now_ms = timer()->NowMs();
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
TEST_F(TimerBasedLockTest, AlwaysLock) {
  int64 start = timer_.NowMs();
  AlwaysLock always_lock(&timer_);
  always_lock.Lock();
  EXPECT_TRUE(always_lock.LockTimedWait(kLongMs));
  always_lock.LockStealOld(kLongMs);
  EXPECT_TRUE(always_lock.LockTimedWaitStealOld(kLongMs, kLongMs));
  // Nothing should ever have slept.
  int64 end = timer_.NowMs();
  EXPECT_EQ(0, end - start);
}

TEST_F(TimerBasedLockTest, TimeoutHappens) {
  int64 start = timer_.NowMs();
  NeverLock never_lock(&timer_);
  EXPECT_FALSE(never_lock.LockTimedWait(kShortMs));
  int64 end = timer_.NowMs();
  // At least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // But not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(TimerBasedLockTest, TimeoutHappensStealOld) {
  int64 start = timer_.NowMs();
  NeverLock never_lock(&timer_);
  EXPECT_FALSE(never_lock.LockTimedWaitStealOld(kShortMs, kLongMs));
  int64 end = timer_.NowMs();
  // Again at least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // But not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(TimerBasedLockTest, TimeoutBeforeSteal) {
  int64 start = timer_.NowMs();
  StealOnlyLock steal_only_lock(&timer_);
  EXPECT_FALSE(steal_only_lock.LockTimedWaitStealOld(kShortMs, kLongMs));
  int64 end = timer_.NowMs();
  // Again at least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // But not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(TimerBasedLockTest, Steal) {
  int64 start = timer_.NowMs();
  StealOnlyLock steal_only_lock(&timer_);
  steal_only_lock.LockStealOld(kShortMs);
  int64 end = timer_.NowMs();
  // At least kShortMs must have elapsed.
  EXPECT_LE(kShortMs, end - start);
  // But not more than twice as long.
  EXPECT_GT(2 * kShortMs, end - start);
}

TEST_F(TimerBasedLockTest, StealBeforeTimeout) {
  int64 start = timer_.NowMs();
  StealOnlyLock steal_only_lock(&timer_);
  EXPECT_TRUE(steal_only_lock.LockTimedWaitStealOld(kLongMs, kShortMs));
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
  LockedTimer(Timer* timer, PthreadMutex* mutex)
      : timer_(timer),
        mutex_(mutex),
        sleep_wakeup_condvar_(mutex) { }
  virtual ~LockedTimer() { }
  virtual void SleepUs(int64 us) {
    {
      ScopedMutex lock(mutex_);
      timer_->SleepUs(us);
      sleep_wakeup_condvar_.Signal();
    }
    // Only permit cancellation when no locks are held,
    // as we're seeing inconsistency in behavior.
    pthread_testcancel();
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
      sleep_wakeup_condvar_.Wait();
    }
  }
 private:
  Timer* timer_;
  PthreadMutex* mutex_;
  PthreadCondvar sleep_wakeup_condvar_;
};

class ThreadedTimerBasedLockTest : public TimerBasedLockTest {
 public:
  // Various helper threads.  We could have done this with subclasses, but it's
  // clunky for present needs (which are very simple).
  // Grumble: C++ appears to require these methods to be public
  // if we take their address in a subclass method.

  // The default is DoNothingHelper, which just sleeps a long time and
  // terminates.  The other helper threads do not terminate (and fail if they
  // try).
  void DoNothingHelper() {
    locked_timer_.SleepMs(kLongMs);
  }
  // Attempt to lock and spin forever
  void LockHelper() {
    never_lock_.Lock();
    CHECK(false) << "Should not lock!";
  }
  // Attempt to Lock with a steal and spin forever.  This used to fail.
  void LockStealHelper() {
    never_lock_.LockStealOld(kShortMs);
    CHECK(false) << "Should not lock!";
  }

 protected:
  typedef void (ThreadedTimerBasedLockTest::*HelperThreadMethod)();
  ThreadedTimerBasedLockTest()
      : mutex_(),
        locked_timer_(&timer_, &mutex_),
        never_lock_(&locked_timer_),
        ready_to_start_(false),
        startup_condvar_(&mutex_),
        helper_thread_method_(&ThreadedTimerBasedLockTest::DoNothingHelper) { }
  void StartHelper() {
    pthread_create(&helper_thread_, NULL, &HelperThreadEntryPoint, this);
    {
      ScopedMutex lock(&mutex_);
      while (!ready_to_start_) {
        startup_condvar_.Wait();
      }
      ready_to_start_ = false;
      startup_condvar_.Signal();
    }
  }
  void FinishHelper() {
    pthread_join(helper_thread_, NULL);
  }
  // If the helper thread runs forever, we need to cancel it so that
  // we can safely destruct the test objects before exit.
  void CancelHelper() {
    EXPECT_EQ(0, pthread_cancel(helper_thread_));
    FinishHelper();
  }
  void set_helper(HelperThreadMethod helper) {
    helper_thread_method_ = helper;
  }

  PthreadMutex mutex_;
  LockedTimer locked_timer_;
  NeverLock never_lock_;

 private:
  static void* HelperThreadEntryPoint(void* data) {
    int ignored;
    CHECK_EQ(0, pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &ignored));
    CHECK_EQ(0, pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &ignored));
    ThreadedTimerBasedLockTest* test =
        static_cast<ThreadedTimerBasedLockTest*>(data);
    test->HelperThread();
    return NULL;
  }

  // Actually run helper thread after doing necessary startup synchronization
  void HelperThread() {
    {
      ScopedMutex lock(&mutex_);
      ready_to_start_ = true;
      startup_condvar_.Signal();
      while (ready_to_start_) {
        startup_condvar_.Wait();
      }
    }
    (this->*helper_thread_method_)();
  }

  volatile bool ready_to_start_;
  PthreadCondvar startup_condvar_;
  pthread_t helper_thread_;
  HelperThreadMethod helper_thread_method_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedTimerBasedLockTest);
};

// Meta-Test that all is well.
TEST_F(ThreadedTimerBasedLockTest, TestStartupHandshake) {
  int now = locked_timer_.NowMs();
  StartHelper();
  locked_timer_.WaitUntilMs(now + kShortMs);
  FinishHelper();
}

TEST_F(ThreadedTimerBasedLockTest, TestLockBlock) {
  set_helper(&ThreadedTimerBasedLockTest::LockHelper);
  int now = locked_timer_.NowMs();
  StartHelper();
  locked_timer_.WaitUntilMs(now + kLongMs);
  CancelHelper();
}

TEST_F(ThreadedTimerBasedLockTest, TestLockStealBlock) {
  set_helper(&ThreadedTimerBasedLockTest::LockStealHelper);
  int now = locked_timer_.NowMs();
  StartHelper();
  locked_timer_.WaitUntilMs(now + kLongMs);
  CancelHelper();
}

}  // namespace

}  // namespace net_instaweb
