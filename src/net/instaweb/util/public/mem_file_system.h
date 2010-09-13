/**
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MEM_FILE_SYSTEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MEM_FILE_SYSTEM_H_

#include <stdlib.h>
#include <map>
#include "base/basictypes.h"
#include "net/instaweb/util/public/file_system.h"

namespace net_instaweb {

// An in-memory implementation of the FileSystem interface, e.g. for use in
// unit tests.  Does not support directories.  Not particularly efficient.
// TODO(abliss): add an ability to block writes for arbitrarily long, to
// enable testing resilience to concurrency problems with real filesystems.
class MemFileSystem : public FileSystem {
 public:
  MemFileSystem() : temp_file_index_(0), current_time_(0) {}
  virtual ~MemFileSystem();

  // We offer a "simulated atime" in which the clock ticks forward one
  // second every time you read or write a file.
  // TODO(abliss): replace this with a MockTimer.
  virtual bool Atime(const StringPiece& path, int64* timestamp_sec,
                     MessageHandler* handler);
  virtual BoolOrError Exists(const char* path, MessageHandler* handler);
  virtual BoolOrError IsDir(const char* path, MessageHandler* handler);
  virtual bool ListContents(const StringPiece& dir, StringVector* files,
                            MessageHandler* handler);
  virtual bool MakeDir(const char* directory_path, MessageHandler* handler);
  virtual InputFile* OpenInputFile(const char* filename,
                                   MessageHandler* message_handler);
  virtual OutputFile* OpenOutputFileHelper(const char* filename,
                                          MessageHandler* message_handler);
  virtual OutputFile* OpenTempFileHelper(const StringPiece& prefix_name,
                                        MessageHandler* message_handle);
  virtual bool RecursivelyMakeDir(const StringPiece& directory_path,
                                  MessageHandler* handler);
  virtual bool RemoveFile(const char* filename, MessageHandler* handler);
  virtual bool RenameFileHelper(const char* old_file, const char* new_file,
                               MessageHandler* handler);
  virtual bool Size(const StringPiece& path, int64* size,
                    MessageHandler* handler);

  // Empties out the entire filesystem.  Should not be called while files
  // are open.
  void Clear();
 private:
  typedef std::map<std::string, std::string> StringMap;
  StringMap string_map_;
  std::map<std::string, int64> atime_map_;
  int temp_file_index_;
  int current_time_;
  DISALLOW_COPY_AND_ASSIGN(MemFileSystem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MEM_FILE_SYSTEM_H_
