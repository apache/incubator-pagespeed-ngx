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

// Author: lsong@google.com (Libo Song)

// Unit-test the file cache
#include "pagespeed/kernel/cache/file_cache.h"

#include <unistd.h>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/file_system_test_base.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/thread/slow_worker.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"

namespace net_instaweb {

class FileCacheTest : public CacheTestBase {
 protected:
  FileCacheTest()
      : thread_system_(Platform::CreateThreadSystem()),
        worker_("cleaner", thread_system_.get()),
        mock_timer_(thread_system_->NewMutex(), 0),
        file_system_(thread_system_.get(), &mock_timer_),
        kCleanIntervalMs(Timer::kMinuteMs),
        kTargetSize(12),  // Small enough to overflow with a few strings.
        kTargetInodeLimit(10),
        stats_(thread_system_.get()),
        lock_timeout_ms_(FileCache::kLockTimeoutMs) {
    FileCache::InitStats(&stats_);
    ResetFileCache(kCleanIntervalMs, kTargetSize);
    disk_checks_ = stats_.GetVariable(FileCache::kDiskChecks);
    cleanups_ = stats_.GetVariable(FileCache::kCleanups);
    evictions_ = stats_.GetVariable(FileCache::kEvictions);
    skipped_cleanups_ = stats_.GetVariable(FileCache::kSkippedCleanups);
    started_cleanups_ = stats_.GetVariable(FileCache::kStartedCleanups);
    bytes_freed_in_cleanup_ = stats_.GetVariable(
        FileCache::kBytesFreedInCleanup);

    // TODO(jmarantz): consider using mock_thread_system if we want
    // explicit control of time.
    file_system_.set_advance_time_on_update(true, &mock_timer_);
  }

  void ResetFileCache(int64 clean_interval_ms, int64 target_size_bytes) {
    cache_.reset(new FileCache(
        GTestTempDir(), &file_system_, thread_system_.get(), &worker_,
        new FileCache::CachePolicy(&mock_timer_, &hasher_, clean_interval_ms,
                                   target_size_bytes, kTargetInodeLimit),
        &stats_, &message_handler_));
  }

  void CheckCleanTimestamp(int64 min_time_ms) {
    GoogleString buffer;
    file_system_.ReadFile(cache_->clean_time_path_.c_str(), &buffer,
                           &message_handler_);
    int64 clean_time_ms;
    StringToInt64(buffer, &clean_time_ms);
    EXPECT_LT(min_time_ms, clean_time_ms);
  }

  virtual void SetUp() {
    worker_.Start();
    file_system_.Clear();
    file_system_.set_atime_enabled(true);
  }

  virtual CacheInterface* Cache() { return cache_.get(); }
  virtual void PostOpCleanup() { }

  bool Clean(int64 size, int64 inode_count) {
    // Cache expects to be locked when cleaning.
    EXPECT_TRUE(file_system_.TryLock(
        cache_->clean_lock_path_, &message_handler_).is_true());

    bool success = cache_->Clean(size, inode_count);

    EXPECT_TRUE(file_system_.Unlock(
        cache_->clean_lock_path_, &message_handler_));

    return success;
  }

  void WaitForWorker(SlowWorker* worker) {
    while (worker->IsBusy()) {
      usleep(10);
    }
  }

  void RunClean() {
    cache_->CleanIfNeeded();
    WaitForWorker(&worker_);
  }

  class StallingNotifier : public FileSystem::ProgressNotifier {
   public:
    StallingNotifier(ThreadSystem* thread_system) :
        stall_on_next_use_(false),
        stall_(thread_system),
        resume_(thread_system) {}

    void Notify() override {
      if (stall_on_next_use_) {
        stall_on_next_use_ = false;
        resume_.Notify();
        stall_.Wait();
      }
    }

    void StallOnNextUse() {
      CHECK(!stall_on_next_use_);
      stall_on_next_use_ = true;
    }

    void WaitUntilStall() {
      resume_.Wait();
    }

