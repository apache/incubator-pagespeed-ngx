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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_FILE_SYSTEM_TEST_H_
#define NET_INSTAWEB_UTIL_PUBLIC_FILE_SYSTEM_TEST_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class FileSystem;
class Timer;

// Base class for testing a FileSystem implementation.  Subclasses
// must implement DeleteRecursively and GetFileSystem, then should
// create their own tests calling each of our Test* methods.
class FileSystemTest : public testing::Test {
 protected:
  FileSystemTest();
  virtual ~FileSystemTest();

  void CheckDoesNotExist(const GoogleString& filename);

  void CheckRead(const GoogleString& filename,
                 const GoogleString& expected_contents);

  void CheckInputFileRead(const GoogleString& filename,
                          const GoogleString& expected_contents);

  // Delete (at least) the named file or directory and everything
  // underneath it.  The test is permitted to delete more things (up
  // to and including the entire file system).
  virtual void DeleteRecursively(const StringPiece& filename) = 0;

  // Provide a pointer to your favorite filesystem implementation.
  virtual FileSystem* file_system() = 0;

  // Pointer to a timer to use in tests.
  virtual Timer* timer() = 0;

  // Provide a temporary directory for tests to put files in.
  const GoogleString& test_tmpdir() { return test_tmpdir_; }

  GoogleString WriteNewFile(const StringPiece& suffix,
                            const GoogleString& content);

  // Memory based file system implementations of Size return the size of the
  // file, while the APR file system returns the size allocated on disk. This
  // function is overridable to allow AprFileSystemTest and StdioFileSystemTest
  // to calculate the on-disk size of the file.
  virtual int FileSize(StringPiece contents) const = 0;

  int FileContentSize(StringPiece contents) const {
    return contents.size();
  }

  // Calculate on-disk usage of contents by returning size rounded up to nearest
  // default block size.
  int FileBlockSize(StringPiece contents, int64 default_file_size) const {
    return ((contents.size() + kBlockSize - 1) / kBlockSize) * kBlockSize +
        default_file_size;
  }

  // Return the size of directories in the file system. This can vary depending
  // on the implementation, since directories in disk-based file systems can
  // consume a disk block.
  virtual int DefaultDirSize() const = 0;

  // All FileSystem implementations should run the following tests.
  // Note: If you add a test below, please add invocations in:
  // AprFileSystemTest, StdioFileSystemTest, MemFileSystemTest.
  void TestWriteRead();
  void TestTemp();
  void TestAppend();
  void TestRename();
  void TestRemove();
  void TestExists();
  void TestCreateFileInDir();
  void TestMakeDir();
  void TestRemoveDir();
  void TestIsDir();
  void TestRecursivelyMakeDir();
  void TestRecursivelyMakeDir_NoPermission();
  void TestRecursivelyMakeDir_FileInPath();
  void TestListContents();
  void TestAtime();
  void TestMtime();
  void TestDirInfo();
  void TestLock();
  void TestLockTimeout();

  GoogleMessageHandler handler_;
  GoogleString test_tmpdir_;

 private:
  // Default file system block size is 4KB.
  static const int kBlockSize = 4096;

  DISALLOW_COPY_AND_ASSIGN(FileSystemTest);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FILE_SYSTEM_TEST_H_
