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
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the stdio file system

#include "pagespeed/kernel/base/stdio_file_system.h"

#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class StdioFileSystemTest : public FileSystemTest {
 protected:
  StdioFileSystemTest() {
    // Create the temp directory, so we are not dependent on test order
    // to make it.
    file_system()->RecursivelyMakeDir(test_tmpdir(), &handler_);

    // Also compute the "small" directory size. This seems to be different on
    // different FSs.
    EXPECT_TRUE(
        stdio_file_system_.Size(test_tmpdir(), &default_dir_size_, &handler_));

    // We also need to know how many blocks an empty file consumes. On ext3,
    // empty files are observed to consume 1 block (4K), while 1 byte files
    // consume 2 blocks. On ext4, empty files consume 0 blocks, and 1 byte files
    // consume 1 block.
    GoogleString tmpfile = test_tmpdir() + "/testfile";
    EXPECT_TRUE(file_system()->WriteFile(tmpfile.c_str(), "", &handler_));
    EXPECT_TRUE(file_system()->Size(tmpfile, &default_file_size_, &handler_));
  }
  virtual ~StdioFileSystemTest() {}

  virtual void DeleteRecursively(const StringPiece& filename) {
    GoogleString filename_string;
    filename.CopyToString(&filename_string);
    if (stdio_file_system_.Exists(
        filename_string.c_str(), &handler_).is_false()) {
      // OK if just not there.
      return;
    }
    DeleteRecursivelyImpl(filename.as_string());
  }
  virtual FileSystem* file_system() {
    return &stdio_file_system_;
  }
  virtual Timer* timer()  { return &timer_; }

  // Disk based file systems should return the number of disk blocks allocated
  // for a file, not the size of the contents.
  virtual int FileSize(StringPiece contents) const {
    return FileBlockSize(contents, default_file_size_);
  }

  virtual int DefaultDirSize() const {
    return default_dir_size_;
  }

 private:
  // This expects the file to not exist, for better error-checking.
  // DeleteRecursively() handles that case.
  void DeleteRecursivelyImpl(const GoogleString& filename) {
    if (stdio_file_system_.IsDir(filename.c_str(), &handler_).is_true()) {
      // Remove everything inside first.
      StringVector files;
      stdio_file_system_.ListContents(filename, &files, &handler_);
      for (int i = 0; i < files.size(); ++i) {
        ASSERT_TRUE(HasPrefixString(files[i], "/"));
        DeleteRecursivelyImpl(files[i]);
      }

      EXPECT_TRUE(
          stdio_file_system_.RemoveDir(filename.c_str(), &handler_));
    } else {
      EXPECT_TRUE(stdio_file_system_.RemoveFile(filename.c_str(), &handler_));
    }
  }

  PosixTimer timer_;
  StdioFileSystem stdio_file_system_;
  int64 default_dir_size_;
  int64 default_file_size_;

  DISALLOW_COPY_AND_ASSIGN(StdioFileSystemTest);
};

// Write a named file, then read it.
TEST_F(StdioFileSystemTest, TestWriteRead) {
  TestWriteRead();
}

// Write a temp file, then read it.
TEST_F(StdioFileSystemTest, TestTemp) {
  TestTemp();
}

// Write a temp file, close it, append to it, then read it.
TEST_F(StdioFileSystemTest, TestAppend) {
  TestAppend();
}

// Write a temp file, rename it, then read it.
TEST_F(StdioFileSystemTest, TestRename) {
  TestRename();
}

// Write a file and successfully delete it.
TEST_F(StdioFileSystemTest, TestRemove) {
  TestRemove();
}

// Write a file and check that it exists.
TEST_F(StdioFileSystemTest, TestExists) {
  TestExists();
}

// Create a file along with its directory which does not exist.
TEST_F(StdioFileSystemTest, TestCreateFileInDir) {
  TestCreateFileInDir();
}

// Make a directory and check that files may be placed in it.
TEST_F(StdioFileSystemTest, TestMakeDir) {
  TestMakeDir();
}

// Create a directory and verify removing it.
TEST_F(StdioFileSystemTest, TestRemoveDir) {
  TestRemoveDir();
}

// Make a directory and check that it is a directory.
TEST_F(StdioFileSystemTest, TestIsDir) {
  TestIsDir();
}

// Recursively make directories and check that it worked.
TEST_F(StdioFileSystemTest, TestRecursivelyMakeDir) {
  TestRecursivelyMakeDir();
}

// Check that we cannot create a directory we do not have permissions for.
// Note: depends upon root dir not being writable.
TEST_F(StdioFileSystemTest, TestRecursivelyMakeDir_NoPermission) {
  TestRecursivelyMakeDir_NoPermission();
}

// Check that we cannot create a directory below a file.
TEST_F(StdioFileSystemTest, TestRecursivelyMakeDir_FileInPath) {
  TestRecursivelyMakeDir_FileInPath();
}

// Check that we cannot create a directory below a file.
TEST_F(StdioFileSystemTest, TestListContents) {
  TestListContents();
}

TEST_F(StdioFileSystemTest, TestMtime) {
  TestMtime();
}

TEST_F(StdioFileSystemTest, TestDirInfo) {
  TestDirInfo();
}

TEST_F(StdioFileSystemTest, TestLock) {
  TestLock();
}

TEST_F(StdioFileSystemTest, TestLockTimeout) {
  TestLockTimeout();
}

}  // namespace net_instaweb