    void Unstall() {
      stall_.Notify();
    }

   private:
    bool stall_on_next_use_;
    WorkerTestBase::SyncPoint stall_;
    WorkerTestBase::SyncPoint resume_;
  };

  void SetNotifier(FileSystem::ProgressNotifier* notifier) {
    cache_->notifier_for_tests_ = notifier;
  }

  void BumpLock() {
    file_system_.BumpLockTimeout(cache_->clean_lock_path_.c_str(),
                                 &message_handler_);
  }

 protected:
  scoped_ptr<ThreadSystem> thread_system_;
  MD5Hasher hasher_;
  SlowWorker worker_;
  MockTimer mock_timer_;
  MemFileSystem file_system_;
  const int64 kCleanIntervalMs;
  const int64 kTargetSize;
  const int64 kTargetInodeLimit;
  SimpleStats stats_;
  scoped_ptr<FileCache> cache_;
  GoogleMessageHandler message_handler_;
  const int64 lock_timeout_ms_;

  Variable* disk_checks_;
  Variable* cleanups_;
  Variable* evictions_;
  Variable* skipped_cleanups_;
  Variable* started_cleanups_;
  Variable* bytes_freed_in_cleanup_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileCacheTest);
};

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(FileCacheTest, PutGetDelete) {
  CheckPut("Name", "Value");
  CheckGet("Name", "Value");
  CheckNotFound("Another Name");

  CheckPut("Name", "NewValue");
  CheckGet("Name", "NewValue");

  cache_->Delete("Name");
  CheckNotFound("Name");
}

