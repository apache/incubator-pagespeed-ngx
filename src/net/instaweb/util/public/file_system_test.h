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

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

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

  // Delete (at least) the named file or directory and everything
  // underneath it.  The test is permitted to delete more things (up
  // to and including the entire file system).
  virtual void DeleteRecursively(const StringPiece& filename) = 0;

  // Provide a pointer to your favorite filesystem implementation.
  virtual FileSystem* file_system() = 0;

  // Provide a temporary directory for tests to put files in.
  virtual GoogleString test_tmpdir() = 0;

  GoogleString WriteNewFile(const StringPiece& suffix,
                            const GoogleString& content);

  void TestWriteRead();
  void TestTemp();
  void TestRename();
  void TestRemove();
  void TestExists();
  void TestCreateFileInDir();
  void TestMakeDir();
  void TestIsDir();
  void TestRecursivelyMakeDir();
  void TestRecursivelyMakeDir_NoPermission();
  void TestRecursivelyMakeDir_FileInPath();
  void TestListContents();
  void TestAtime();
  void TestSize();
  void TestLock();
  void TestLockTimeout(Timer* timer);

  GoogleMessageHandler handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSystemTest);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FILE_SYSTEM_TEST_H_
