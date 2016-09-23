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

#include "pagespeed/kernel/base/stdio_file_system.h"

#include <errno.h>
#include <sys/stat.h>
#include <utime.h>
#ifdef WIN32
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif  // WIN32

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include "base/logging.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/debug.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace {
// The st_blocks field returned by stat is the number of 512B blocks allocated
// for the files. (While POSIX doesn't specify this, it's the proper value on
// at least Linux, FreeBSD, and OS X).
const int kBlockSize = 512;

static const char kOutstandingOps[] = "stdio_fs_outstanding_ops";
static const char kSlowOps[] = "stdio_fs_slow_ops";
static const char kTotalOps[] = "stdio_fs_total_ops";

}  // namespace

namespace net_instaweb {

// Helper class to factor out common implementation details between Input and
// Output files, in lieu of multiple inheritance.
class StdioFileHelper {
 public:
  StdioFileHelper(FILE* f, const StringPiece& filename, StdioFileSystem* fs)
      : file_(f),
        file_system_(fs),
        start_us_(0) {
    filename.CopyToString(&filename_);
  }

  ~StdioFileHelper() {
    CHECK(file_ == NULL);
  }

  void ReportError(MessageHandler* message_handler, const char* context) {
    message_handler->Message(kError, "%s: %s %d(%s)", filename_.c_str(),
                             context, errno, strerror(errno));
  }

  bool Close(MessageHandler* message_handler) {
    bool ret = true;
    if (file_ != stdout && file_ != stderr && file_ != stdin) {
      if (fclose(file_) != 0) {
        ReportError(message_handler, "closing file");
        ret = false;
      }
    }
    file_ = NULL;
    return ret;
  }

  void StartTimer() {
    start_us_ = file_system_->StartTimer();
  }

  void EndTimer(const char* operation) {
    file_system_->EndTimer(filename_.c_str(), operation, start_us_);
  }


  FILE* file_;
  GoogleString filename_;
  StdioFileSystem* file_system_;
  int64 start_us_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StdioFileHelper);
};

class StdioInputFile : public FileSystem::InputFile {
 public:
  StdioInputFile(FILE* f, const StringPiece& filename, StdioFileSystem* fs)
      : file_helper_(f, filename, fs) {
  }

  bool ReadFile(GoogleString* buf, int64 max_file_size,
                MessageHandler* message_handler) override {
    bool ret = false;
    struct stat statbuf;
    file_helper_.StartTimer();
    if ((fstat(fileno(file_helper_.file_), &statbuf) < 0)) {
      file_helper_.ReportError(message_handler, "stating file");
    } else if (max_file_size == FileSystem::kUnlimitedSize ||
               statbuf.st_size <= max_file_size) {
      buf->resize(statbuf.st_size);
      int nread = fread(&(*buf)[0], 1, statbuf.st_size, file_helper_.file_);
      if (nread != statbuf.st_size) {
        file_helper_.ReportError(message_handler, "reading file");
      } else {
        ret = true;
      }
    }
    file_helper_.EndTimer("ReadFile");
    return ret;
  }

  int Read(char* buf, int size, MessageHandler* message_handler) override {
    file_helper_.StartTimer();
    int ret = fread(buf, 1, size, file_helper_.file_);
    if ((ret == 0) && (ferror(file_helper_.file_) != 0)) {
      file_helper_.ReportError(message_handler, "reading file");
    }
    file_helper_.EndTimer("read");
    return ret;
  }

  bool Close(MessageHandler* message_handler) override {
    return file_helper_.Close(message_handler);
  }

  const char* filename() override { return file_helper_.filename_.c_str(); }

 private:
  StdioFileHelper file_helper_;

  DISALLOW_COPY_AND_ASSIGN(StdioInputFile);
};

class StdioOutputFile : public FileSystem::OutputFile {
 public:
  StdioOutputFile(FILE* f, const StringPiece& filename, StdioFileSystem* fs)
      : file_helper_(f, filename, fs) {
  }

  virtual bool Write(const StringPiece& buf, MessageHandler* handler) {
    file_helper_.StartTimer();
    size_t bytes_written =
        fwrite(buf.data(), 1, buf.size(), file_helper_.file_);
    bool ret = (bytes_written == buf.size());
    if (!ret) {
      file_helper_.ReportError(handler, "writing file");
    }
    file_helper_.EndTimer("write");
    return ret;
  }

