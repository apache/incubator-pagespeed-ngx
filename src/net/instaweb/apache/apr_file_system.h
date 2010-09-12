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

#ifndef NET_INSTAWEB_APACHE_APR_FILE_SYSTEM_H_
#define NET_INSTAWEB_APACHE_APR_FILE_SYSTEM_H_

#include "base/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/message_handler.h"

struct apr_pool_t;
using net_instaweb::FileSystem;
using net_instaweb::MessageHandler;
using net_instaweb::BoolOrError;


namespace html_rewriter {

class AprFileSystem : public FileSystem {
 public:
  explicit AprFileSystem(apr_pool_t* pool);
  ~AprFileSystem();
  virtual bool Atime(const net_instaweb::StringPiece& path,
                     int64* timestamp_sec, MessageHandler* handler);
  virtual InputFile* OpenInputFile(
      const char* file, MessageHandler* message_handler);
  virtual OutputFile* OpenOutputFileHelper(
      const char* file, MessageHandler* message_handler);
  // See FileSystem interface for specifics of OpenTempFile.
  virtual OutputFile* OpenTempFileHelper(
      const net_instaweb::StringPiece& prefix_name,
      MessageHandler* message_handler);
  virtual bool RenameFileHelper(const char* old_filename,
                                const char* new_filename,
                                MessageHandler* message_handler);
  virtual bool RemoveFile(const char* filename,
                          MessageHandler* message_handler);
  virtual bool Size(const net_instaweb::StringPiece& path, int64* size,
                    MessageHandler* handler);
  // Like POSIX 'mkdir', makes a directory only if parent directory exists.
  // Fails if directory_name already exists or parent directory doesn't exist.
  virtual bool MakeDir(const char* directory_path, MessageHandler* handler);

  // Like POSIX 'test -e', checks if path exists (is a file, directory, etc.).
  virtual BoolOrError Exists(const char* path, MessageHandler* handler);

  // Like POSIX 'test -d', checks if path exists and refers to a directory.
  virtual BoolOrError IsDir(const char* path, MessageHandler* handler);

  virtual bool ListContents(const net_instaweb::StringPiece& dir,
                            net_instaweb::StringVector* files,
                            MessageHandler* handler);

 private:
  apr_pool_t* pool_;

  DISALLOW_COPY_AND_ASSIGN(AprFileSystem);
};

}  // namespace html_rewriter

#endif  // NET_INSTAWEB_APACHE_APR_FILE_SYSTEM_H_
