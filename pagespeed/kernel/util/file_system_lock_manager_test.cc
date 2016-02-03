/*
 * Copyright 2010 Google Inc.
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

// Author: jmaessen@google.com (Jan Maessen)

// Unit test the file_system_lock_manager using single-threaded mocks.

#include "pagespeed/kernel/util/file_system_lock_manager.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/thread/mock_scheduler.h"
#include "pagespeed/kernel/thread/scheduler_based_abstract_lock.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

const char kLock1[] = "lock1";
const char kLock2[] = "lock2";

const int64 kStealMs = 50000;
const int64 kWaitMs = 10000;

}  // namespace

class FileSystemLockManagerTest : public testing::Test {
 protected:
  FileSystemLockManagerTest()
      : thread_system_(Platform::CreateThreadSystem()),
        timer_(thread_system_->NewMutex(), 0),
        scheduler_(thread_system_.get(), &timer_),
        file_system_(thread_system_.get(), &timer_),
        manager_(&file_system_, GTestTempDir(), &scheduler_, &handler_) { }
  virtual ~FileSystemLockManagerTest() { }

  SchedulerBasedAbstractLock* MakeLock(const StringPiece& name) {
    SchedulerBasedAbstractLock* result = manager_.CreateNamedLock(name);
    CHECK(NULL != result) << "Creating lock " << name;
    EXPECT_EQ(StrCat(GTestTempDir(), "/", name), result->name());
    return result;
  }

  void AllLocksFail(SchedulerBasedAbstractLock* lock) {
    // Note: we do it in this order to make sure that the timed waits don't
    // cause the lock to time out.
    // Note also that we don't do the blocking lock operations, as they'll block
    // indefinitely here!
    EXPECT_FALSE(lock->TryLock());
    EXPECT_FALSE(lock->TryLockStealOld(kStealMs));
    EXPECT_FALSE(lock->LockTimedWaitStealOld(kWaitMs, kStealMs));
    EXPECT_FALSE(lock->LockTimedWait(kWaitMs));
  }

  MockTimer* timer() {
    return &timer_;
  }

  bool TryLock(const scoped_ptr<SchedulerBasedAbstractLock>& lock) {
    return lock->TryLock();
  }

  bool TryLockStealOld(int64 steal_ms,
                       const scoped_ptr<SchedulerBasedAbstractLock>& lock) {
    return lock->TryLockStealOld(steal_ms);
  }

  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;
  MockScheduler scheduler_;
  GoogleMessageHandler handler_;
  MemFileSystem file_system_;
  FileSystemLockManager manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSystemLockManagerTest);
};

namespace {

TEST_F(FileSystemLockManagerTest, LockUnlock) {
  scoped_ptr<SchedulerBasedAbstractLock> lock1(MakeLock(kLock1));
  // Just do pairs of matched lock / unlock, making sure
  // we can't lock while the lock is held.
  EXPECT_TRUE(TryLock(lock1));
  EXPECT_TRUE(lock1->Held());
  AllLocksFail(lock1.get());

  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());

  EXPECT_TRUE(TryLock(lock1));
  EXPECT_TRUE(lock1->Held());
  AllLocksFail(lock1.get());

  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());

  EXPECT_TRUE(lock1->LockTimedWait(kWaitMs));
  EXPECT_TRUE(lock1->Held());
  AllLocksFail(lock1.get());

  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());

  EXPECT_TRUE(TryLockStealOld(kStealMs, lock1));
  EXPECT_TRUE(lock1->Held());
  AllLocksFail(lock1.get());

  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());

  EXPECT_TRUE(lock1->LockTimedWaitStealOld(kWaitMs, kStealMs));
  EXPECT_TRUE(lock1->Held());
  AllLocksFail(lock1.get());

  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
}

TEST_F(FileSystemLockManagerTest, DoubleLockUnlock) {
  scoped_ptr<SchedulerBasedAbstractLock> lock1(MakeLock(kLock1));
  scoped_ptr<SchedulerBasedAbstractLock> lock11(MakeLock(kLock1));
  // Just do pairs of matched lock / unlock, but make sure
  // we hold a separate lock object with the same lock name.
  EXPECT_TRUE(TryLock(lock1));
  EXPECT_TRUE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
  AllLocksFail(lock11.get());
  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
  EXPECT_FALSE(lock11->Held());

  EXPECT_TRUE(TryLock(lock1));
  AllLocksFail(lock11.get());
  EXPECT_TRUE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
  EXPECT_FALSE(lock11->Held());

  EXPECT_TRUE(lock1->LockTimedWait(kWaitMs));
  EXPECT_TRUE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
  AllLocksFail(lock11.get());
  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
  EXPECT_FALSE(lock11->Held());

  EXPECT_TRUE(TryLockStealOld(kStealMs, lock1));
  EXPECT_TRUE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
  AllLocksFail(lock11.get());
  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
  EXPECT_FALSE(lock11->Held());

  EXPECT_TRUE(lock1->LockTimedWaitStealOld(kWaitMs, kStealMs));
  EXPECT_TRUE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
  AllLocksFail(lock11.get());
  lock1->Unlock();
  EXPECT_FALSE(lock1->Held());
  EXPECT_FALSE(lock11->Held());
}

// From this point, we assume all the locking routines hold
// the lock in equivalent ways.  Now we just need to check that
// their timeout behaviors are correct.

TEST_F(FileSystemLockManagerTest, UnlockOnDestruct) {
  scoped_ptr<SchedulerBasedAbstractLock> lock1(MakeLock(kLock1));
  {
    scoped_ptr<SchedulerBasedAbstractLock> lock11(MakeLock(kLock1));
    EXPECT_TRUE(TryLock(lock11));
    EXPECT_FALSE(TryLock(lock1));
    // Should implicitly unlock on lock11 destructor call.
  }
  EXPECT_TRUE(TryLock(lock1));
}

TEST_F(FileSystemLockManagerTest, LockIndependence) {
  // Differently-named locks are different.
  scoped_ptr<SchedulerBasedAbstractLock> lock1(MakeLock(kLock1));
  scoped_ptr<SchedulerBasedAbstractLock> lock2(MakeLock(kLock2));
  EXPECT_TRUE(TryLock(lock1));
  EXPECT_TRUE(TryLock(lock2));
  EXPECT_FALSE(TryLock(lock1));
  EXPECT_FALSE(TryLock(lock2));
  lock2->Unlock();
  EXPECT_FALSE(TryLock(lock1));
  EXPECT_TRUE(TryLock(lock2));
}

TEST_F(FileSystemLockManagerTest, TimeoutFail) {
  scoped_ptr<SchedulerBasedAbstractLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(TryLock(lock1));
  EXPECT_TRUE(lock1->Held());
  int64 start_ms = timer()->NowMs();
  EXPECT_FALSE(lock1->LockTimedWait(kWaitMs));
  EXPECT_TRUE(lock1->Held());  // was never unlocked...
  int64 end_ms = timer()->NowMs();
  EXPECT_LE(start_ms + kWaitMs, end_ms);
}

TEST_F(FileSystemLockManagerTest, StealOld) {
  scoped_ptr<SchedulerBasedAbstractLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(TryLock(lock1));
  // Now we can't steal the lock until after >kStealMs has elapsed.
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1));
  timer()->AdvanceMs(kStealMs);
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1));
  // But 1ms longer than kStealMs and we can steal the lock.
  timer()->AdvanceMs(1);
  EXPECT_TRUE(TryLockStealOld(kStealMs, lock1));
  // After steal the timer should reset.
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1));
  timer()->AdvanceMs(kStealMs);
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1));
  EXPECT_TRUE(lock1->Held());  // was never unlocked...
  // But again expire after >kStealMs elapses.
  timer()->AdvanceMs(1);
  EXPECT_TRUE(TryLockStealOld(kStealMs, lock1));
  EXPECT_TRUE(lock1->Held());  // was never unlocked...
}

TEST_F(FileSystemLockManagerTest, BlockingStealOld) {
  scoped_ptr<SchedulerBasedAbstractLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(TryLock(lock1));
  // Now a call to LockTimedWaitStealOld should block until kStealMs has
  // elapsed.
  int64 start_ms = timer()->NowMs();
  lock1->LockTimedWaitStealOld(kStealMs * 100, kStealMs);
  int64 end_ms = timer()->NowMs();
  EXPECT_LT(start_ms + kStealMs, end_ms);
  EXPECT_GT(start_ms + kStealMs * 100, end_ms);
  // Again the timer should reset after the lock is obtained.
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1));
  timer()->AdvanceMs(kStealMs);
  EXPECT_FALSE(TryLockStealOld(kStealMs, lock1));
  timer()->AdvanceMs(1);
  EXPECT_TRUE(TryLockStealOld(kStealMs, lock1));
}

TEST_F(FileSystemLockManagerTest, WaitStealOld) {
  scoped_ptr<SchedulerBasedAbstractLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(TryLock(lock1));
  int64 start_ms = timer()->NowMs();
  // If we start now, we'll time out with time to spare.
  EXPECT_FALSE(lock1->LockTimedWaitStealOld(kWaitMs, kStealMs));
  int64 end_ms = timer()->NowMs();
  EXPECT_LE(start_ms + kWaitMs, end_ms);
  EXPECT_GT(start_ms + kStealMs, end_ms);
  // Advance time so that the lock timeout is within the wait time.
  int64 time_ms = start_ms + kStealMs - kWaitMs / 2;
  timer()->SetTimeUs(1000 * time_ms);
  start_ms = timer()->NowMs();
  EXPECT_TRUE(lock1->LockTimedWaitStealOld(kWaitMs, kStealMs));
  end_ms = timer()->NowMs();
  EXPECT_GT(start_ms + kWaitMs, end_ms);
}

}  // namespace

}  // namespace net_instaweb