  virtual bool Flush(MessageHandler* message_handler) {
    bool ret = true;
    if (fflush(file_helper_.file_) != 0) {
      file_helper_.ReportError(message_handler, "flushing file");
      ret = false;
    }
    return ret;
  }

  virtual bool Close(MessageHandler* message_handler) {
    return file_helper_.Close(message_handler);
  }

  virtual const char* filename() { return file_helper_.filename_.c_str(); }

  virtual bool SetWorldReadable(MessageHandler* message_handler) {
    bool ret = true;
#ifdef WIN32
    const char* filename = file_helper_.filename_.c_str();
    ret = (_chmod(filename, _S_IREAD) == 0);
#else
    int fd = fileno(file_helper_.file_);
    ret = (fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == 0);
#endif  // WIN32
    if (!ret) {
      file_helper_.ReportError(message_handler, "setting world-readable");
    }
    return ret;
  }

 private:
  StdioFileHelper file_helper_;

  DISALLOW_COPY_AND_ASSIGN(StdioOutputFile);
};

StdioFileSystem::StdioFileSystem()
    : slow_file_latency_threshold_us_(0),
      timer_(NULL),
      statistics_(NULL),
      outstanding_ops_(NULL),
      slow_ops_(NULL),
      total_ops_(NULL) {
}

StdioFileSystem::~StdioFileSystem() {
}

void StdioFileSystem::InitStats(Statistics* stats) {
  stats->AddUpDownCounter(kOutstandingOps);
  stats->AddVariable(kSlowOps);
  stats->AddVariable(kTotalOps);
}

void StdioFileSystem::TrackTiming(int64 slow_file_latency_threshold_us,
                                  Timer* timer, Statistics* stats,
                                  MessageHandler* handler) {
  slow_file_latency_threshold_us_ = slow_file_latency_threshold_us;
  timer_ = timer;
  statistics_ = stats;
  outstanding_ops_ = stats->GetUpDownCounter(kOutstandingOps);
  slow_ops_ = stats->GetVariable(kSlowOps);
  total_ops_ = stats->GetVariable(kTotalOps);
  message_handler_ = handler;
}

int64 StdioFileSystem::StartTimer() {
  if (timer_ == NULL) {
    return 0;
  }
  if (outstanding_ops_ != NULL) {
    outstanding_ops_->Add(1);
  }
  if (total_ops_ != NULL) {
    total_ops_->Add(1);
  }
  return timer_->NowUs();
}

void StdioFileSystem::EndTimer(const char* filename, const char* operation,
                               int64 start_us) {
  if (outstanding_ops_ != NULL) {
    outstanding_ops_->Add(-1);
  }
  if (timer_ != NULL) {
    int64 end_us = timer_->NowUs();
    int64 latency_us = end_us - start_us;
    if (latency_us > slow_file_latency_threshold_us_) {
      if (slow_ops_ != NULL) {
        slow_ops_->Add(1);
      }
      message_handler_->Message(
          kError, "Slow %s operation on file %s: %gms; "
          "configure SlowFileLatencyUs to change threshold\n",
          operation, filename, latency_us / 1000.0);
    }
  }
}

int StdioFileSystem::MaxPathLength(const StringPiece& base) const {
#ifdef WIN32
  return MAX_PATH;
#else
  const int kMaxInt = std::numeric_limits<int>::max();

  long limit = pathconf(base.as_string().c_str(), _PC_PATH_MAX);  // NOLINT
  if (limit < 0) {
    // pathconf failed.
    return FileSystem::MaxPathLength(base);
  } else if (limit > kMaxInt) {
    // As pathconf returns a long, we may have to clamp it.
    return kMaxInt;
  } else {
    return limit;
  }
#endif  // WIN32
}

FileSystem::InputFile* StdioFileSystem::OpenInputFile(
    const char* filename, MessageHandler* message_handler) {
  FileSystem::InputFile* input_file = NULL;
  FILE* f = fopen(filename, "r");
  if (f == NULL) {
    message_handler->Error(filename, 0, "opening input file: %s",
                           strerror(errno));
  } else {
    input_file = new StdioInputFile(f, filename, this);
  }
  return input_file;
}


FileSystem::OutputFile* StdioFileSystem::OpenOutputFileHelper(
    const char* filename, bool append, MessageHandler* message_handler) {
  FileSystem::OutputFile* output_file = NULL;
  if (strcmp(filename, "-") == 0) {
    output_file = new StdioOutputFile(stdout, "<stdout>", this);
  } else {
    const char* mode = append ? "a" : "w";
    FILE* f = fopen(filename, mode);
    if (f == NULL) {
      message_handler->Error(filename, 0,
                             "opening output file: %s", strerror(errno));
    } else {
      output_file = new StdioOutputFile(f, filename, this);
    }
  }
  return output_file;
}

