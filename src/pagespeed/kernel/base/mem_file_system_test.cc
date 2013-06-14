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

// Author: abliss@google.com (Adam Bliss)

// Unit-test the in-memory filesystem

#include "pagespeed/kernel/base/mem_file_system.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/file_system_test.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/null_thread_system.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

class MemFileSystemTest : public FileSystemTest {
 protected:
  MemFileSystemTest()
      : thread_system_(new NullThreadSystem),
        timer_(0),
        mem_file_system_(thread_system_.get(), &timer_) {
    mem_file_system_.set_advance_time_on_update(true, &timer_);
  }
  virtual void DeleteRecursively(const StringPiece& filename) {
    mem_file_system_.Clear();
  }
  virtual FileSystem* file_system() { return &mem_file_system_; }
  virtual Timer* timer() { return &timer_; }
  virtual GoogleString test_tmpdir() { return GTestTempDir(); }

  virtual int FileSize(StringPiece contents) const {
    return FileContentSize(contents);
  }

  virtual int DefaultDirSize() const {
    return 0;
  }

  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;
  MemFileSystem mem_file_system_;

  DISALLOW_COPY_AND_ASSIGN(MemFileSystemTest);
};

// Write a named file, then read it.
TEST_F(MemFileSystemTest, TestWriteRead) {
  TestWriteRead();
}

// Write a temp file, then read it.
TEST_F(MemFileSystemTest, TestTemp) {
  TestTemp();
}

// Write a temp file, close it, append to it, then read it.
TEST_F(MemFileSystemTest, TestAppend) {
  TestAppend();
}

// Write a temp file, rename it, then read it.
TEST_F(MemFileSystemTest, TestRename) {
  TestRename();
}

// Write a file and successfully delete it.
TEST_F(MemFileSystemTest, TestRemove) {
  TestRemove();
}

// Write a file and check that it exists.
TEST_F(MemFileSystemTest, TestExists) {
  TestExists();
}

// Create a file along with its directory which does not exist.
TEST_F(MemFileSystemTest, TestCreateFileInDir) {
  TestCreateFileInDir();
}


// Make a directory and check that files may be placed in it.
TEST_F(MemFileSystemTest, TestMakeDir) {
  TestMakeDir();
}

// Create a directory and verify removing it.
TEST_F(MemFileSystemTest, TestRemoveDir) {
  TestRemoveDir();
}

// We intentionally do not test TestIsDir, TestRecursivelyMakeDir*

TEST_F(MemFileSystemTest, TestListContents) {
  TestListContents();
}

TEST_F(MemFileSystemTest, TestAtime) {
  TestAtime();
}

TEST_F(MemFileSystemTest, TestMtime) {
  TestMtime();
}

TEST_F(MemFileSystemTest, TestMtimeWithAtimeDisabled) {
  mem_file_system_.set_atime_enabled(false);
  TestMtime();
}

TEST_F(MemFileSystemTest, MtimeAtimeAcrossRename) {
  const int kCurrentTimeSec = 12345;
  mem_file_system_.set_advance_time_on_update(false, &timer_);
  timer_.SetTimeMs(kCurrentTimeSec * Timer::kSecondMs);
  ASSERT_TRUE(file_system()->WriteFileAtomic("my.file", "hello, world",
                                             &handler_));
  int64 mtime_sec = 0, atime_sec = 0;
  ASSERT_TRUE(file_system()->Mtime("my.file", &mtime_sec, &handler_));
  EXPECT_EQ(kCurrentTimeSec, mtime_sec);
  ASSERT_TRUE(file_system()->Atime("my.file", &atime_sec, &handler_));
  EXPECT_EQ(kCurrentTimeSec, atime_sec);
}

TEST_F(MemFileSystemTest, TestDirInfo) {
  TestDirInfo();
}

TEST_F(MemFileSystemTest, TestSizeOld) {
  // Since we don't have directories, we need to do a slightly
  // different size test.
  GoogleString filename1 = "file-in-dir.txt";
  GoogleString filename2 = "another-file-in-dir.txt";
  GoogleString content1 = "12345";
  GoogleString content2 = "1234567890";
  ASSERT_TRUE(file_system()->WriteFile(filename1.c_str(),
                                       content1, &handler_));
  ASSERT_TRUE(file_system()->WriteFile(filename2.c_str(),
                                       content2, &handler_));
  int64 size;

  EXPECT_TRUE(file_system()->Size(filename1, &size, &handler_));
  EXPECT_EQ(5, size);
  EXPECT_TRUE(file_system()->Size(filename2, &size, &handler_));
  EXPECT_EQ(10, size);
}

TEST_F(MemFileSystemTest, TestLock) {
  TestLock();
}

// TODO(sligocki): This test does not seem to work for MemFileSystem
// TEST_F(MemFileSystemTest, TestLockTimeout) {
//   TestLockTimeout();
// }

// Since this filesystem doesn't support directories, we skip these tests:
// TestIsDir
// TestRecursivelyMakeDir
// TestRecursivelyMakeDir_NoPermission
// TestRecursivelyMakeDir_FileInPath

}  // namespace net_instaweb
