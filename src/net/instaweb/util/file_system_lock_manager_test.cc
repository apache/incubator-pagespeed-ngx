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

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

namespace {

const char kLock1[] = "lock1";
const char kLock2[] = "lock2";

const int64 kTimeoutMs = 50000;
const int64 kWaitMs = 10000;

class FileSystemLockManagerTest : public testing::Test {
 protected:
  FileSystemLockManagerTest()
      : timer_(0),
        thread_system_(ThreadSystem::CreateThreadSystem()),
        scheduler_(thread_system_.get(), &timer_),
        file_system_(&timer_),
        manager_(&file_system_, GTestTempDir(), &scheduler_, &handler_) { }
  virtual ~FileSystemLockManagerTest() { }

  NamedLock* MakeLock(const StringPiece& name) {
    NamedLock* result = manager_.CreateNamedLock(name);
    CHECK(NULL != result) << "Creating lock " << name;
    EXPECT_EQ(StrCat(GTestTempDir(), "/", name), result->name());
    return result;
  }

  void AllLocksFail(NamedLock* lock) {
    // Note: we do it in this order to make sure that the timed waits don't
    // cause the lock to time out.
    // Note also that we don't do the blocking lock operations, as they'll block
    // indefinitely here!
    EXPECT_FALSE(lock->TryLock());
    EXPECT_FALSE(lock->TryLockStealOld(kTimeoutMs));
    EXPECT_FALSE(lock->LockTimedWaitStealOld(kWaitMs, kTimeoutMs));
    EXPECT_FALSE(lock->LockTimedWait(kWaitMs));
  }

  MockTimer* timer() {
    return &timer_;
  }

  MockTimer timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockScheduler scheduler_;
  GoogleMessageHandler handler_;
  MemFileSystem file_system_;
  FileSystemLockManager manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSystemLockManagerTest);
};

TEST_F(FileSystemLockManagerTest, LockUnlock) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  // Just do pairs of matched lock / unlock, making sure
  // we can't lock while the lock is held.
  EXPECT_TRUE(lock1->TryLock());
  AllLocksFail(lock1.get());
  lock1->Unlock();
  EXPECT_TRUE(lock1->TryLock());
  AllLocksFail(lock1.get());
  lock1->Unlock();
  EXPECT_TRUE(lock1->LockTimedWait(kWaitMs));
  AllLocksFail(lock1.get());
  lock1->Unlock();
  EXPECT_TRUE(lock1->TryLockStealOld(kTimeoutMs));
  AllLocksFail(lock1.get());
  lock1->Unlock();
  EXPECT_TRUE(lock1->LockTimedWaitStealOld(kWaitMs, kTimeoutMs));
  AllLocksFail(lock1.get());
  lock1->Unlock();
}

TEST_F(FileSystemLockManagerTest, DoubleLockUnlock) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock11(MakeLock(kLock1));
  // Just do pairs of matched lock / unlock, but make sure
  // we hold a separate lock object with the same lock name.
  EXPECT_TRUE(lock1->TryLock());
  AllLocksFail(lock11.get());
  lock1->Unlock();
  EXPECT_TRUE(lock1->TryLock());
  AllLocksFail(lock11.get());
  lock1->Unlock();
  EXPECT_TRUE(lock1->LockTimedWait(kWaitMs));
  AllLocksFail(lock11.get());
  lock1->Unlock();
  EXPECT_TRUE(lock1->TryLockStealOld(kTimeoutMs));
  AllLocksFail(lock11.get());
  lock1->Unlock();
  EXPECT_TRUE(lock1->LockTimedWaitStealOld(kWaitMs, kTimeoutMs));
  AllLocksFail(lock11.get());
  lock1->Unlock();
}

// From this point, we assume all the locking routines hold
// the lock in equivalent ways.  Now we just need to check that
// their timeout behaviors are correct.

TEST_F(FileSystemLockManagerTest, UnlockOnDestruct) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  {
    scoped_ptr<NamedLock> lock11(MakeLock(kLock1));
    EXPECT_TRUE(lock11->TryLock());
    EXPECT_FALSE(lock1->TryLock());
    // Should implicitly unlock on lock11 destructor call.
  }
  EXPECT_TRUE(lock1->TryLock());
}

