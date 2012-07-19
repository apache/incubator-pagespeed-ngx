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

#include "net/instaweb/util/public/mem_file_system.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/file_system_test.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class Timer;

class MemFileSystemTest : public FileSystemTest {
 protected:
  MemFileSystemTest()
      : thread_system_(ThreadSystem::CreateThreadSystem()),
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

 private:
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

TEST_F(MemFileSystemTest, TestSize) {
  TestSize();
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
//TEST_F(MemFileSystemTest, TestLockTimeout) {
//  TestLockTimeout();
//}

// Since this filesystem doesn't support directories, we skip these tests:
// TestIsDir
// TestRecursivelyMakeDir
// TestRecursivelyMakeDir_NoPermission
// TestRecursivelyMakeDir_FileInPath

}  // namespace net_instaweb