FileSystem::OutputFile* StdioFileSystem::OpenTempFileHelper(
    const StringPiece& prefix, MessageHandler* message_handler) {
  // TODO(jmarantz): As jmaessen points out, mkstemp warns "Don't use
  // this function, use tmpfile(3) instead.  It is better defined and
  // more portable."  However, tmpfile does not allow a location to be
  // specified.  I'm not 100% sure if that's going to be work well for
  // us.  More importantly, our usage scenario is that we will be
  // closing the file and renaming it to a permanent name.  tmpfiles
  // automatically are deleted when they are closed.
  int prefix_len = prefix.length();
  static char mkstemp_hook[] = "XXXXXX";
  char* template_name = new char[prefix_len + sizeof(mkstemp_hook)];
  memcpy(template_name, prefix.data(), prefix_len);
  memcpy(template_name + prefix_len, mkstemp_hook, sizeof(mkstemp_hook));
#ifdef WIN32
  int fd = _mktemp_s(template_name, prefix_len + sizeof(mkstemp_hook));
#else
  int fd = mkstemp(template_name);
#endif  // WIN32
  OutputFile* output_file = NULL;
  if (fd < 0) {
    message_handler->Error(template_name, 0,
                           "opening temp file: %s", strerror(errno));
  } else {
#ifdef WIN32
    FILE* f = _fdopen(fd, "w");
    if (f == NULL) {
      _close(fd);
#else
    FILE* f = fdopen(fd, "w");
    if (f == NULL) {
      close(fd);
#endif

      // If we failed to open the temp file, silently clean it before returning.
      message_handler->Error(template_name, 0,
                             "re-opening temp file: %s", strerror(errno));
      NullMessageHandler null_message_handler;
      RemoveFile(template_name, &null_message_handler);
    } else {
      output_file = new StdioOutputFile(f, template_name, this);
    }
  }

  delete [] template_name;
  return output_file;
}


bool StdioFileSystem::RemoveFile(const char* filename,
                                 MessageHandler* handler) {
  bool ret = (remove(filename) == 0);
  if (!ret) {
    handler->Message(kError, "Failed to delete file %s: %s",
                     filename, strerror(errno));
  }
  return ret;
}

bool StdioFileSystem::RenameFileHelper(const char* old_file,
                                       const char* new_file,
                                       MessageHandler* handler) {
  bool ret = (rename(old_file, new_file) == 0);
  if (!ret) {
    handler->Message(kError, "Failed to rename file %s to %s: %s",
                     old_file, new_file, strerror(errno));
  }
  return ret;
}

bool StdioFileSystem::MakeDir(const char* path, MessageHandler* handler) {
#ifdef WIN32
  bool ret = (_mkdir(path) == 0);
#else
  // Mode 0777 makes the file use standard umask permissions.
  bool ret = (mkdir(path, 0777) == 0);
#endif  // WIN32
  if (!ret) {
    handler->Message(kError, "Failed to make directory %s: %s",
                     path, strerror(errno));
  }
  return ret;
}

bool StdioFileSystem::RemoveDir(const char* path, MessageHandler* handler) {
#ifdef WIN32
  bool ret = (_rmdir(path) == 0);
#else
  bool ret = (rmdir(path) == 0);
#endif  // WIN32
  if (!ret) {
    handler->Message(kError, "Failed to remove directory %s: %s",
                     path, strerror(errno));
  }
  return ret;
}

BoolOrError StdioFileSystem::Exists(const char* path, MessageHandler* handler) {
  struct stat statbuf;
  BoolOrError ret(stat(path, &statbuf) == 0);
  if (ret.is_false() && errno != ENOENT) {  // Not error if file doesn't exist.
    handler->Message(kError, "Failed to stat %s: %s",
                     path, strerror(errno));
    ret.set_error();
  }
  return ret;
}

BoolOrError StdioFileSystem::IsDir(const char* path, MessageHandler* handler) {
  struct stat statbuf;
  BoolOrError ret(false);
  if (stat(path, &statbuf) == 0) {
#ifdef WIN32
    ret.set((statbuf.st_mode & _S_IFDIR) != 0);
#else
    ret.set(S_ISDIR(statbuf.st_mode));
#endif  // WIN32
  } else if (errno != ENOENT) {  // Not an error if file doesn't exist.
    handler->Message(kError, "Failed to stat %s: %s",
                     path, strerror(errno));
    ret.set_error();
  }
  return ret;
}

bool StdioFileSystem::ListContents(const StringPiece& dir, StringVector* files,
                                   MessageHandler* handler) {
#ifdef WIN32
  const char kDirSeparator[] = "\\";
  std::string dir_string = dir.as_string();
  if (!dir.ends_with(kDirSeparator)) {
    dir_string.append(kDirSeparator);
  }
  std::string pattern = dir_string + "*";
  std::wstring wpattern = std::wstring(pattern.begin(), pattern.end());
  WIN32_FIND_DATA entry;
  HANDLE iter = FindFirstFile(wpattern.c_str(), &entry);
  if (iter == INVALID_HANDLE_VALUE) {
    handler->Error(dir_string.c_str(), 0,
                   "Failed to FindFirstFile: %s", strerror(errno));
    return false;
  }
  do {
    std::wstring wfilename(entry.cFileName);
    std::string filename(wfilename.begin(), wfilename.end());  // This is dodgy.
    if (filename != "." && filename != "..") {
      files->push_back(dir_string + filename);
    }
  } while (FindNextFile(iter, &entry) != 0);
  if (GetLastError() != ERROR_NO_MORE_FILES) {
    handler->Error(dir_string.c_str(), 0,
                   "Failed to FindNextFile: %s", strerror(errno));
    FindClose(iter);
    return false;
  }
  FindClose(iter);
  return true;
#else
  GoogleString dir_string = dir.as_string();
  EnsureEndsInSlash(&dir_string);
  const char* dirname = dir_string.c_str();
  DIR* mydir = opendir(dirname);
  if (mydir == NULL) {
      handler->Error(dirname, 0, "Failed to opendir: %s", strerror(errno));
    return false;
  } else {
    dirent* entry = NULL;
    dirent buffer;
    while (readdir_r(mydir, &buffer, &entry) == 0 && entry != NULL) {
      if ((strcmp(entry->d_name, ".") != 0) &&
          (strcmp(entry->d_name, "..") != 0)) {
        files->push_back(dir_string + entry->d_name);
      }
    }
    if (closedir(mydir) != 0) {
      handler->Error(dirname, 0, "Failed to closedir: %s", strerror(errno));
      return false;
    }
    return true;
  }
#endif  // WIN32
}

bool StdioFileSystem::Stat(const StringPiece& path, struct stat* statbuf,
                           MessageHandler* handler) const {
  const GoogleString path_string = path.as_string();
  const char* path_str = path_string.c_str();
  if (stat(path_str, statbuf) == 0) {
    return true;
  } else if (errno != ENOENT) {  // Not an error if file doesn't exist see #972.
    // https://github.com/pagespeed/ngx_pagespeed/issues/972
    handler->Message(kError, "Failed to stat %s: %s", path_str,
                     strerror(errno));
  }
  return false;
}

// TODO(abliss): there are some situations where this doesn't work
// -- e.g. if the filesystem is mounted noatime.  We should try to
// detect that and provide a workaround.
bool StdioFileSystem::Atime(const StringPiece& path, int64* timestamp_sec,
                            MessageHandler* handler) {
  struct stat statbuf;
  bool ret = Stat(path, &statbuf, handler);
  if (ret) {
    *timestamp_sec = statbuf.st_atime;
  }
  return ret;
}

bool StdioFileSystem::Mtime(const StringPiece& path, int64* timestamp_sec,
                            MessageHandler* handler) {
  struct stat statbuf;
  bool ret = Stat(path, &statbuf, handler);
  if (ret) {
    *timestamp_sec = statbuf.st_mtime;
  }
  return ret;
}

bool StdioFileSystem::Size(const StringPiece& path, int64* size,
                           MessageHandler* handler) const {
  struct stat statbuf;
  bool ret = Stat(path, &statbuf, handler);
  if (ret) {
#ifdef WIN32
    *size = statbuf.st_size;
#else
    *size = statbuf.st_blocks * kBlockSize;
#endif  // WIN32
  }
  return ret;
}

BoolOrError StdioFileSystem::TryLock(const StringPiece& lock_name,
                                     MessageHandler* handler) {
  const GoogleString lock_string = lock_name.as_string();
  const char* lock_str = lock_string.c_str();
  // POSIX mkdir is widely believed to be atomic, although I have
  // found no reliable documentation of this fact.
#ifdef WIN32
  if (_mkdir(lock_str) == 0) {
#else
  if (mkdir(lock_str, 0777) == 0) {
#endif  // WIN32
    return BoolOrError(true);
  } else if (errno == EEXIST) {
    return BoolOrError(false);
  } else {
    handler->Message(kError, "Failed to mkdir %s: %s",
                     lock_str, strerror(errno));
    return BoolOrError();
  }
}

BoolOrError StdioFileSystem::TryLockWithTimeout(const StringPiece& lock_name,
                                                int64 timeout_ms,
                                                const Timer* timer,
                                                MessageHandler* handler) {
  const GoogleString lock_string = lock_name.as_string();
  BoolOrError result = TryLock(lock_name, handler);
  if (result.is_true() || result.is_error()) {
    // We got the lock, or the lock is ungettable.
    return result;
  }
  int64 m_time_sec;
  if (!Mtime(lock_name, &m_time_sec, handler)) {
    // We can't stat the lockfile.
    return BoolOrError();
  }

  int64 now_us = timer->NowUs();
  int64 elapsed_since_lock_us = now_us - Timer::kSecondUs * m_time_sec;
  int64 timeout_us = Timer::kMsUs * timeout_ms;
  if (elapsed_since_lock_us <= timeout_us) {
    // The lock is held and timeout hasn't elapsed.
    return BoolOrError(false);
  }
  // Lock has timed out.  We have two options here:
  // 1) Leave the lock in its present state and assume we've taken ownership.
  //    This is kind to the file system, but causes lots of repeated work at
  //    timeout, as subsequent threads also see a timed-out lock.
  // 2) Force-unlock the lock and re-lock it.  This resets the timeout period,
  //    but is hard on the file system metadata log.
  const char* lock_str = lock_string.c_str();
  if (!Unlock(lock_name, handler)) {
    // We couldn't break the lock.  Maybe someone else beat us to it.
    // We optimistically forge ahead anyhow (1), since we know we've timed out.
    handler->Info(lock_str, 0,
                  "Breaking lock without reset! now-ctime=%d-%d > %d (sec)\n%s",
                  static_cast<int>(now_us / Timer::kSecondUs),
                  static_cast<int>(m_time_sec),
                  static_cast<int>(timeout_ms / Timer::kSecondMs),
                  StackTraceString().c_str());
    return BoolOrError(true);
  }
  handler->Info(lock_str, 0, "Broke lock! now-ctime=%d-%d > %d (sec)\n%s",
                static_cast<int>(now_us / Timer::kSecondUs),
                static_cast<int>(m_time_sec),
                static_cast<int>(timeout_ms / Timer::kSecondMs),
                StackTraceString().c_str());
  result = TryLock(lock_name, handler);
  if (!result.is_true()) {
    // Someone else grabbed the lock after we broke it.
    handler->Info(lock_str, 0, "Failed to take lock after breaking it!");
  }
  return result;
}

bool StdioFileSystem::BumpLockTimeout(const StringPiece& lock_name,
                                      MessageHandler* handler) {
  bool success = utime(lock_name.as_string().c_str(),
                       NULL /* update mtime to current time */) == 0;
  if (!success) {
    handler->Info(lock_name.as_string().c_str(), 0,
                  "Failed to bump lock: %s", strerror(errno));
  }
  return success;
}

bool StdioFileSystem::Unlock(const StringPiece& lock_name,
                             MessageHandler* handler) {
  const GoogleString lock_string = lock_name.as_string();
  const char* lock_str = lock_string.c_str();
#ifdef WIN32
  if (_rmdir(lock_str) == 0) {
#else
  if (rmdir(lock_str) == 0) {
#endif  // WIN32
    return true;
  } else {
    handler->Message(kError, "Failed to rmdir %s: %s",
                     lock_str, strerror(errno));
    return false;
  }
}

FileSystem::InputFile* StdioFileSystem::Stdin() {
  return new StdioInputFile(stdin, "stdin", this);
}

FileSystem::OutputFile* StdioFileSystem::Stdout() {
  return new StdioOutputFile(stdout, "stdout", this);
}

FileSystem::OutputFile* StdioFileSystem::Stderr() {
  return new StdioOutputFile(stderr, "stderr", this);
}

}  // namespace net_instaweb
