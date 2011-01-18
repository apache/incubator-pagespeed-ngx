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

#include <unistd.h>
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/file_system_test.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include <string>
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

FileSystemTest::FileSystemTest() { }
FileSystemTest::~FileSystemTest() { }

std::string FileSystemTest::WriteNewFile(const StringPiece& suffix,
                                          const std::string& content) {
  std::string filename = StrCat(test_tmpdir(), suffix);

  // Make sure we don't read an old file.
  DeleteRecursively(filename);
  EXPECT_TRUE(file_system()->WriteFile(filename.c_str(), content, &handler_));

  return filename;
}

// Check that a file has been read.
void FileSystemTest::CheckRead(const std::string& filename,
                               const std::string& expected_contents) {
  std::string buffer;
  ASSERT_TRUE(file_system()->ReadFile(filename.c_str(), &buffer, &handler_));
  EXPECT_EQ(buffer, expected_contents);
}

// Make sure we can no longer read the file by the old name.  Note
// that this will spew some error messages into the log file, and
// we can add a null_message_handler implementation to
// swallow them, if they become annoying.
void FileSystemTest::CheckDoesNotExist(const std::string& filename) {
  std::string read_buffer;
  EXPECT_FALSE(file_system()->ReadFile(filename.c_str(), &read_buffer,
                                       &handler_));
  EXPECT_TRUE(file_system()->Exists(filename.c_str(), &handler_).is_false());
}

// Write a named file, then read it.
void FileSystemTest::TestWriteRead() {
  std::string filename = test_tmpdir() + "/write.txt";
  std::string msg("Hello, world!");

  DeleteRecursively(filename);
  FileSystem::OutputFile* ofile = file_system()->OpenOutputFile(
      filename.c_str(), &handler_);
  ASSERT_TRUE(ofile != NULL);
  EXPECT_TRUE(ofile->Write(msg, &handler_));
  EXPECT_TRUE(file_system()->Close(ofile, &handler_));
  CheckRead(filename, msg);
}

// Write a temp file, then read it.
void FileSystemTest::TestTemp() {
  std::string prefix = test_tmpdir() + "/temp_prefix";
  FileSystem::OutputFile* ofile = file_system()->OpenTempFile(
      prefix.c_str(), &handler_);
  ASSERT_TRUE(ofile != NULL);
  std::string filename(ofile->filename());
  std::string msg("Hello, world!");
  EXPECT_TRUE(ofile->Write(msg, &handler_));
  EXPECT_TRUE(file_system()->Close(ofile, &handler_));

  CheckRead(filename, msg);
}

// Write a temp file, rename it, then read it.
void FileSystemTest::TestRename() {
  std::string from_text = "Now is time time";
  std::string to_file = test_tmpdir() + "/to.txt";
  DeleteRecursively(to_file);

  std::string from_file = WriteNewFile("/from.txt", from_text);
  ASSERT_TRUE(file_system()->RenameFile(from_file.c_str(), to_file.c_str(),
                                        &handler_));

  CheckDoesNotExist(from_file);
  CheckRead(to_file, from_text);
}

// Write a file and successfully delete it.
void FileSystemTest::TestRemove() {
  std::string filename = WriteNewFile("/remove.txt", "Goodbye, world!");
  ASSERT_TRUE(file_system()->RemoveFile(filename.c_str(), &handler_));
  CheckDoesNotExist(filename);
}

// Write a file and check that it exists.
void FileSystemTest::TestExists() {
  std::string filename = WriteNewFile("/exists.txt", "I'm here.");
  ASSERT_TRUE(file_system()->Exists(filename.c_str(), &handler_).is_true());
}

// Create a file along with its directory which does not exist.
void FileSystemTest::TestCreateFileInDir() {
  std::string dir_name = test_tmpdir() + "/make_dir";
  DeleteRecursively(dir_name);
  std::string filename = dir_name + "/file-in-dir.txt";

  FileSystem::OutputFile* file =
      file_system()->OpenOutputFile(filename.c_str(), &handler_);
  ASSERT_TRUE(file);
  file_system()->Close(file, &handler_);
}


