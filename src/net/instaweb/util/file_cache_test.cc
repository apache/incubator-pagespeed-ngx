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
#include "net/instaweb/util/public/file_cache.h"

#include <unistd.h>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class FileCacheTest : public CacheTestBase {
 protected:
  FileCacheTest()
      : thread_system_(ThreadSystem::CreateThreadSystem()),
        worker_(thread_system_.get()),
        mock_timer_(0),
        file_system_(thread_system_.get(), &mock_timer_),
        kCleanIntervalMs(Timer::kMinuteMs),
        kTargetSize(12),  // Small enough to overflow with a few strings.
        kTargetInodeLimit(10),
        cache_(GTestTempDir(), &file_system_, &worker_,
               &filename_encoder_,
               new FileCache::CachePolicy(
                   &mock_timer_, &hasher_, kCleanIntervalMs, kTargetSize,
                   kTargetInodeLimit),
               &message_handler_) {
    // TODO(jmarantz): consider using mock_thread_system if we want
    // explicit control of time.  For now, just mutex-protect the
    // MockTimer.
    mock_timer_.set_mutex(thread_system_->NewMutex());
    file_system_.set_advance_time_on_update(true, &mock_timer_);
  }

  void CheckCleanTimestamp(int64 min_time_ms) {
    GoogleString buffer;
    file_system_.ReadFile(cache_.clean_time_path_.c_str(), &buffer,
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

  virtual CacheInterface* Cache() { return &cache_; }
  virtual void SanityCheck() { }

  bool Clean(int64 size, int64 inode_count) {
    return cache_.Clean(size, inode_count);
  }

  bool CheckClean() {
    cache_.CleanIfNeeded();
    while (worker_.IsBusy()) {
      usleep(10);
    }
    return cache_.last_conditional_clean_result_;
  }

 protected:
  scoped_ptr<ThreadSystem> thread_system_;
  MD5Hasher hasher_;
  SlowWorker worker_;
  MockTimer mock_timer_;
  MemFileSystem file_system_;
  FilenameEncoder filename_encoder_;
  const int64 kCleanIntervalMs;
  const int64 kTargetSize;
  const int64 kTargetInodeLimit;
  FileCache cache_;
  GoogleMessageHandler message_handler_;

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

  cache_.Delete("Name");
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
  const char* names2[] =
      {"b/1", "b2", "b3", "b4", "b5", "b6", "b7", "b8", "b9"};
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
  for (int i = 0; i < 27; i++) {
    // This pattern represents more common usage of the names1 files.
    CheckGet(names1[i % 3], values1[i % 3]);
    CheckGet(names2[i % 9], values2[i % 9]);
  }

  file_system_.GetDirInfo(GTestTempDir(), &dir_info, &message_handler_);
  EXPECT_EQ((2 + 4 + 8) * 4, dir_info.size_bytes);
  EXPECT_EQ(15, dir_info.inode_count);

  // Verify that inode_count_target of 0 (meaning no inode limit) is respected.
  EXPECT_TRUE(Clean(dir_info.size_bytes + 1, 0));
  file_system_.GetDirInfo(GTestTempDir(), &dir_info, &message_handler_);
  EXPECT_EQ((2 + 4 + 8) * 4, dir_info.size_bytes);
  EXPECT_EQ(15, dir_info.inode_count);

  // Test cleaning by target_size, not inode_count
  // TODO(jmarantz): gcc 4.1 warns about double/int64 comparisons here,
  // but this really should be factored into a settable member var.
  int64 target_size = (4 * dir_info.size_bytes) / 5 - 1;
  int64 target_inode_count = dir_info.inode_count * 2;
  EXPECT_TRUE(Clean(target_size, target_inode_count));
  // b/c/, b/1, b2, b3, b4, b5, b6 should be removed
  for (int i = 0; i < 3; i++) {
    CheckGet(names1[i], values1[i]);
    CheckGet(names2[i+6], values2[i+6]);
  }
  for (int i = 0; i < 6; i++) {
    CheckNotFound(names2[i]);
  }
  file_system_.GetDirInfo(GTestTempDir(), &dir_info, &message_handler_);
  EXPECT_EQ((2 + 4 + 8) * 2, dir_info.size_bytes);
  EXPECT_EQ(8, dir_info.inode_count);

  // Test that empty directories get removed, non-empty directories stay
  EXPECT_TRUE(file_system_.Exists(dir1.c_str(), &message_handler_).is_true());
  EXPECT_TRUE(file_system_.Exists(dir2.c_str(), &message_handler_).is_true());
  EXPECT_TRUE(file_system_.Exists(dir3.c_str(), &message_handler_).is_false());

  // Test cleaning by inode_count, not target_size
  target_size = dir_info.size_bytes * 2;
  target_inode_count = (4 * dir_info.inode_count) / 5 - 1;
  EXPECT_TRUE(Clean(target_size, target_inode_count));
  // b/, b8, a2, b7, a1 should be removed
  for (int i = 0; i < 2; i++) {
    CheckNotFound(names1[i]);
  }
  for (int i = 0; i < 8; i++) {
    CheckNotFound(names2[i]);
  }
  CheckGet(names1[2], values1[2]);
  CheckGet(names2[8], values2[8]);
  EXPECT_TRUE(file_system_.Exists(dir1.c_str(), &message_handler_).is_true());
  EXPECT_TRUE(file_system_.Exists(dir2.c_str(), &message_handler_).is_false());
  EXPECT_TRUE(file_system_.Exists(dir3.c_str(), &message_handler_).is_false());
  file_system_.GetDirInfo(GTestTempDir(), &dir_info, &message_handler_);
  EXPECT_EQ(8 * 2, dir_info.size_bytes);
  EXPECT_EQ(3, dir_info.inode_count);
}

// Test the auto-cleaning behavior
TEST_F(FileCacheTest, CheckClean) {
  CheckPut("Name1", "Value1");
  // Cache should not clean at first.
  EXPECT_FALSE(CheckClean());
  mock_timer_.SleepMs(kCleanIntervalMs + 1);
  // Because there's no timestamp, the cache should be cleaned.
  int64 time_ms = mock_timer_.NowUs() / 1000;
  EXPECT_TRUE(CheckClean());
  // .. but since we're under the desired size, nothing should be removed.
  CheckGet("Name1", "Value1");
  // Check that the timestamp was written correctly.
  CheckCleanTimestamp(time_ms);

  // Make the cache oversize
  CheckPut("Name2", "Value2");
  CheckPut("Name3", "Value3");
  // Not enough time has elapsed.
  EXPECT_FALSE(CheckClean());
  mock_timer_.SleepMs(kCleanIntervalMs + 1);
  // Now we should clean.  This should work even if atime doesn't work as we
  // expect.
  file_system_.set_atime_enabled(false);
  time_ms = mock_timer_.NowUs() / 1000;
  EXPECT_TRUE(CheckClean());
  // And the timestamp should be updated.
  CheckCleanTimestamp(time_ms);
}

}  // namespace net_instaweb
