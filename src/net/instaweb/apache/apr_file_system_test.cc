// Copyright 2010 Google Inc.
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
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apr_file_system.h"
#include "net/instaweb/util/public/file_system_test.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

using html_rewriter::AprFileSystem;

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

class AprFileSystemTest : public net_instaweb::FileSystemTest {
 protected:
  AprFileSystemTest() { }

  virtual void DeleteRecursively(const net_instaweb::StringPiece& filename) {
    MyDeleteFileRecursively(filename.as_string(), NULL, NULL);
  }
  virtual FileSystem* file_system() {
    return file_system_.get();
  }
  virtual std::string test_tmpdir() {
    return test_tmpdir_;
  }
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

 protected:
  net_instaweb::GoogleMessageHandler handler_;
  scoped_ptr<AprFileSystem> file_system_;
  apr_pool_t* pool_;
  std::string test_tmpdir_;

  DISALLOW_COPY_AND_ASSIGN(AprFileSystemTest);
};

TEST_F(AprFileSystemTest, TestWriteRead) {
  TestWriteRead();
}

TEST_F(AprFileSystemTest, TestTemp) {
  TestTemp();
}

TEST_F(AprFileSystemTest, TestRename) {
  TestRename();
}

TEST_F(AprFileSystemTest, TestRemove) {
  TestRemove();
}

TEST_F(AprFileSystemTest, TestExists) {
  TestExists();
}

TEST_F(AprFileSystemTest, TestCreateFileInDir) {
  TestCreateFileInDir();
}


TEST_F(AprFileSystemTest, TestMakeDir) {
  TestMakeDir();
}

TEST_F(AprFileSystemTest, TestIsDir) {
  TestIsDir();
}

TEST_F(AprFileSystemTest, TestRecursivelyMakeDir) {
  TestRecursivelyMakeDir();
}

TEST_F(AprFileSystemTest, TestRecursivelyMakeDir_NoPermission) {
  TestRecursivelyMakeDir_NoPermission();
}

TEST_F(AprFileSystemTest, TestRecursivelyMakeDir_FileInPath) {
  TestRecursivelyMakeDir_FileInPath();
}

TEST_F(AprFileSystemTest, TestListContents) {
  TestListContents();
}

TEST_F(AprFileSystemTest, TestAtime) {
  TestAtime();
}

TEST_F(AprFileSystemTest, TestSize) {
  TestSize();
}

TEST_F(AprFileSystemTest, TestLock) {
  TestLock();
}

}  // namespace
