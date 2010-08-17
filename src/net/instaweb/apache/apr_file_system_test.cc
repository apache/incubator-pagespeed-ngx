// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include "apr_file_io.h"
#include "apr_pools.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apr_file_system.h"
#include "net/instaweb/apache/html_parser_message_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

using html_rewriter::AprFileSystem;
using html_rewriter::HtmlParserMessageHandler;

namespace {
const int kErrorMessageBufferSize = 1024;

void AprReportError(MessageHandler* message_handler, const char* filename,
                    int line, const char* message, int error_code) {
  char buf[kErrorMessageBufferSize];
  apr_strerror(error_code, buf, sizeof(buf));
  std::string error_format(message);
  error_format.append(" (code=%d %s)");
  message_handler->Error(filename, line, error_format.c_str(), error_code, buf);
}

class AprFileSystemTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    apr_initialize();
    atexit(apr_terminate);
    apr_pool_create(&pool_, NULL);
    file_system_.reset(new AprFileSystem(pool_));
    GetAprFileSystemTestDir(&test_tmpdir_);
  }

  virtual void TearDown() {
    file_system_.reset();
    apr_pool_destroy(pool_);
  }

  void GetAprFileSystemTestDir(std::string* str) {
    const char* tmpdir;
    apr_status_t status = apr_temp_dir_get(&tmpdir, pool_);
    ASSERT_EQ(APR_SUCCESS, status);
    char* test_temp_dir;
    status = apr_filepath_merge(&test_temp_dir, tmpdir, "apr_file_sytem_test",
                                APR_FILEPATH_NATIVE, pool_);
    ASSERT_EQ(APR_SUCCESS, status);
    if (file_system_->Exists(test_temp_dir, &handler_).is_false()) {
      ASSERT_TRUE(file_system_->MakeDir(test_temp_dir, &handler_));
    }
    ASSERT_TRUE(file_system_->Exists(test_temp_dir, &handler_).is_true());
    *str = test_temp_dir;
  }

  void MyDeleteFileRecursively(const std::string& filename,
                               const char* /*a*/,
                               const char* /*b*/) {
    if (file_system_->IsDir(filename.c_str(), &handler_).is_true()) {
      // TODO(lsong): Make it recursive.
      apr_status_t status = apr_dir_remove(filename.c_str(), pool_);
      if (status != APR_SUCCESS) {
        AprReportError(&handler_, __FILE__, __LINE__, "dir remove", status);
        // TODO(lsong): Rename the dir to try.
        if (APR_STATUS_IS_ENOTEMPTY(status)) {
          // Need a tempname to rename to.
          char* template_name;
          std::string tempname = filename + "-apr-XXXXXX";
          status = apr_filepath_merge(&template_name, test_tmpdir_.c_str(),
                                      tempname.c_str(), APR_FILEPATH_NATIVE,
                                      pool_);
          ASSERT_EQ(APR_SUCCESS, status);
          apr_file_t* file;
          status = apr_file_mktemp(&file, template_name, 0, pool_);
          ASSERT_EQ(APR_SUCCESS, status);
          const char* the_path_name;
          status = apr_file_name_get(&the_path_name, file);
          ASSERT_EQ(APR_SUCCESS, status);
          status = apr_file_close(file);
          ASSERT_EQ(APR_SUCCESS, status);
          // Got the name to rename to.
          status = apr_file_rename(filename.c_str(), the_path_name, pool_);
          if (status != APR_SUCCESS) {
            AprReportError(&handler_, __FILE__, __LINE__, "dir rename", status);
          }
        }
      }
      ASSERT_EQ(APR_SUCCESS, status);
    } else {
      file_system_->RemoveFile(filename.c_str(), &handler_);
    }
  }
  std::string WriteNewFile(const std::string& suffix,
                           const std::string& content) {
    std::string filename(test_tmpdir_);
    filename += suffix;

    // Make sure we don't read an old file.
    MyDeleteFileRecursively(filename, NULL, NULL);
    EXPECT_TRUE(file_system_->WriteFile(filename.c_str(), content, &handler_));

    return filename;
  }

  // Check that a file has been read.  We could use
  // file_base::ReadFileToStringOrDie, but we are trying
  // to also test our file-reading code in AprFileSystem.
  void CheckRead(const std::string& filename,
                 const std::string& expected_contents) {
    std::string buffer;
    ASSERT_TRUE(file_system_->ReadFile(filename.c_str(), &buffer, &handler_));
    EXPECT_EQ(buffer, expected_contents);
  }

  // Make sure we can no longer read the file by the old name.  Note
  // that this will spew some error messages into the log file, and
  // we can add a null_message_handler implementation to
  // swallow them, if they become annoying.
  void CheckDoesNotExist(const std::string& filename) {
    std::string read_buffer;
    EXPECT_FALSE(file_system_->ReadFile(filename.c_str(), &read_buffer,
                                        &handler_));
  }

 protected:
  HtmlParserMessageHandler handler_;
  scoped_ptr<AprFileSystem> file_system_;
  apr_pool_t* pool_;
  std::string test_tmpdir_;
};