// Throw a bunch of files into the cache and verify that they are
// evicted sensibly.
TEST_F(FileCacheTest, Clean) {
  // Make some "directory" entries so that the mem_file_system recurses
  // correctly.
  GoogleString dir1 = GTestTempDir() + "/a/";
  GoogleString dir2 = GTestTempDir() + "/b/";
  GoogleString dir3 = GTestTempDir() + "/b/c/";
  EXPECT_TRUE(file_system_.MakeDir(dir1.c_str(), &message_handler_));
  EXPECT_TRUE(file_system_.Exists(dir1.c_str(), &message_handler_).is_true());
  EXPECT_TRUE(file_system_.MakeDir(dir2.c_str(), &message_handler_));
  EXPECT_TRUE(file_system_.Exists(dir2.c_str(), &message_handler_).is_true());
  EXPECT_TRUE(file_system_.MakeDir(dir3.c_str(), &message_handler_));
  EXPECT_TRUE(file_system_.Exists(dir3.c_str(), &message_handler_).is_true());
  // Commonly-used keys
  const char* names1[] = {"a1", "a2", "a/3"};
  const char* values1[] = {"a2", "a234", "a2345678"};
  // Less common keys
  const char* names2[] = {"b/1", "b2", "b3",
                          "b4", "b5", "b6",
                          "b7", "b8", "b9"};
  const char* values2[] = {"b2", "b234", "b2345678",
                           "b2", "b234", "b2345678",
                           "b2", "b234", "b2345678"};
  for (int i = 0; i < 3; i++) {
    CheckPut(names1[i], values1[i]);
  }
  for (int i = 0; i < 9; i++) {
    CheckPut(names2[i], values2[i]);
  }

  FileSystem::DirInfo dir_info;
  file_system_.GetDirInfo(GTestTempDir(), &dir_info, &message_handler_);
  EXPECT_EQ((2 + 4 + 8) * 4, dir_info.size_bytes);
  EXPECT_EQ(15, dir_info.inode_count);

  // Clean should not remove anything if target is bigger than total size.
  EXPECT_TRUE(Clean(dir_info.size_bytes + 1, dir_info.inode_count + 1));
  EXPECT_EQ(1, disk_checks_->Get());
  EXPECT_EQ(0, cleanups_->Get());
  EXPECT_EQ(0, evictions_->Get());
  EXPECT_EQ(0, bytes_freed_in_cleanup_->Get());

  for (int i = 0; i < 27; i++) {
    // This pattern represents more common usage of the names1 files.
    CheckGet(names1[i % 3], values1[i % 3]);
    CheckGet(names2[i % 9], values2[i % 9]);
  }

  file_system_.GetDirInfo(GTestTempDir(), &dir_info, &message_handler_);
  EXPECT_EQ((2 + 4 + 8) * 4, dir_info.size_bytes);
  EXPECT_EQ(15, dir_info.inode_count);

  stats_.Clear();
  // Verify that inode_count_target of 0 (meaning no inode limit) is respected.
  EXPECT_TRUE(Clean(dir_info.size_bytes + 1, 0));
  EXPECT_EQ(1, disk_checks_->Get());
  EXPECT_EQ(0, cleanups_->Get());
  file_system_.GetDirInfo(GTestTempDir(), &dir_info, &message_handler_);
  EXPECT_EQ((2 + 4 + 8) * 4, dir_info.size_bytes);
  EXPECT_EQ(15, dir_info.inode_count);

  stats_.Clear();
  // Test cleaning by target_size, not inode_count
  int64 target_size = dir_info.size_bytes;
  int64 target_inode_count = dir_info.inode_count + 1;
  EXPECT_TRUE(Clean(target_size, target_inode_count));
  EXPECT_EQ(1, disk_checks_->Get());
  EXPECT_EQ(1, cleanups_->Get());
  EXPECT_EQ(3, evictions_->Get());  // Directory is not counted.
  EXPECT_EQ(2 + 4 + 8, bytes_freed_in_cleanup_->Get());
  // b/c/, b/1, b2, b3 should be removed
  for (int i = 0; i < 3; i++) {
    CheckGet(names1[i], values1[i]);
    CheckNotFound(names2[i]);
    CheckGet(names2[i+3], values2[i+3]);
    CheckGet(names2[i+6], values2[i+6]);
  }

  file_system_.GetDirInfo(GTestTempDir(), &dir_info, &message_handler_);
  EXPECT_EQ((2 + 4 + 8) * 3, dir_info.size_bytes);
  EXPECT_EQ(11, dir_info.inode_count);

  // Test that empty directories get removed, non-empty directories stay
  EXPECT_TRUE(file_system_.Exists(dir1.c_str(), &message_handler_).is_true());
  EXPECT_TRUE(file_system_.Exists(dir2.c_str(), &message_handler_).is_true());
  EXPECT_TRUE(file_system_.Exists(dir3.c_str(), &message_handler_).is_false());

  stats_.Clear();
  // Test cleaning by inode_count, not target_size
  target_size = dir_info.size_bytes + 1;
  target_inode_count = dir_info.inode_count;
  EXPECT_TRUE(Clean(target_size, target_inode_count));
  EXPECT_EQ(1, disk_checks_->Get());
  EXPECT_EQ(1, cleanups_->Get());
  EXPECT_EQ(4, evictions_->Get());
  EXPECT_EQ(2 + 2 + 2 + 4, bytes_freed_in_cleanup_->Get());
  // b/, b4, b7, a1, a2 should be removed
  for (int i = 0; i < 2; i++) {
    CheckNotFound(names1[i]);
    CheckNotFound(names2[i]);
    CheckNotFound(names2[i + 2]);
    CheckGet(names2[i + 4], values2[i + 4]);
    CheckGet(names2[i + 7], values2[i + 7]);
  }
  CheckGet(names1[2], values1[2]);
  CheckNotFound(names2[6]);

  EXPECT_TRUE(file_system_.Exists(dir1.c_str(), &message_handler_).is_true());
  EXPECT_TRUE(file_system_.Exists(dir2.c_str(), &message_handler_).is_false());
  EXPECT_TRUE(file_system_.Exists(dir3.c_str(), &message_handler_).is_false());
  file_system_.GetDirInfo(GTestTempDir(), &dir_info, &message_handler_);
  EXPECT_EQ((4 + 8) * 2 + 8, dir_info.size_bytes);
  EXPECT_EQ(6, dir_info.inode_count);
}

