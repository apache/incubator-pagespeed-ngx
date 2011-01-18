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
#include "base/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/shared_string.h"
#include <string>

namespace net_instaweb {

class FileCacheTest : public testing::Test {
 protected:
  FileCacheTest()
      : mock_timer_(0),
        kCleanIntervalMs(Timer::kMinuteMs),
        kTargetSize(409600),
        cache_(GTestTempDir(), &file_system_, &filename_encoder_,
               new FileCache::CachePolicy(
                   &mock_timer_, kCleanIntervalMs, kTargetSize),
               &message_handler_) {
  }

  void CheckGet(const char* key, const std::string& expected_value) {
    SharedString value_buffer;
    EXPECT_TRUE(cache_.Get(key, &value_buffer));
    EXPECT_EQ(expected_value, *value_buffer);
    EXPECT_EQ(CacheInterface::kAvailable, cache_.Query(key));
  }

  void Put(const char* key, const char* value) {
    SharedString put_buffer(value);
    cache_.Put(key, &put_buffer);
  }

  void CheckNotFound(const char* key) {
    SharedString value_buffer;
    EXPECT_FALSE(cache_.Get(key, &value_buffer));
    EXPECT_EQ(CacheInterface::kNotFound, cache_.Query(key));
  }

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
  Put("Name", "Value");
  CheckGet("Name", "Value");
  CheckNotFound("Another Name");

  Put("Name", "NewValue");
  CheckGet("Name", "NewValue");

  cache_.Delete("Name");
  SharedString value_buffer;
  EXPECT_FALSE(cache_.Get("Name", &value_buffer));
}

// Throw a bunch of files into the cache and verify that they are
// evicted sensibly.
TEST_F(FileCacheTest, Clean) {
  file_system_.Clear();
  // Make some "directory" entries so that the mem_file_system recurses
  // correctly.
  std::string dir1 = GTestTempDir() + "/a/";
  std::string dir2 = GTestTempDir() + "/b/";
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
    Put(names1[i], values1[i]);
  }
  for (int i = 0; i < 9; i++) {
    Put(names2[i], values2[i]);
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
// TODO(abliss): add test for CheckClean
}  // namespace net_instaweb
