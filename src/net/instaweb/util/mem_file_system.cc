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

#include "net/instaweb/util/public/mem_file_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include <string>
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class MemInputFile : public FileSystem::InputFile {
 public:
  MemInputFile(const StringPiece& filename, const std::string& contents)
      : contents_(contents),
        filename_(filename.data(), filename.size()),
        offset_(0) {
  }

  virtual bool Close(MessageHandler* message_handler) {
    offset_ = contents_.length();
    return true;
  }

  virtual const char* filename() { return filename_.c_str(); }

  virtual int Read(char* buf, int size, MessageHandler* message_handler) {
    if (size + offset_ > static_cast<int>(contents_.length())) {
      size = contents_.length() - offset_;
    }
    memcpy(buf, contents_.c_str() + offset_, size);
    offset_ += size;
    return size;
  }

 private:
  const std::string contents_;
  const std::string filename_;
  int offset_;

  DISALLOW_COPY_AND_ASSIGN(MemInputFile);
};


class MemOutputFile : public FileSystem::OutputFile {
 public:
  MemOutputFile(const StringPiece& filename, std::string* contents)
      : contents_(contents), filename_(filename.data(), filename.size()) {
    contents_->clear();
  }

  virtual bool Close(MessageHandler* message_handler) {
    Flush(message_handler);
    return true;
  }

  virtual const char* filename() { return filename_.c_str(); }

  virtual bool Flush(MessageHandler* message_handler) {
    contents_->append(written_);
    written_.clear();
    return true;
  }

  virtual bool SetWorldReadable(MessageHandler* message_handler) {
    return true;
  }

  virtual bool Write(const StringPiece& buf, MessageHandler* handler) {
    buf.AppendToString(&written_);
    return true;
  }

 private:
  std::string* contents_;
  const std::string filename_;
  std::string written_;

  DISALLOW_COPY_AND_ASSIGN(MemOutputFile);
};

MemFileSystem::~MemFileSystem() {
}

void MemFileSystem::UpdateAtime(const StringPiece& path) {
  if (atime_enabled_) {
    int64 now_us = timer_.NowUs();
    int64 now_s = now_us / Timer::kSecondUs;
    timer_.advance_us(Timer::kSecondUs);
    atime_map_[path.as_string()] = now_s;
  }
}

void MemFileSystem::Clear() {
  string_map_.clear();
}

BoolOrError MemFileSystem::Exists(const char* path, MessageHandler* handler) {
  StringMap::const_iterator iter = string_map_.find(path);
  return BoolOrError(iter != string_map_.end());
}

BoolOrError MemFileSystem::IsDir(const char* path, MessageHandler* handler) {
  return Exists(path, handler).is_true()
      ? BoolOrError(EndsInSlash(path)) : BoolOrError();
}

bool MemFileSystem::MakeDir(const char* path, MessageHandler* handler) {
  // We store directories as empty files with trailing slashes.
  std::string path_string = path;
  EnsureEndsInSlash(&path_string);
  string_map_[path_string] = "";
  UpdateAtime(path_string);
  return true;
}

FileSystem::InputFile* MemFileSystem::OpenInputFile(
    const char* filename, MessageHandler* message_handler) {
  if (!enabled_) {
    return NULL;
  }

  StringMap::const_iterator iter = string_map_.find(filename);
  if (iter == string_map_.end()) {
    message_handler->Error(filename, 0, "opening input file: %s",
                           "file not found");
    return NULL;
  } else {
    UpdateAtime(filename);
    return new MemInputFile(filename, iter->second);
  }
}

FileSystem::OutputFile* MemFileSystem::OpenOutputFileHelper(
    const char* filename, MessageHandler* message_handler) {
  UpdateAtime(filename);
  return new MemOutputFile(filename, &(string_map_[filename]));
}

