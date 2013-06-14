/*
 * Copyright 2013 Google Inc.
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

#include "pagespeed/kernel/cache/purge_context.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/simple_stats.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/thread/mock_scheduler.h"
#include "pagespeed/kernel/util/file_system_lock_manager.h"
#include "pagespeed/kernel/util/platform.h"

namespace {

const int kMaxBytes = 100;
const char kPurgeFile[] = "/cache/cache.flush";
const char kBasePath[] = "/cache";

}  // namespace

namespace net_instaweb {

class PurgeContextTest : public testing::Test {
 public:
  void CorruptWrittenFileHook(const GoogleString& filename) {
    EXPECT_TRUE(file_system_.WriteFile(filename.c_str(), "bogus",
                                       &message_handler_));
  }

  void CorruptFileAndAddNewUpdate(const GoogleString& filename) {
    EXPECT_TRUE(file_system_.WriteFile(filename.c_str(), "bogus",
                                       &message_handler_));
    lock_->Unlock();
    ASSERT_TRUE(lock_->LockTimedWaitStealOld(0, 0));
    purge_context1_->AddPurgeUrl("a", 500000, ExpectSuccess());
  }

  bool PollAndTest(const GoogleString& url, int64 now_ms,
                   const CopyOnWrite<PurgeSet>& purge_set,
                   PurgeContext* purge_context) {
    purge_context->PollFileSystem();
    return purge_set->IsValid(url, now_ms);
  }

  bool PollAndTest1(const GoogleString& url, int64 now_ms) {
    return PollAndTest(url, now_ms, purge_set1_, purge_context1_.get());
  }

  bool PollAndTest2(const GoogleString& url, int64 now_ms) {
    return PollAndTest(url, now_ms, purge_set2_, purge_context2_.get());
  }

 protected:
  PurgeContextTest()
      : timer_(MockTimer::kApr_5_2010_ms),
        thread_system_(Platform::CreateThreadSystem()),
        message_handler_(thread_system_->NewMutex()),
        file_system_(thread_system_.get(), &timer_),
        scheduler_(thread_system_.get(), &timer_),
        lock_manager_(&file_system_, kBasePath, &scheduler_, &message_handler_),
        simple_stats_(thread_system_.get()) {
    PurgeContext::InitStats(&simple_stats_);
    purge_context1_.reset(MakePurgeContext());
    purge_context2_.reset(MakePurgeContext());

    purge_context1_->SetUpdateCallback(
        NewPermanentCallback(this, &PurgeContextTest::UpdatePurgeSet1));
    purge_context2_->SetUpdateCallback(
        NewPermanentCallback(this, &PurgeContextTest::UpdatePurgeSet2));

    message_handler_.AddPatternToSkipPrinting("*opening input file*");
  }

  PurgeContext* MakePurgeContext() {
    return new PurgeContext(kPurgeFile, &file_system_, &timer_,
                            kMaxBytes, thread_system_.get(), &lock_manager_,
                            &simple_stats_, &message_handler_);
  }

  GoogleString LockName() { return purge_context1_->LockName(); }

  void ExpectSuccessHelper(bool x) {
    EXPECT_TRUE(x);
  }
  PurgeContext::BoolCallback* ExpectSuccess() {
    return NewCallback(this, &PurgeContextTest::ExpectSuccessHelper);
  }

  void ExpectFailureHelper(bool x) {
    EXPECT_FALSE(x);
  }
  PurgeContext::BoolCallback* ExpectFailure() {
    return NewCallback(this, &PurgeContextTest::ExpectFailureHelper);
  }

  int64 LockContentionStart(PurgeContext::BoolCallback* callback) {
    scheduler_.AdvanceTimeMs(10 * Timer::kSecondMs);
    lock_.reset(lock_manager_.CreateNamedLock(LockName()));
    EXPECT_TRUE(lock_->LockTimedWaitStealOld(0, 0));
    EXPECT_TRUE(lock_->Held());
    int64 now_ms = timer_.NowMs();
    purge_context1_->SetCachePurgeGlobalTimestampMs(now_ms, callback);

    // We don't check pending_purges_ in PollAndTestValid; the invalidation will
    // only be visible to purge_context1 when it can acquire the lock and
    // write its records.
    EXPECT_TRUE(PollAndTest1("b", now_ms - 1));
    EXPECT_TRUE(PollAndTest2("b", now_ms - 1));

    // Advance time by a second; which is not enough to steal the lock,
    // so we still consider 'b' to be valid in both contexts.
    scheduler_.AdvanceTimeMs(1 * Timer::kSecondMs);
    EXPECT_TRUE(PollAndTest1("b", now_ms - 1));
    EXPECT_TRUE(PollAndTest2("b", now_ms - 1));
    scheduler_.AdvanceTimeMs(1 * Timer::kSecondMs);  // Not enough to steal it.
    return now_ms;
  }

  int num_cancellations() {
    return simple_stats_.GetVariable(PurgeContext::kCancellations)->Get();
  }

  int num_contentions() {
    return simple_stats_.GetVariable(PurgeContext::kContentions)->Get();
  }

  int file_parse_failures() {
    return simple_stats_.GetVariable(PurgeContext::kFileParseFailures)->Get();
  }

  void UpdatePurgeSet1(const CopyOnWrite<PurgeSet>& purge_set) {
    purge_set1_ = purge_set;
  }

  void UpdatePurgeSet2(const CopyOnWrite<PurgeSet>& purge_set) {
    purge_set2_ = purge_set;
  }

  MockTimer timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockMessageHandler message_handler_;
  MemFileSystem file_system_;
  MockScheduler scheduler_;
  FileSystemLockManager lock_manager_;
  SimpleStats simple_stats_;
  scoped_ptr<PurgeContext> purge_context1_;
  scoped_ptr<PurgeContext> purge_context2_;
  CopyOnWrite<PurgeSet> purge_set1_;
  CopyOnWrite<PurgeSet> purge_set2_;
  scoped_ptr<NamedLock> lock_;
};

TEST_F(PurgeContextTest, Empty) {
  EXPECT_TRUE(PollAndTest1("a", 500));
}

TEST_F(PurgeContextTest, InvalidationSharing) {
  scheduler_.AdvanceTimeMs(1000);
  purge_context1_->SetCachePurgeGlobalTimestampMs(400000, ExpectSuccess());
  purge_context1_->AddPurgeUrl("a", 500000, ExpectSuccess());
  EXPECT_FALSE(PollAndTest1("a", 500000));
  EXPECT_TRUE(PollAndTest1("a", 500001));
  EXPECT_FALSE(PollAndTest1("b", 399999));
  EXPECT_TRUE(PollAndTest1("b", 400000));

  // These will get transmitted to purge_context2_, which has not
  // yet read the cache invalidation file, but will pick up the
  // changes from the file system.
  EXPECT_FALSE(PollAndTest2("a", 500000));
  EXPECT_TRUE(PollAndTest2("a", 500001));
  EXPECT_FALSE(PollAndTest2("b", 399999));
  EXPECT_TRUE(PollAndTest2("b", 400000));

  // Now push a time-based flush the other direction.  Because
  // we only poll the file system periodically we do have to advance
  // time.
  purge_context2_->SetCachePurgeGlobalTimestampMs(600000, ExpectSuccess());
  EXPECT_FALSE(PollAndTest2("a", 500001));
  EXPECT_TRUE(PollAndTest1("a", 500001));
  scheduler_.AdvanceTimeMs(10 * Timer::kSecondMs);     // force poll
  EXPECT_FALSE(PollAndTest1("a", 500001));
  EXPECT_TRUE(PollAndTest1("b", 600001));
  EXPECT_FALSE(PollAndTest2("a", 500001));
  EXPECT_TRUE(PollAndTest2("b", 600001));

  // Now invalidate 'b' till 700k.
  purge_context2_->AddPurgeUrl("b", 700000, ExpectSuccess());
  EXPECT_FALSE(PollAndTest2("b", 700000));
  EXPECT_TRUE(PollAndTest1("b", 700000));
  scheduler_.AdvanceTimeMs(10 * Timer::kSecondMs);      // force poll
  EXPECT_FALSE(PollAndTest1("b", 700000));
  EXPECT_TRUE(PollAndTest1("b", 700001));
  EXPECT_FALSE(PollAndTest2("b", 700000));
  EXPECT_TRUE(PollAndTest2("b", 700001));
  EXPECT_EQ(0, file_parse_failures());
}

TEST_F(PurgeContextTest, EmptyPurgeFile) {
  // The currently documented mechanism to flush the entire cache is
  // to simply touch CACHE_DIR/cache.flush.  That should continue to
  // work as advertised.
  scheduler_.AdvanceTimeMs(10 * Timer::kSecondMs);
  ASSERT_TRUE(file_system_.WriteFile(kPurgeFile, "", &message_handler_));
  EXPECT_FALSE(PollAndTest1("b", timer_.NowMs() - 1));
  EXPECT_TRUE(PollAndTest1("b", timer_.NowMs() + 1));
  EXPECT_EQ(0, file_parse_failures());
}

TEST_F(PurgeContextTest, LockContentionFailure) {
  int64 now_ms = LockContentionStart(ExpectFailure());

  // Release & retake the lock making it harder to steal by refreshing it.
  lock_->Unlock();
  ASSERT_TRUE(lock_->LockTimedWaitStealOld(0, 0));

  // Get our ExpectFailure callback called and confirm that the invalidation
  // didn't have any effect.
  scheduler_.AdvanceTimeMs(10 * Timer::kSecondMs);
  EXPECT_TRUE(PollAndTest1("b", now_ms - 1));
  EXPECT_TRUE(PollAndTest2("b", now_ms - 1));
  EXPECT_EQ(1, num_cancellations());
  EXPECT_EQ(0, num_contentions());
  EXPECT_EQ(0, file_parse_failures());
}

TEST_F(PurgeContextTest, LockContentionSuccess) {
  int64 now_ms = LockContentionStart(ExpectSuccess());

  // Now advance time by 10 seconds; this should ensure that we steal
  // the lock and can write the invalidation records for all to see.
  scheduler_.AdvanceTimeMs(10 * Timer::kSecondMs);
  EXPECT_FALSE(PollAndTest1("b", now_ms - 1));
  EXPECT_FALSE(PollAndTest2("b", now_ms - 1));
  EXPECT_EQ(0, num_cancellations());
  EXPECT_EQ(0, num_contentions());
  EXPECT_EQ(0, file_parse_failures());
}

TEST_F(PurgeContextTest, FileWriteConflict) {
  int64 now_ms = LockContentionStart(ExpectSuccess());
  file_system_.set_write_callback(
      NewCallback(this, &PurgeContextTest::CorruptWrittenFileHook));

  // Now advance time by 10 seconds; this should ensure that we steal
  // the lock and can write the invalidation records for all to see.
  // Unfortunately the file-write will not be verified and will have
  // to grab the lock and do it again.
  scheduler_.AdvanceTimeMs(10 * Timer::kSecondMs);
  EXPECT_FALSE(PollAndTest1("b", now_ms - 1));
  EXPECT_FALSE(PollAndTest2("b", now_ms - 1));
  EXPECT_EQ(0, num_cancellations());
  EXPECT_EQ(1, num_contentions());
  EXPECT_EQ(1, file_parse_failures());
}

TEST_F(PurgeContextTest,  FileWriteConflictWithInterveningUpdate) {
  int64 now_ms = LockContentionStart(ExpectSuccess());

  file_system_.set_write_callback(
      NewCallback(this, &PurgeContextTest::CorruptFileAndAddNewUpdate));

  // Now advance time by 10 seconds; this should ensure that we steal
  // the lock and can write the invalidation records for all to see.
  // Unfortunately the file-write will not be verified and will have
  // to grab the lock and do it again.
  scheduler_.AdvanceTimeMs(10 * Timer::kSecondMs);
  EXPECT_FALSE(PollAndTest1("b", now_ms - 1));
  EXPECT_FALSE(PollAndTest2("b", now_ms - 1));
  EXPECT_EQ(0, num_cancellations());
  EXPECT_EQ(1, num_contentions());
  EXPECT_EQ(1, file_parse_failures());
}

TEST_F(PurgeContextTest, InvalidTimestampInPurgeRecord) {
  ASSERT_TRUE(file_system_.WriteFile(
      kPurgeFile,
      "-1\n"                // timestamp in past
      "x\n"                 // not enough tokens
      "2000000000000 y\n"   // timestamp(ms) in far future
      "500 a\n",            // valid record should be parsed.
      &message_handler_));
  EXPECT_FALSE(PollAndTest1("a", 500));
  EXPECT_TRUE(PollAndTest1("a", 501));
  EXPECT_EQ(3, file_parse_failures());
}

}  // namespace net_instaweb
