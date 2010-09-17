// Copyright 2010 and onwards Google Inc.
// Author: abliss@google.com (Adam Bliss)

// Unit-test the in-memory filesystem

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/file_system_test.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include <string>

namespace net_instaweb {

class MemFileSystemTest : public FileSystemTest {
 protected:
  MemFileSystemTest() {}
  virtual void DeleteRecursively(const StringPiece& filename) {
    mem_file_system_.Clear();
  }
  virtual FileSystem* file_system() {
    return &mem_file_system_;
  }
  virtual std::string test_tmpdir() {
    return GTestTempDir();
  }
 private:
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

TEST_F(MemFileSystemTest, TestSize) {
  // Since we don't have directories, we need to do a slightly
  // different size test.
  std::string filename1 = "file-in-dir.txt";
  std::string filename2 = "another-file-in-dir.txt";
  std::string content1 = "12345";
  std::string content2 = "1234567890";
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

TEST_F(MemFileSystemTest, TestListContents) {
  TestListContents();
}

TEST_F(MemFileSystemTest, TestAtime) {
  // Slightly modified version of TestAtime, without the sleeps
  std::string dir_name = test_tmpdir() + "/make_dir";
  DeleteRecursively(dir_name);
  std::string filename1 = "file-in-dir.txt";
  std::string filename2 = "another-file-in-dir.txt";
  std::string full_path1 = dir_name + "/" + filename1;
  std::string full_path2 = dir_name + "/" + filename2;
  std::string content = "Lorem ipsum dolor sit amet";

  ASSERT_TRUE(file_system()->MakeDir(dir_name.c_str(), &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path1.c_str(),
                                       content, &handler_));
  ASSERT_TRUE(file_system()->WriteFile(full_path2.c_str(),
                                       content, &handler_));

  int64 atime1, atime2;
  CheckRead(full_path1, content);
  CheckRead(full_path2, content);
  ASSERT_TRUE(file_system()->Atime(full_path1, &atime1, &handler_));
  ASSERT_TRUE(file_system()->Atime(full_path2, &atime2, &handler_));
  EXPECT_LT(atime1, atime2);

  CheckRead(full_path2, content);
  CheckRead(full_path1, content);
  ASSERT_TRUE(file_system()->Atime(full_path1, &atime1, &handler_));
  ASSERT_TRUE(file_system()->Atime(full_path2, &atime2, &handler_));
  EXPECT_LT(atime2, atime1);
}

TEST_F(MemFileSystemTest, TestLock) {
  TestLock();
}

// Since this filesystem doesn't support directories, we skip these tests:
// TestIsDir
// TestRecursivelyMakeDir
// TestRecursivelyMakeDir_NoPermission
// TestRecursivelyMakeDir_FileInPath

}  // namespace net_instaweb