// Make a directory and check that files may be placed in it.
void FileSystemTest::TestMakeDir() {
  std::string dir_name = test_tmpdir() + "/make_dir";
  DeleteRecursively(dir_name);
  std::string filename = dir_name + "/file-in-dir.txt";

  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  // ... but we can open a file after we've created the directory.
  FileSystem::OutputFile* file =
      file_system()->OpenOutputFile(filename.c_str(), &handler_);
  ASSERT_TRUE(file);
  file_system()->Close(file, &handler_);
}

// Make a directory and check that it is a directory.
void FileSystemTest::TestIsDir() {
  std::string dir_name = test_tmpdir() + "/this_is_a_dir";
  DeleteRecursively(dir_name);

  // Make sure we don't think the directory is there when it isn't ...
  ASSERT_TRUE(file_system()->IsDir(dir_name.c_str(), &handler_).is_false());
  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  // ... and that we do think it's there when it is.
  ASSERT_TRUE(file_system()->IsDir(dir_name.c_str(), &handler_).is_true());

  // Make sure that we don't think a regular file is a directory.
  std::string filename = dir_name + "/this_is_a_file.txt";
  std::string content = "I'm not a directory.";
  ASSERT_TRUE(file_system()->WriteFile(filename.c_str(), content, &handler_));
  ASSERT_TRUE(file_system()->IsDir(filename.c_str(), &handler_).is_false());
}

// Recursively make directories and check that it worked.
void FileSystemTest::TestRecursivelyMakeDir() {
  std::string base = test_tmpdir() + "/base";
  std::string long_path = base + "/dir/of/a/really/deep/hierarchy";
  DeleteRecursively(base);

  // Make sure we don't think the directory is there when it isn't ...
  ASSERT_TRUE(file_system()->IsDir(long_path.c_str(), &handler_).is_false());
  ASSERT_TRUE(file_system()->RecursivelyMakeDir(long_path, &handler_));
  // ... and that we do think it's there when it is.
  ASSERT_TRUE(file_system()->IsDir(long_path.c_str(), &handler_).is_true());
}

// Check that we cannot create a directory we do not have permissions for.
// Note: depends upon root dir not being writable.
void FileSystemTest::TestRecursivelyMakeDir_NoPermission() {
  std::string base = "/bogus-dir";
  std::string path = base + "/no/permission/to/make/this/dir";

  // Make sure the bogus bottom level directory is not there.
  ASSERT_TRUE(file_system()->Exists(base.c_str(), &handler_).is_false());
  // We do not have permission to create it.
  ASSERT_FALSE(file_system()->RecursivelyMakeDir(path, &handler_));
}

// Check that we cannot create a directory below a file.
void FileSystemTest::TestRecursivelyMakeDir_FileInPath() {
  std::string base = test_tmpdir() + "/file-in-path";
  std::string filename = base + "/this-is-a-file";
  std::string bad_path = filename + "/some/more/path";
  DeleteRecursively(base);
  std::string content = "Your path must end here. You shall not pass!";

  ASSERT_TRUE(file_system()->MakeDir(base.c_str(), &handler_));
  ASSERT_TRUE(file_system()->WriteFile(filename.c_str(), content, &handler_));
  ASSERT_FALSE(file_system()->RecursivelyMakeDir(bad_path, &handler_));
}

void FileSystemTest::TestListContents() {
  std::string dir_name = test_tmpdir() + "/make_dir";
  DeleteRecursively(dir_name);
  std::string filename1 = dir_name + "/file-in-dir.txt";
  std::string filename2 = dir_name + "/another-file-in-dir.txt";
  std::string content = "Lorem ipsum dolor sit amet";

  StringVector mylist;

  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  ASSERT_TRUE(file_system()->WriteFile(filename1.c_str(),
                                       content, &handler_));
  ASSERT_TRUE(file_system()->WriteFile(filename2.c_str(),
                                       content, &handler_));
  EXPECT_TRUE(file_system()->ListContents(dir_name, &mylist, &handler_));
  EXPECT_EQ(size_t(2), mylist.size());
  // Make sure our filenames are in there
  EXPECT_FALSE(filename1.compare(mylist.at(0))
               && filename1.compare(mylist.at(1)));
  EXPECT_FALSE(filename2.compare(mylist.at(0))
               && filename2.compare(mylist.at(1)));
}