// Write a named file, then read it.
TEST_F(AprFileSystemTest, TestWriteRead) {
  std::string filename = test_tmpdir_ + "/write.txt";
  std::string msg("Hello, world!");

  MyDeleteFileRecursively(filename, NULL, NULL);
  FileSystem::OutputFile* ofile = file_system_->OpenOutputFile(
      filename.c_str(), &handler_);
  ASSERT_TRUE(ofile != NULL);
  EXPECT_TRUE(ofile->Write(msg, &handler_));
  EXPECT_TRUE(file_system_->Close(ofile, &handler_));
  CheckRead(filename, msg);
  // Do not leave the temp files around.
  ASSERT_TRUE(file_system_->RemoveFile(filename.c_str(), &handler_));
  CheckDoesNotExist(filename);
}

// Write a temp file, then read it.
TEST_F(AprFileSystemTest, TestTemp) {
  std::string prefix = test_tmpdir_ + "/temp_prefix";
  FileSystem::OutputFile* ofile = file_system_->OpenTempFile(
      prefix.c_str(), &handler_);
  ASSERT_TRUE(ofile != NULL);
  std::string filename(ofile->filename());
  std::string msg("Hello, world!");
  EXPECT_TRUE(ofile->Write(msg, &handler_));
  EXPECT_TRUE(file_system_->Close(ofile, &handler_));

  CheckRead(filename, msg);

  // Do not leave the temp files around.
  ASSERT_TRUE(file_system_->RemoveFile(filename.c_str(), &handler_));
  CheckDoesNotExist(filename);
}

// Write a temp file, rename it, then read it.
TEST_F(AprFileSystemTest, TestRename) {
  std::string from_text = "Now is time time";
  std::string from_file = WriteNewFile("/from.txt", from_text);
  std::string to_file = test_tmpdir_ + "/to.txt";

  MyDeleteFileRecursively(to_file, NULL, NULL);
  ASSERT_TRUE(file_system_->RenameFile(from_file.c_str(), to_file.c_str(),
                                       &handler_));

  CheckDoesNotExist(from_file);
  CheckRead(to_file, from_text);
  // Do not leave the temp files around.
  ASSERT_TRUE(file_system_->RemoveFile(to_file.c_str(), &handler_));
  CheckDoesNotExist(to_file);
}

// Write a file and successfully delete it.
TEST_F(AprFileSystemTest, TestRemove) {
  std::string filename = WriteNewFile("/remove.txt", "Goodbye, world!");
  ASSERT_TRUE(file_system_->RemoveFile(filename.c_str(), &handler_));
  CheckDoesNotExist(filename);
}

// Write a file and check that it exists.
TEST_F(AprFileSystemTest, TestExists) {
  std::string filename = WriteNewFile("/exists.txt", "I'm here.");
  ASSERT_TRUE(file_system_->Exists(filename.c_str(), &handler_).is_true());
  // Do not leave the temp files around.
  ASSERT_TRUE(file_system_->RemoveFile(filename.c_str(), &handler_));
  CheckDoesNotExist(filename);
}

