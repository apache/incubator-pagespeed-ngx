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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MEM_FILE_SYSTEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MEM_FILE_SYSTEM_H_

#include <cstdlib>
#include <map>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/mock_timer.h"

namespace net_instaweb {

// An in-memory implementation of the FileSystem interface, for use in
// unit tests.  Does not fully support directories.  Not particularly efficient.
// Not threadsafe.
// TODO(abliss): add an ability to block writes for arbitrarily long, to
// enable testing resilience to concurrency problems with real filesystems.
class MemFileSystem : public FileSystem {
 public:
  MemFileSystem() : enabled_(true), timer_(0), temp_file_index_(0),
                    atime_enabled_(true)  {}
  virtual ~MemFileSystem();

  // We offer a "simulated atime" in which the clock ticks forward one
  // second every time you read or write a file.
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
  virtual BoolOrError TryLock(const StringPiece& lock_name,
                              MessageHandler* handler);
  virtual BoolOrError TryLockWithTimeout(const StringPiece& lock_name,
                                         int64 timeout_ms,
                                         MessageHandler* handler);
  virtual bool Unlock(const StringPiece& lock_name,
                      MessageHandler* handler);

  // When atime is disabled, reading a file will not update its atime.
  void set_atime_enabled(bool enabled) { atime_enabled_ = enabled; }

  // Empties out the entire filesystem.  Should not be called while files
  // are open.
  void Clear();

  // Test-specific functionality to disable and re-enable the filesystem.
  void Disable() { enabled_ = false; }
  void Enable() { enabled_ = true; }

  // Accessor for timer.  Timer is owned by mem_file_system.
  MockTimer* timer() { return &timer_; }

 private:
  inline void UpdateAtime(const StringPiece& path);
  bool enabled_;  // When disabled, OpenInputFile returns NULL.
  StringStringMap string_map_;
  MockTimer timer_;
  // atime_map_ holds times (in s) that files were last opened/modified.  Each
  // time we do such an operation, timer() advances by 1s (so all ATimes are
  // distinct).
  std::map<GoogleString, int64> atime_map_;
  int temp_file_index_;
  // lock_map_ holds times that locks were established (in ms).
  // locking and unlocking don't advance time.
  std::map<GoogleString, int64> lock_map_;
  bool atime_enabled_;
  DISALLOW_COPY_AND_ASSIGN(MemFileSystem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MEM_FILE_SYSTEM_H_
