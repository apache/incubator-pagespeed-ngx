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

#ifndef PAGESPEED_KERNEL_BASE_MEM_FILE_SYSTEM_H_
#define PAGESPEED_KERNEL_BASE_MEM_FILE_SYSTEM_H_

#include <map>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"

namespace net_instaweb {

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
  typedef Callback1<const GoogleString&> FileCallback;

  explicit MemFileSystem(ThreadSystem* threads, Timer* timer);
  virtual ~MemFileSystem();

  virtual InputFile* OpenInputFile(const char* filename,
                                   MessageHandler* message_handler);
  virtual OutputFile* OpenOutputFileHelper(const char* filename,
                                           bool append,
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
                    MessageHandler* handler) const;
  virtual BoolOrError Exists(const char* path, MessageHandler* handler);
  virtual BoolOrError IsDir(const char* path, MessageHandler* handler);

  virtual BoolOrError TryLock(const StringPiece& lock_name,
                              MessageHandler* handler);
  virtual BoolOrError TryLockWithTimeout(const StringPiece& lock_name,
                                         int64 timeout_ms,
                                         const Timer* timer,
                                         MessageHandler* handler);
  virtual bool BumpLockTimeout(const StringPiece& lock_name,
                               MessageHandler* handler);
  virtual bool Unlock(const StringPiece& lock_name, MessageHandler* handler);

  // When atime is disabled, reading a file will not update its atime.
  void set_atime_enabled(bool enabled) {
    ScopedMutex lock(all_else_mutex_.get());
    atime_enabled_ = enabled;
  }

  // In order to test file-system 'atime' code, we need to move mock
  // time forward during tests by an entire second (aka 1000 ms).
  // However, that's disruptive to other tests that try to use
  // mock-time to examine millisecond-level timing, so we leave this
  // behavior off by default.
  bool advance_time_on_update() {
    ScopedMutex lock(all_else_mutex_.get());
    return advance_time_on_update_;
  }
  void set_advance_time_on_update(bool x, MockTimer* mock_timer) {
    ScopedMutex lock(all_else_mutex_.get());
    advance_time_on_update_ = x;
    mock_timer_ = mock_timer;
  }

  // Empties out the entire filesystem.  Should not be called while files
  // are open.
  void Clear();

  // Test-specific functionality to disable and re-enable the filesystem.
  void Disable() {
    ScopedMutex lock(all_else_mutex_.get());
    enabled_ = false;
  }
  void Enable() {
    ScopedMutex lock(all_else_mutex_.get());
    enabled_ = true;
  }

  // Access statistics.
  void ClearStats() {
    ScopedMutex lock(all_else_mutex_.get());
    num_input_file_opens_ = 0;
    num_input_file_stats_ = 0;
    num_output_file_opens_ = 0;
    num_temp_file_opens_ = 0;
  }
  int num_input_file_opens() const {
    ScopedMutex lock(all_else_mutex_.get());
    return num_input_file_opens_;
  }

  // returns number of times MTime was called.
  int num_input_file_stats() const {
    ScopedMutex lock(all_else_mutex_.get());
    return num_input_file_stats_;
  }
  int num_output_file_opens() const {
    ScopedMutex lock(all_else_mutex_.get());
    return num_output_file_opens_;
  }
  int num_temp_file_opens() const {
    ScopedMutex lock(all_else_mutex_.get());
    return num_temp_file_opens_;
  }

  // Adds a callback to be called once after a file-write and then
  // deleted.
  //
  // This is intended primarily for testing, and thus is not on the base
  // class.
  void set_write_callback(FileCallback* x) { write_callback_.reset(x); }

  virtual bool WriteFile(const char* filename,
                         const StringPiece& buffer,
                         MessageHandler* handler);
  virtual bool WriteTempFile(const StringPiece& prefix_name,
                             const StringPiece& buffer,
                             GoogleString* filename,
                             MessageHandler* handler);


 private:
  inline void UpdateAtime(const StringPiece& path)
      SHARED_LOCKS_REQUIRED(all_else_mutex_);
  inline void UpdateMtime(const StringPiece& path)
      SHARED_LOCKS_REQUIRED(all_else_mutex_);

  scoped_ptr<AbstractMutex> lock_map_mutex_;  // controls access to lock_map_
  scoped_ptr<AbstractMutex> all_else_mutex_;  // controls access to all else.

  // When disabled, OpenInputFile returns NULL.
  bool enabled_ GUARDED_BY(all_else_mutex_);
  // MemFileSystem::RemoveDir depends on string_map_ being sorted by key. If an
  // unsorted data structure is used (say a hash_map) this implementation will
  // need to be modified.
  StringStringMap string_map_ GUARDED_BY(all_else_mutex_);
  Timer* timer_;
  // Used only for auto-advance functionality.
  MockTimer* mock_timer_ GUARDED_BY(all_else_mutex_);

  // atime_map_ holds times (in s) that files were last opened/modified.  Each
  // time we do such an operation, timer() advances by 1s (so all ATimes are
  // distinct).
  // ctime and mtime are updated only for moves and modifications.
  std::map<GoogleString, int64> atime_map_ GUARDED_BY(all_else_mutex_);
  std::map<GoogleString, int64> mtime_map_ GUARDED_BY(all_else_mutex_);
  int temp_file_index_ GUARDED_BY(all_else_mutex_);
  // lock_map_ holds times that locks were established (in ms).
  // locking and unlocking don't advance time.
  std::map<GoogleString, int64> lock_map_ GUARDED_BY(lock_map_mutex_);
  bool atime_enabled_ GUARDED_BY(all_else_mutex_);

  // Indicates whether MemFileSystem will advance mock time whenever
  // a file is written.
  bool advance_time_on_update_ GUARDED_BY(all_else_mutex_);

  // Access statistics.
  int num_input_file_opens_ GUARDED_BY(all_else_mutex_);
  int num_input_file_stats_ GUARDED_BY(all_else_mutex_);
  int num_output_file_opens_ GUARDED_BY(all_else_mutex_);
  int num_temp_file_opens_ GUARDED_BY(all_else_mutex_);

  // Hook to run after a file-write.
  scoped_ptr<FileCallback> write_callback_;

  DISALLOW_COPY_AND_ASSIGN(MemFileSystem);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_MEM_FILE_SYSTEM_H_