// Create a file along with its directory which does not exist.
TEST_F(AprFileSystemTest, TestCreateFileInDir) {
  std::string dir_name = test_tmpdir_ + "/make_dir";
  MyDeleteFileRecursively(dir_name, NULL, NULL);
  std::string filename = dir_name + "/file-in-dir.txt";

  FileSystem::OutputFile* file =
      file_system_->OpenOutputFile(filename.c_str(), &handler_);
  ASSERT_TRUE(file);
  file_system_->Close(file, &handler_);
}

// Make a directory and check that files may be placed in it.
TEST_F(AprFileSystemTest, TestMakeDir) {
  std::string dir_name = test_tmpdir_ + "/make_dir";
  MyDeleteFileRecursively(dir_name, NULL, NULL);
  std::string filename = dir_name + "/file-in-dir.txt";

  ASSERT_TRUE(file_system_->MakeDir(dir_name.c_str(), &handler_));
  // ... but we can open a file after we've created the directory.
  FileSystem::OutputFile* file =
      file_system_->OpenOutputFile(filename.c_str(), &handler_);
  ASSERT_TRUE(file);
  file_system_->Close(file, &handler_);
  // Do not leave the temp files around.
  ASSERT_TRUE(file_system_->RemoveFile(filename.c_str(), &handler_));
  CheckDoesNotExist(filename);
}

// Make a directory and check that it is a directory.
TEST_F(AprFileSystemTest, TestIsDir) {
  std::string dir_name = test_tmpdir_ + "/this_is_a_dir";
  MyDeleteFileRecursively(dir_name, NULL, NULL);

  // Make sure we don't think the directory is there when it isn't ...
  ASSERT_TRUE(file_system_->IsDir(dir_name.c_str(), &handler_).is_false());
  ASSERT_TRUE(file_system_->MakeDir(dir_name.c_str(), &handler_));
  // ... and that we do think it's there when it is.
  ASSERT_TRUE(file_system_->IsDir(dir_name.c_str(), &handler_).is_true());

  // Make sure that we don't think a regular file is a directory.
  std::string filename = dir_name + "/this_is_a_file.txt";
  std::string content = "I'm not a directory.";
  ASSERT_TRUE(file_system_->WriteFile(filename.c_str(), content, &handler_));
  ASSERT_TRUE(file_system_->IsDir(filename.c_str(), &handler_).is_false());
}

// Recursively make directories and check that it worked.
TEST_F(AprFileSystemTest, TestRecursivelyMakeDir) {
  std::string base = test_tmpdir_ + "/base";
  std::string long_path = base + "/dir/of/a/really/deep/hierarchy";
  MyDeleteFileRecursively(base, NULL, NULL);

  // Make sure we don't think the directory is there when it isn't ...
  ASSERT_TRUE(file_system_->IsDir(long_path.c_str(), &handler_).is_false());
  ASSERT_TRUE(file_system_->RecursivelyMakeDir(long_path, &handler_));
  // ... and that we do think it's there when it is.
  ASSERT_TRUE(file_system_->IsDir(long_path.c_str(), &handler_).is_true());
}

// Check that we cannot create a directory we do not have permissions for.
// Note: depends upon root dir not being writable.
TEST_F(AprFileSystemTest, TestRecursivelyMakeDir_NoPermission) {
  std::string base = "/bogus-dir";
  std::string path = base + "/no/permission/to/make/this/dir";

  // Make sure the bogus bottom level directory is not there.
  ASSERT_TRUE(file_system_->Exists(base.c_str(), &handler_).is_false());
  // We do not have permission to create it.
  ASSERT_FALSE(file_system_->RecursivelyMakeDir(path, &handler_));
}

// Check that we cannot create a directory below a file.
TEST_F(AprFileSystemTest, TestRecursivelyMakeDir_FileInPath) {
  std::string base = test_tmpdir_ + "/file-in-path";
  std::string filename = base + "/this-is-a-file";
  std::string bad_path = filename + "/some/more/path";
  MyDeleteFileRecursively(base, NULL, NULL);
  std::string content = "Your path must end here. You shall not pass!";

  ASSERT_TRUE(file_system_->MakeDir(base.c_str(), &handler_));
  ASSERT_TRUE(file_system_->WriteFile(filename.c_str(), content, &handler_));
  ASSERT_FALSE(file_system_->RecursivelyMakeDir(bad_path, &handler_));
}

}  // namespace
