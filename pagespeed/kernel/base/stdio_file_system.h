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

#ifndef PAGESPEED_KERNEL_BASE_STDIO_FILE_SYSTEM_H_
#define PAGESPEED_KERNEL_BASE_STDIO_FILE_SYSTEM_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"

struct stat;

namespace net_instaweb {

class MessageHandler;
class Timer;

class StdioFileSystem : public FileSystem {
 public:
  StdioFileSystem();
  virtual ~StdioFileSystem();

  virtual int MaxPathLength(const StringPiece& base) const;

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
  virtual bool RemoveDir(const char* directory_path, MessageHandler* handler);
  virtual bool RemoveFile(const char* filename, MessageHandler* handler);
  virtual bool RenameFileHelper(const char* old_file, const char* new_file,
                                MessageHandler* handler);

  virtual bool Atime(const StringPiece& path, int64* timestamp_sec,
                     MessageHandler* handler);
  virtual bool Mtime(const StringPiece& path, int64* timestamp_sec,
                     MessageHandler* handler);
  // Report the disk utilization of the file specified by path. Note that disk
  // utilization could differ from the apparent size of the file as it depends
  // on the underlying file system and default block size.
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

  InputFile* Stdin();
  OutputFile* Stdout();
  OutputFile* Stderr();

  static void InitStats(Statistics* stats);
  void TrackTiming(int64 slow_file_latency_threshold_us, Timer* timer,
                   Statistics* stats, MessageHandler* handler);

  int64 StartTimer();
  void EndTimer(const char* filename, const char* operation, int64 start_us);

 private:
  // Used by *time and Size methods to get file info.
  bool Stat(const StringPiece& path, struct stat* statbuf,
            MessageHandler* handler) const;

  int64 slow_file_latency_threshold_us_;
  Timer* timer_;
  Statistics* statistics_;
  UpDownCounter* outstanding_ops_;
  Variable* slow_ops_;
  Variable* total_ops_;
  MessageHandler* message_handler_;  // Only set by TrackTiming, not by ctor.

  DISALLOW_COPY_AND_ASSIGN(StdioFileSystem);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_STDIO_FILE_SYSTEM_H_