// Test that Clean properly calls the notifier.
TEST_F(FileCacheTest, CheckCleanNotifier) {
  CheckPut("Name1", "Value1");
  CheckPut("Name2", "Value2");
  CheckPut("Name3", "Value3");
  CountingProgressNotifier notifier;
  SetNotifier(&notifier);
  EXPECT_TRUE(Clean(0 /* delete everything */, 0));

  int expected_notifications = (
      1 /* for getDirInfo considering the parent directory */ +
      3 /* for getDirInfo considering the three entries */ +
      3 /* for deleting the three entries */);

  EXPECT_EQ(expected_notifications, notifier.get_count());
  CheckNotFound("Name1");
  CheckNotFound("Name2");
  CheckNotFound("Name3");
}

// Test the auto-cleaning behavior
TEST_F(FileCacheTest, CheckClean) {
  CheckPut("Name1", "Value");
  // Cache should not clean at first.
  RunClean();
  mock_timer_.SleepMs(kCleanIntervalMs + 1);
  // Because there's no timestamp, the cache should be cleaned.
  int64 time_ms = mock_timer_.NowUs() / 1000;
  RunClean();
  // .. but since we're under the desired size, nothing should be removed.
  CheckGet("Name1", "Value");
  // Check that the timestamp was written correctly.
  CheckCleanTimestamp(time_ms);

  // Make the cache oversize
  CheckPut("Name2", "Value2");
  CheckPut("Name3", "Value3");
  // Not enough time has elapsed.
  RunClean();
  mock_timer_.SleepMs(kCleanIntervalMs + 1);
  // Now we should clean.  This should work even if atime doesn't work as we
  // expect.
  file_system_.set_atime_enabled(false);
  time_ms = mock_timer_.NowUs() / 1000;
  RunClean();
  // And the timestamp should be updated.
  CheckCleanTimestamp(time_ms);
}

// Test cleaning doesn't mean deleting everything in the cache.
TEST_F(FileCacheTest, CheckPartialClean) {
  // Set the cache capacity to be big enough to hold two entries.  We clean down
  // to something like 75% full, though, so after cleaning only one entry will
  // be left.
  const int target_size = StrCat("Name1", "Value1",
                                 "Name2", "Value2").length();
  ResetFileCache(kCleanIntervalMs, target_size);

  CheckPut("Name1", "Value1");
  CheckPut("Name2", "Value2");
  // Advance time to make sure we clean the old ones first.
  mock_timer_.SleepMs(1);
  CheckPut("Name3", "Value3");
  mock_timer_.SleepMs(kCleanIntervalMs + 1);

  // The cache is now oversize.  Run clean.  It deletes entries 1 and 2.
  RunClean();

  CheckNotFound("Name1");  // cleaned
  CheckNotFound("Name2");  // cleaned
  CheckGet("Name3", "Value3");
}

// Test that if we disable cleaning then cleaning doesn't happen.  This is the
// same as CheckPartialClean, with cleaning disabled.
TEST_F(FileCacheTest, CheckPartialCleanWithCleaningDisabled) {
  // Set the cache capacity to be big enough to hold two entries.  We clean down
  // to something like 75% full, though, so after cleaning only one entry will
  // be left.
  const int target_size = StrCat("Name1", "Value1",
                                 "Name2", "Value2").length();
  ResetFileCache(FileCache::kDisableCleaning, target_size);

  CheckPut("Name1", "Value1");
  CheckPut("Name2", "Value2");
  // Advance time for consistency with the yes-clean version above.
  mock_timer_.SleepMs(1);
  CheckPut("Name3", "Value3");
  mock_timer_.SleepMs(kCleanIntervalMs + 1);

  // The cache is now oversize, but we won't clean because that's turned off.
  RunClean();

  // Everything is still there.
  CheckGet("Name1", "Value1");
  CheckGet("Name2", "Value2");
  CheckGet("Name3", "Value3");
}

