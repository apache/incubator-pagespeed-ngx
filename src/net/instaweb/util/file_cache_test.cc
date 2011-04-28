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
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/cache_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {
class CacheInterface;

class FileCacheTest : public CacheTestBase {
 protected:
  FileCacheTest()
      : mock_timer_(0),
        kCleanIntervalMs(Timer::kMinuteMs),
        kTargetSize(12), // Small enough to overflow with a few strings.
        cache_(GTestTempDir(), &file_system_, &filename_encoder_,
               new FileCache::CachePolicy(
                   &mock_timer_, kCleanIntervalMs, kTargetSize),
               &message_handler_) {
  }

  void CheckCleanTimestamp(int64 min_time_ms) {
    GoogleString buffer;
    file_system_.ReadFile(cache_.clean_time_path_.c_str(), &buffer,
                           &message_handler_);
    int64 clean_time_ms;
    StringToInt64(buffer,&clean_time_ms);
    EXPECT_LT(min_time_ms, clean_time_ms);
  }

  virtual void SetUp() {
    file_system_.Clear();
    file_system_.set_atime_enabled(true);
  }

  virtual CacheInterface* Cache() { return &cache_; }
  virtual void SanityCheck() { }

 protected:
  MemFileSystem file_system_;
  FilenameEncoder filename_encoder_;
  MockTimer mock_timer_;
  const int64 kCleanIntervalMs;
  const int64 kTargetSize;
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
  EXPECT_TRUE(file_system_.MakeDir(dir1.c_str(), &message_handler_));
  EXPECT_TRUE(file_system_.MakeDir(dir2.c_str(), &message_handler_));
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
  int64 total_size = 0;
  EXPECT_TRUE(
      file_system_.RecursiveDirSize(GTestTempDir(), &total_size,
                                    &message_handler_));
  EXPECT_EQ((2 + 4 + 8) * 4, total_size);

  // Clean should not remove anything if target is bigger than total size.
  EXPECT_TRUE(cache_.Clean(total_size + 1));
  for (int i = 0; i < 27; i++) {
    // This pattern represents more common usage of the names1 files.
    CheckGet(names1[i % 3], values1[i % 3]);
    CheckGet(names2[i % 9], values2[i % 9]);
  }

  // TODO(jmarantz): gcc 4.1 warns about double/int64 comparisons here,
  // but this really should be factored into a settable member var.
  int64 target_size = (4 * total_size) / 5 - 1;
  EXPECT_TRUE(cache_.Clean(target_size));
  // Common files should stay
  for (int i = 0; i < 3; i++) {
    CheckGet(names1[i], values1[i]);
  }
  // Some of the less common files should be gone
  for (int i = 0; i < 3; i++) {
    CheckNotFound(names2[i]);
  }
}

// Test the auto-cleaning behavior
TEST_F(FileCacheTest, CheckClean) {
  CheckPut("Name1", "Value1");
  // Cache should not clean at first.
  EXPECT_FALSE(cache_.CheckClean());
  mock_timer_.SleepMs(kCleanIntervalMs + 1);
  // Because there's no timestamp, the cache should be cleaned.
  int64 time_ms = mock_timer_.NowUs() / 1000;
  EXPECT_TRUE(cache_.CheckClean());
  // .. but since we're under the desired size, nothing should be removed.
  CheckGet("Name1", "Value1");
  // Check that the timestamp was written correctly.
  CheckCleanTimestamp(time_ms);

  // Make the cache oversize
  CheckPut("Name2", "Value2");
  CheckPut("Name3", "Value3");
  // Not enough time has elapsed.
  EXPECT_FALSE(cache_.CheckClean());
  mock_timer_.SleepMs(kCleanIntervalMs + 1);
  // Now we should clean.  This should work even if atime doesn't work as we
  // expect.
  file_system_.set_atime_enabled(false);
  time_ms = mock_timer_.NowUs() / 1000;
  EXPECT_TRUE(cache_.CheckClean());
  // And the timestamp should be updated.
  CheckCleanTimestamp(time_ms);
}

}  // namespace net_instaweb
