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
//
// Author: lsong@google.com (Libo Song)

#include <string>

#include "apr_file_io.h"
#include "apr_pools.h"
#include "net/instaweb/util/public/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/apache/apr_file_system.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/file_system_test.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/thread_system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net_instaweb {

class AprFileSystemTest : public FileSystemTest {
 protected:
  AprFileSystemTest() { }

  virtual void DeleteRecursively(const StringPiece& filename) {
    MyDeleteFileRecursively(filename.as_string(), NULL, NULL);
  }
  virtual FileSystem* file_system() { return file_system_.get(); }
  virtual Timer* timer() { return &timer_; }
  virtual GoogleString test_tmpdir() { return test_tmpdir_; }
  virtual void SetUp() {
    apr_initialize();
    atexit(apr_terminate);
    apr_pool_create(&pool_, NULL);
    thread_system_.reset(ThreadSystem::CreateThreadSystem());
    file_system_.reset(new AprFileSystem(pool_, thread_system_.get()));
    GetAprFileSystemTestDir(&test_tmpdir_);
  }

  virtual void TearDown() {
    file_system_.reset();
    apr_pool_destroy(pool_);
  }

  void GetAprFileSystemTestDir(GoogleString* str) {
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

  void MyDeleteFileRecursively(const GoogleString& filename,
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

          // Handle case where filename was passed in with a '/' otherwise
          // apr_filepath_merge will generate the wrong path
          GoogleString tempname = filename;
          if (!tempname.empty() && tempname[tempname.size() - 1] == '/') {
            tempname.resize(tempname.size() - 1);
          }

          tempname += "-apr-XXXXXX";
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
  GoogleMessageHandler handler_;
  AprTimer timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  scoped_ptr<AprFileSystem> file_system_;
  apr_pool_t* pool_;
  GoogleString test_tmpdir_;

  DISALLOW_COPY_AND_ASSIGN(AprFileSystemTest);
};

TEST_F(AprFileSystemTest, TestWriteRead) {
  TestWriteRead();
}

TEST_F(AprFileSystemTest, TestTemp) {
  TestTemp();
}

TEST_F(AprFileSystemTest, TestAppend) {
  TestAppend();
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

// Create a directory and verify removing it.
TEST_F(AprFileSystemTest, TestRemoveDir) {
  TestRemoveDir();
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

// This test appears to be flaky under valgrind, possibly due to OS buffer
// cache issues.  It also won't work on file systems mounted with 'noatime'.
TEST_F(AprFileSystemTest, TestAtime) {
  if (!RunningOnValgrind()) {
    TestAtime();
  }
}

TEST_F(AprFileSystemTest, TestMtime) {
  TestMtime();
}

TEST_F(AprFileSystemTest, TestDirInfo) {
  TestDirInfo();
}

TEST_F(AprFileSystemTest, TestLock) {
  TestLock();
}

TEST_F(AprFileSystemTest, TestLockTimeout) {
  TestLockTimeout();
}

}  // namespace net_instaweb