void FileSystemTest::TestAtime() {
  std::string dir_name = test_tmpdir() + "/make_dir";
  DeleteRecursively(dir_name);
  std::string filename1 = "file-in-dir.txt";
  std::string filename2 = "another-file-in-dir.txt";
  std::string full_path1 = dir_name + "/" + filename1;
  std::string full_path2 = dir_name + "/" + filename2;
  std::string content = "Lorem ipsum dolor sit amet";
  // We need to sleep a bit between accessing files so that the
  // difference shows up in in atimes which are measured in seconds.
  unsigned int sleep_micros = 1500000;

  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path1.c_str(),
                                       content, &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path2.c_str(),
                                       content, &handler_));

  int64 atime1, atime2;
  CheckRead(full_path1, content);
  usleep(sleep_micros);
  CheckRead(full_path2, content);
  ASSERT_TRUE(file_system()->Atime(full_path1, &atime1, &handler_));
  ASSERT_TRUE(file_system()->Atime(full_path2, &atime2, &handler_));
  EXPECT_LT(atime1, atime2);

  CheckRead(full_path2, content);
  usleep(sleep_micros);
  CheckRead(full_path1, content);
  ASSERT_TRUE(file_system()->Atime(full_path1, &atime1, &handler_));
  ASSERT_TRUE(file_system()->Atime(full_path2, &atime2, &handler_));
  EXPECT_LT(atime2, atime1);

}

void FileSystemTest::TestSize() {
  std::string dir_name = test_tmpdir() + "/make_dir";
  DeleteRecursively(dir_name);
  std::string dir_name2 = dir_name + "/make_dir2";
  std::string filename1 = "file-in-dir.txt";
  std::string filename2 = "another-file-in-dir.txt";
  std::string full_path1 = dir_name2 + "/" + filename1;
  std::string full_path2 = dir_name2 + "/" + filename2;
  std::string content1 = "12345";
  std::string content2 = "1234567890";
  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  ASSERT_TRUE(file_system()->MakeDir(dir_name2.c_str(), &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path1.c_str(),
                                       content1, &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path2.c_str(),
                                       content2, &handler_));
  int64 size;

  EXPECT_TRUE(file_system()->Size(full_path1, &size, &handler_));
  EXPECT_EQ(content1.size(), size_t(size));
  EXPECT_TRUE(file_system()->Size(full_path2, &size, &handler_));
  EXPECT_EQ(content2.size(), size_t(size));
  size = 0;
  EXPECT_TRUE(file_system()->RecursiveDirSize(dir_name2, &size, &handler_));
  EXPECT_EQ(content1.size() + content2.size(), size_t(size));
  size = 0;
  EXPECT_TRUE(file_system()->RecursiveDirSize(dir_name, &size, &handler_));
  EXPECT_EQ(content1.size() + content2.size(), size_t(size));
}

void FileSystemTest::TestLock() {
  std::string dir_name = test_tmpdir() + "/make_dir";
  DeleteRecursively(dir_name);
  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  std::string lock_name = dir_name + "/lock";
  // Acquire the lock
  EXPECT_TRUE(file_system()->TryLock(lock_name, &handler_).is_true());
  // Can't re-acquire the lock
  EXPECT_TRUE(file_system()->TryLock(lock_name, &handler_).is_false());
  // Release the lock
  EXPECT_TRUE(file_system()->Unlock(lock_name, &handler_));
  // Do it all again to make sure the release worked.
  EXPECT_TRUE(file_system()->TryLock(lock_name, &handler_).is_true());
  EXPECT_TRUE(file_system()->TryLock(lock_name, &handler_).is_false());
  EXPECT_TRUE(file_system()->Unlock(lock_name, &handler_));
}

}  // namespace net_instaweb
