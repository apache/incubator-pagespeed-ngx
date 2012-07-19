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

#include <map>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class MessageHandler;
class MockTimer;
class ThreadSystem;
class Timer;

// An in-memory implementation of the FileSystem interface. This was originally
// for use in unit tests; but can also host the lock manager if needed.
// Does not fully support directories.  Not particularly efficient.
// Not threadsafe except for lock methods.
// TODO(abliss): add an ability to block writes for arbitrarily long, to
// enable testing resilience to concurrency problems with real filesystems.
//
// TODO(jmarantz): make threadsafe.
class MemFileSystem : public FileSystem {
 public:
  explicit MemFileSystem(ThreadSystem* threads, Timer* timer);
  virtual ~MemFileSystem();

  virtual InputFile* OpenInputFile(const char* filename,
                                   MessageHandler* message_handler);
  virtual OutputFile* OpenOutputFileHelper(const char* filename,
                                          MessageHandler* message_handler);
  virtual OutputFile* OpenTempFileHelper(const StringPiece& prefix_name,
                                        MessageHandler* message_handle);

  virtual bool ListContents(const StringPiece& dir, StringVector* files,
                            MessageHandler* handler);
  virtual bool MakeDir(const char* directory_path, MessageHandler* handler);
  virtual bool RecursivelyMakeDir(const StringPiece& directory_path,
                                  MessageHandler* handler);
  virtual bool RemoveDir(const char* path, MessageHandler* handler);
  virtual bool RemoveFile(const char* filename, MessageHandler* handler);
  virtual bool RenameFileHelper(const char* old_file, const char* new_file,
                               MessageHandler* handler);

  // We offer a "simulated atime" in which the clock ticks forward one
  // second every time you read or write a file.
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
  virtual BoolOrError TryLockWithTimeout(const StringPiece& lock_name,
                                         int64 timeout_ms,
                                         MessageHandler* handler);
  virtual bool Unlock(const StringPiece& lock_name, MessageHandler* handler);

  // When atime is disabled, reading a file will not update its atime.
  void set_atime_enabled(bool enabled) { atime_enabled_ = enabled; }

  // In order to test file-system 'atime' code, we need to move mock
  // time forward during tests by an entire second (aka 1000 ms).
  // However, that's disruptive to other tests that try to use
  // mock-time to examine millisecond-level timing, so we leave this
  // behavior off by default.
  bool advance_time_on_update() { return advance_time_on_update_; }
  void set_advance_time_on_update(bool x, MockTimer* mock_timer) {
    advance_time_on_update_ = x;
    mock_timer_ = mock_timer;
  }

  // Empties out the entire filesystem.  Should not be called while files
  // are open.
  void Clear();

  // Test-specific functionality to disable and re-enable the filesystem.
  void Disable() { enabled_ = false; }
  void Enable() { enabled_ = true; }

  // Access statistics.
  void ClearStats() {
    num_input_file_opens_ = 0;
    num_input_file_stats_ = 0;
    num_output_file_opens_ = 0;
    num_temp_file_opens_ = 0;
  }
  int num_input_file_opens() const { return num_input_file_opens_; }

  // returns number of times MTime was called.
  int num_input_file_stats() const { return num_input_file_stats_; }
  int num_output_file_opens() const { return num_output_file_opens_; }
  int num_temp_file_opens() const { return num_temp_file_opens_; }

 private:
  // all_else_mutex_ should be held when calling these.
  inline void UpdateAtime(const StringPiece& path);
  inline void UpdateMtime(const StringPiece& path);

  scoped_ptr<AbstractMutex> lock_map_mutex_;  // controls access to lock_map_
  scoped_ptr<AbstractMutex> all_else_mutex_;  // controls access to all else.

  bool enabled_;  // When disabled, OpenInputFile returns NULL.
  // MemFileSystem::RemoveDir depends on string_map_ being sorted by key. If an
  // unsorted data structure is used (say a hash_map) this implementation will
  // need to be modified.
  StringStringMap string_map_;
  Timer* timer_;
  MockTimer* mock_timer_;  // used only for auto-advance functionality.

  // atime_map_ holds times (in s) that files were last opened/modified.  Each
  // time we do such an operation, timer() advances by 1s (so all ATimes are
  // distinct).
  // ctime and mtime are updated only for moves and modifications.
  std::map<GoogleString, int64> atime_map_;
  std::map<GoogleString, int64> mtime_map_;
  int temp_file_index_;
  // lock_map_ holds times that locks were established (in ms).
  // locking and unlocking don't advance time.
  std::map<GoogleString, int64> lock_map_;
  bool atime_enabled_;

  // Indicates whether MemFileSystem will advance mock time whenever
  // a file is written.
  bool advance_time_on_update_;

  // Access statistics.
  int num_input_file_opens_;
  int num_input_file_stats_;
  int num_output_file_opens_;
  int num_temp_file_opens_;

  DISALLOW_COPY_AND_ASSIGN(MemFileSystem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MEM_FILE_SYSTEM_H_
