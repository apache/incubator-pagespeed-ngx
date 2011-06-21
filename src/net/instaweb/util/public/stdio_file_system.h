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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STDIO_FILE_SYSTEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STDIO_FILE_SYSTEM_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/string_util.h"

struct stat;

namespace net_instaweb {

class MessageHandler;

class StdioFileSystem : public FileSystem {
 public:
  StdioFileSystem() {}
  virtual ~StdioFileSystem();

  virtual InputFile* OpenInputFile(const char* filename,
                                   MessageHandler* message_handler);
  virtual OutputFile* OpenOutputFileHelper(const char* filename,
                                           MessageHandler* message_handler);
  virtual OutputFile* OpenTempFileHelper(const StringPiece& prefix_name,
                                         MessageHandler* message_handle);

  virtual bool ListContents(const StringPiece& dir, StringVector* files,
                            MessageHandler* handler);
  virtual bool MakeDir(const char* directory_path, MessageHandler* handler);
  virtual bool RemoveFile(const char* filename, MessageHandler* handler);
  virtual bool RenameFileHelper(const char* old_file, const char* new_file,
                                MessageHandler* handler);

  virtual bool Atime(const StringPiece& path, int64* timestamp_sec,
                     MessageHandler* handler);
  virtual bool Mtime(const StringPiece& path, int64* timestamp_sec,
                     MessageHandler* handler);
  virtual bool Size(const StringPiece& path, int64* size,
                    MessageHandler* handler);
  virtual BoolOrError Exists(const char* path, MessageHandler* handler);
  virtual BoolOrError IsDir(const char* path, MessageHandler* handler);

  virtual BoolOrError TryLock(const StringPiece& lock_name,
                              MessageHandler* handler);
  virtual bool Unlock(const StringPiece& lock_name, MessageHandler* handler);

  InputFile* Stdin();
  OutputFile* Stdout();
  OutputFile* Stderr();

 private:
  // Used by *time and Size methods to get file info.
  bool Stat(const StringPiece& path, struct stat* statbuf,
            MessageHandler* handler);

  DISALLOW_COPY_AND_ASSIGN(StdioFileSystem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STDIO_FILE_SYSTEM_H_