TEST_F(FileSystemLockManagerTest, LockIndependence) {
  // Differently-named locks are different.
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  scoped_ptr<NamedLock> lock2(MakeLock(kLock2));
  EXPECT_TRUE(lock1->TryLock());
  EXPECT_TRUE(lock2->TryLock());
  EXPECT_FALSE(lock1->TryLock());
  EXPECT_FALSE(lock2->TryLock());
  lock2->Unlock();
  EXPECT_FALSE(lock1->TryLock());
  EXPECT_TRUE(lock2->TryLock());
}

TEST_F(FileSystemLockManagerTest, TimeoutFail) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(lock1->TryLock());
  int64 start_ms = timer()->NowMs();
  EXPECT_FALSE(lock1->LockTimedWait(kWaitMs));
  int64 end_ms = timer()->NowMs();
  EXPECT_LE(start_ms + kWaitMs, end_ms);
}

TEST_F(FileSystemLockManagerTest, StealOld) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(lock1->TryLock());
  // Now we can't steal the lock until after >kTimeoutMs has elapsed.
  EXPECT_FALSE(lock1->TryLockStealOld(kTimeoutMs));
  timer()->AdvanceMs(kTimeoutMs);
  EXPECT_FALSE(lock1->TryLockStealOld(kTimeoutMs));
  // But 1ms longer than kTimeoutMs and we can steal the lock.
  timer()->AdvanceMs(1);
  EXPECT_TRUE(lock1->TryLockStealOld(kTimeoutMs));
  // After steal the timer should reset.
  EXPECT_FALSE(lock1->TryLockStealOld(kTimeoutMs));
  timer()->AdvanceMs(kTimeoutMs);
  EXPECT_FALSE(lock1->TryLockStealOld(kTimeoutMs));
  // But again expire after >kTimeoutMs elapses.
  timer()->AdvanceMs(1);
  EXPECT_TRUE(lock1->TryLockStealOld(kTimeoutMs));
}

TEST_F(FileSystemLockManagerTest, BlockingStealOld) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(lock1->TryLock());
  // Now a call to LockTimedWaitStealOld should block until kTimeoutMs has
  // elapsed.
  int64 start_ms = timer()->NowMs();
  lock1->LockTimedWaitStealOld(kTimeoutMs * 100, kTimeoutMs);
  int64 end_ms = timer()->NowMs();
  EXPECT_LT(start_ms + kTimeoutMs, end_ms);
  EXPECT_GT(start_ms + kTimeoutMs * 100, end_ms);
  // Again the timer should reset after the lock is obtained.
  EXPECT_FALSE(lock1->TryLockStealOld(kTimeoutMs));
  timer()->AdvanceMs(kTimeoutMs);
  EXPECT_FALSE(lock1->TryLockStealOld(kTimeoutMs));
  timer()->AdvanceMs(1);
  EXPECT_TRUE(lock1->TryLockStealOld(kTimeoutMs));
}

TEST_F(FileSystemLockManagerTest, WaitStealOld) {
  scoped_ptr<NamedLock> lock1(MakeLock(kLock1));
  EXPECT_TRUE(lock1->TryLock());
  int64 start_ms = timer()->NowMs();
  // If we start now, we'll time out with time to spare.
  EXPECT_FALSE(lock1->LockTimedWaitStealOld(kWaitMs, kTimeoutMs));
  int64 end_ms = timer()->NowMs();
  EXPECT_LE(start_ms + kWaitMs, end_ms);
  EXPECT_GT(start_ms + kTimeoutMs, end_ms);
  // Advance time so that the lock timeout is within the wait time.
  int64 time_ms = start_ms + kTimeoutMs - kWaitMs / 2;
  timer()->SetTimeUs(1000 * time_ms);
  start_ms = timer()->NowMs();
  EXPECT_TRUE(lock1->LockTimedWaitStealOld(kWaitMs, kTimeoutMs));
  end_ms = timer()->NowMs();
  EXPECT_GT(start_ms + kWaitMs, end_ms);
}

}  // namespace

}  // namespace net_instaweb