// Test that if we start a cache cleaning run that takes longer than the
// cleaning interval and then start another run, that the second run quits
// immediately.
TEST_F(FileCacheTest, MultipleSimultaneousCacheCleans) {
  StallingNotifier notifier1(thread_system_.get());
  SetNotifier(&notifier1);

  // This won't trigger cleaning, because there's no timestamp.  Sets up the
  // timestamp for future Put()s, though.
  CheckPut("Name", "Value");
  // We still have to wait before checking the stat, though, in case we're
  // wrong.
  WaitForWorker(&worker_);
  EXPECT_EQ(0, started_cleanups_->Get());
  EXPECT_EQ(0, skipped_cleanups_->Get());

  // The second put will trigger cleaning, which will walk the fs and decide
  // nothing needs cleaning.
  mock_timer_.SleepMs(kCleanIntervalMs + 1);
  CheckPut("Name", "Value");
  WaitForWorker(&worker_);

  EXPECT_EQ(1, started_cleanups_->Get());
  EXPECT_EQ(0, skipped_cleanups_->Get());  // Still nothing skipped.
  stats_.Clear();

  // Trigger cleaning again, but this time stall it first.
  notifier1.StallOnNextUse();
  mock_timer_.SleepMs(kCleanIntervalMs + 1);
  CheckPut("Name", "Value");
  notifier1.WaitUntilStall();

  // Now make a new cache pointing to the same file system, representing a
  // different machine using the same file cache.  They do use the same stats
  // though, for testing convenience.  This cache has a much smaller target
  // size, so always deletes everything if it cleans succesfully.
  SlowWorker worker2("cleaner2", thread_system_.get());
  FileCache cache2(
      GTestTempDir(), &file_system_, thread_system_.get(),
      &worker2, new FileCache::CachePolicy(
          &mock_timer_, &hasher_, kCleanIntervalMs,
          1 /* target_size, very small */, kTargetInodeLimit),
      &stats_, &message_handler_);

  // First cleaning is still stalled.  Advance time, bump the lock, trigger the
  // second cache to try cleaning, see it fail to take the lock and abort the
  // clean.
  mock_timer_.SleepMs(kCleanIntervalMs + 1);
  BumpLock();
  CheckPut(&cache2, "Name", "Value");
  WaitForWorker(&worker2);
  EXPECT_EQ(1, skipped_cleanups_->Get());

  // Unstall the first cache, let cleaning finish.
  notifier1.Unstall();
  WaitForWorker(&worker_);

  // This cleanup also got as far as scanning the FS, but still didn't delete
  // anything, because the cache isn't oversized.
  EXPECT_EQ(1, started_cleanups_->Get());
  EXPECT_EQ(0, cleanups_->Get());
  stats_.Clear();

  CheckGet("Name", "Value");
  CheckGet(&cache2, "Name", "Value");

  // Now test what happens when we break the lock.
  StallingNotifier notifier2(thread_system_.get());
  SetNotifier(&notifier2);
  notifier2.StallOnNextUse();
  CheckPut("Name", "Value");
  notifier2.WaitUntilStall();
  mock_timer_.SleepMs(kCleanIntervalMs + lock_timeout_ms_);

  // Don't bump the lock, so that the second cleaner will run.
  CheckPut(&cache2, "Name", "Value");
  WaitForWorker(&worker2);
  EXPECT_EQ(1, cleanups_->Get());  // Second cleanup actually deleted stuff.
  // Verify it's actually gone.
  CheckNotFound("Name");

  // Let the first cache cleanup finish.
  notifier2.Unstall();
  WaitForWorker(&worker_);
  EXPECT_EQ(2, started_cleanups_->Get());  // Both cleanups scanned the FS.
  EXPECT_EQ(0, skipped_cleanups_->Get());  // Neither cleanup was skipped.
  // First cleanup didn't do any deletes; second one did.
  EXPECT_EQ(1, cleanups_->Get());
}

}  // namespace net_instaweb