FileSystem::OutputFile* MemFileSystem::OpenTempFileHelper(
    const StringPiece& prefix, MessageHandler* message_handler) {
  std::string filename = StringPrintf("tmpfile%d", temp_file_index_++);
  UpdateAtime(filename);
  return new MemOutputFile(filename, &string_map_[filename]);
}

bool MemFileSystem::RecursivelyMakeDir(const StringPiece& full_path_const,
                                       MessageHandler* handler) {
  // This is called to make sure that files can be written under the
  // named directory.  We don't have directories and files can be
  // written anywhere, so just return true.
  return true;
}

bool MemFileSystem::RemoveFile(const char* filename,
                               MessageHandler* handler) {
  atime_map_.erase(filename);
  return (string_map_.erase(filename) == 1);
}

bool MemFileSystem::RenameFileHelper(const char* old_file,
                                     const char* new_file,
                                     MessageHandler* handler) {
  UpdateAtime(new_file);
  if (strcmp(old_file, new_file) == 0) {
    handler->Error(old_file, 0, "Cannot move a file to itself");
    return false;
  }

  StringMap::iterator iter = string_map_.find(old_file);
  if (iter == string_map_.end()) {
    handler->Error(old_file, 0, "File not found");
    return false;
  }

  string_map_[new_file] = iter->second;
  string_map_.erase(iter);
  return true;
}

bool MemFileSystem::ListContents(const StringPiece& dir, StringVector* files,
                                 MessageHandler* handler) {
  std::string prefix = dir.as_string();
  EnsureEndsInSlash(&prefix);
  const size_t prefix_length = prefix.size();
  // We don't have directories, so we just list everything in the
  // filesystem that matches the prefix and doesn't have another
  // internal slash.
  for (StringMap::iterator it = string_map_.begin(), end = string_map_.end();
       it != end;
       it++) {
    const std::string& path = (*it).first;
    if ((0 == path.compare(0, prefix_length, prefix)) &&
        path.length() > prefix_length) {
      const size_t next_slash = path.find("/", prefix_length + 1);
      // Only want to list files without another slash, unless that
      // slash is the last char in the filename.
      if ((next_slash == std::string::npos)
          || (next_slash == path.length() - 1)) {
        files->push_back(path);
      }
    }
  }
  return true;
}

bool MemFileSystem::Atime(const StringPiece& path, int64* timestamp_sec,
                            MessageHandler* handler) {
  *timestamp_sec = atime_map_[path.as_string()];
  return true;
}

bool MemFileSystem::Size(const StringPiece& path, int64* size,
                         MessageHandler* handler) {
  const std::string path_string = path.as_string();
  const char* path_str = path_string.c_str();
  if (Exists(path_str, handler).is_true()) {
    *size = string_map_[path_string].size();
    return true;
  } else {
    return false;
  }
}

BoolOrError MemFileSystem::TryLock(const StringPiece& lock_name,
                                   MessageHandler* handler) {
  // Not actually threadsafe!  This is just for tests.
  if (lock_map_.count(lock_name.as_string()) != 0) {
    return BoolOrError(false);
  } else {
    lock_map_[lock_name.as_string()] = timer_.NowMs();
    return BoolOrError(true);
  }
}

BoolOrError MemFileSystem::TryLockWithTimeout(const StringPiece& lock_name,
                                              int64 timeout_ms,
                                              MessageHandler* handler) {
  // As above, not actually threadsafe (and quick-and-dirty rather than
  // efficient; efficiency requires map::find).
  std::string name = lock_name.as_string();
  int64 now = timer_.NowMs();
  if (lock_map_.count(name) != 0 &&
      now <= lock_map_[name] + timeout_ms) {
    return BoolOrError(false);
  } else {
    lock_map_[name] = timer_.NowMs();
    return BoolOrError(true);
  }
}

bool MemFileSystem::Unlock(const StringPiece& lock_name,
                           MessageHandler* handler) {
  return (lock_map_.erase(lock_name.as_string()) == 1);
}

}  // namespace net_instaweb
