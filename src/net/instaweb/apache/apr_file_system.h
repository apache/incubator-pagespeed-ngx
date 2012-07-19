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

#ifndef NET_INSTAWEB_APACHE_APR_FILE_SYSTEM_H_
#define NET_INSTAWEB_APACHE_APR_FILE_SYSTEM_H_

#include "apr.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/message_handler.h"

struct apr_finfo_t;
struct apr_pool_t;

namespace net_instaweb {

class AbstractMutex;
class ThreadSystem;

void AprReportError(MessageHandler* message_handler, const char* filename,
                    int line, const char* message, int error_code);

class AprFileSystem : public FileSystem {
 public:
  AprFileSystem(apr_pool_t* pool, ThreadSystem* thread_system);
  ~AprFileSystem();

  virtual int MaxPathLength(const StringPiece& base) const;
  virtual InputFile* OpenInputFile(
      const char* file, MessageHandler* message_handler);
  virtual OutputFile* OpenOutputFileHelper(
      const char* file, MessageHandler* message_handler);
  // See FileSystem interface for specifics of OpenTempFile.
  virtual OutputFile* OpenTempFileHelper(const StringPiece& prefix_name,
                                         MessageHandler* message_handler);

  virtual bool ListContents(const StringPiece& dir, StringVector* files,
                            MessageHandler* handler);
  // Like POSIX 'mkdir', makes a directory only if parent directory exists.
  // Fails if directory_name already exists or parent directory doesn't exist.
  virtual bool MakeDir(const char* directory_path, MessageHandler* handler);
  virtual bool RemoveDir(const char* directory_path,
                         MessageHandler* message_handler);
  virtual bool RemoveFile(const char* filename,
                          MessageHandler* message_handler);
  virtual bool RenameFileHelper(const char* old_filename,
                                const char* new_filename,
                                MessageHandler* message_handler);

  virtual bool Atime(const StringPiece& path,
                     int64* timestamp_sec, MessageHandler* handler);
  virtual bool Mtime(const StringPiece& path,
                     int64* timestamp_sec, MessageHandler* handler);
  virtual bool Size(const StringPiece& path, int64* size,
                    MessageHandler* handler);
  virtual BoolOrError Exists(const char* path, MessageHandler* handler);
  virtual BoolOrError IsDir(const char* path, MessageHandler* handler);

  virtual BoolOrError TryLock(const StringPiece& lock_name,
                              MessageHandler* handler);
  virtual BoolOrError TryLockWithTimeout(const StringPiece& lock_name,
                                         int64 timeout_ms,
                                         MessageHandler* handler);
  virtual bool Unlock(const StringPiece& lock_name, MessageHandler* handler);

 private:
  // Used by *time and Size methods to get file info.
  bool Stat(const StringPiece& path,
            apr_finfo_t* file_info, apr_int32_t field_wanted,
            MessageHandler* handler);

  apr_pool_t* pool_;

  // We use a mutex to protect the pool above when calling into apr's file
  // system ops, which might otherwise access it concurrently in an unsafe
  // way.
  scoped_ptr<AbstractMutex> mutex_;

  DISALLOW_COPY_AND_ASSIGN(AprFileSystem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APR_FILE_SYSTEM_H_
