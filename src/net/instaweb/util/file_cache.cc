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

// Author: lsong@google.com (Libo Song)

#include "net/instaweb/util/public/file_cache.h"

#include <algorithm>
#include <vector>
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/shared_string.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {  // For structs used only in Clean().

struct CompareByAtime {
 public:
  // Sort by ascending atime.
  bool operator()(const FileSystem::FileInfo& one,
                  const FileSystem::FileInfo& two) const {
    return one.atime_sec < two.atime_sec;
  }
};

}  // namespace for structs used only in Clean().

class FileCache::CacheCleanFunction : public Function {
 public:
  CacheCleanFunction(FileCache* cache, int64 next_clean_time_ms)
      : cache_(cache),
        next_clean_time_ms_(next_clean_time_ms) {}
  virtual ~CacheCleanFunction() {}
  virtual void Run() {
    cache_->last_conditional_clean_result_ =
        cache_->CleanWithLocking(next_clean_time_ms_);
  }

 private:
  FileCache* cache_;
  int64 next_clean_time_ms_;
  DISALLOW_COPY_AND_ASSIGN(CacheCleanFunction);
};

// Filenames for the next scheduled clean time and the lockfile.  In
// order to prevent these from colliding with actual cachefiles, they
// contain characters that our filename encoder would escape.
const char FileCache::kCleanTimeName[] = "!clean!time!";
const char FileCache::kCleanLockName[] = "!clean!lock!";

// TODO(abliss): remove policy from constructor; provide defaults here
// and setters below.
FileCache::FileCache(const GoogleString& path, FileSystem* file_system,
                     SlowWorker* worker,
                     FilenameEncoder* filename_encoder,
                     CachePolicy* policy,
                     MessageHandler* handler)
    : path_(path),
      file_system_(file_system),
      worker_(worker),
      filename_encoder_(filename_encoder),
      message_handler_(handler),
      cache_policy_(policy),
      path_length_limit_(file_system_->MaxPathLength(path)),
      clean_time_path_(path) {
  next_clean_ms_ = policy->timer->NowMs() + policy->clean_interval_ms / 2;
  EnsureEndsInSlash(&clean_time_path_);
  clean_time_path_ += kCleanTimeName;
}

FileCache::~FileCache() {
}

void FileCache::Get(const GoogleString& key, Callback* callback) {
  GoogleString filename;
  bool ret = EncodeFilename(key, &filename);
  if (ret) {
    GoogleString* buffer = callback->value()->get();

    // Suppress read errors.  Note that we want to show Write errors,
    // as they likely indicate a permissions or disk-space problem
    // which is best not eaten.  It's cheap enough to construct
    // a NullMessageHandler on the stack when we want one.
    NullMessageHandler null_handler;
    ret = file_system_->ReadFile(filename.c_str(), buffer, &null_handler);
  }
  ValidateAndReportResult(key, ret ? kAvailable : kNotFound, callback);
}

void FileCache::Put(const GoogleString& key, SharedString* value) {
  GoogleString filename;
  if (EncodeFilename(key, &filename)) {
    const GoogleString& buffer = **value;
    GoogleString temp_filename;
    if (file_system_->WriteTempFile(filename, buffer,
                                    &temp_filename, message_handler_)) {
      file_system_->RenameFile(temp_filename.c_str(), filename.c_str(),
                               message_handler_);
    }
  }
  CleanIfNeeded();
}

void FileCache::Delete(const GoogleString& key) {
  GoogleString filename;
  if (!EncodeFilename(key, &filename)) {
    return;
  }
  file_system_->RemoveFile(filename.c_str(), message_handler_);
  return;
}

bool FileCache::EncodeFilename(const GoogleString& key,
                               GoogleString* filename) {
  GoogleString prefix = path_;
  // TODO(abliss): unify and make explicit everyone's assumptions
  // about trailing slashes.
  EnsureEndsInSlash(&prefix);
  filename_encoder_->Encode(prefix, key, filename);

  // Make sure the length isn't too big for filesystem to handle; if it is
  // just name the object using a hash.
  if (static_cast<int>(filename->length()) > path_length_limit_) {
    filename_encoder_->Encode(prefix, cache_policy_->hasher->Hash(key),
                              filename);
  }

  return true;
}

bool FileCache::Clean(int64 target_size, int64 target_inode_count) {
  // TODO(jud): this function can delete .lock and .outputlock files, is this
  // problematic?
  message_handler_->Message(kInfo,
                            "Checking cache size against target %s and inode "
                            "count against target %s",
                            Integer64ToString(target_size).c_str(),
                            Integer64ToString(target_inode_count).c_str());

  bool everything_ok = true;

  // Get the contents of the cache
  FileSystem::DirInfo dir_info;
  file_system_->GetDirInfo(path_, &dir_info, message_handler_);

  // Check to see if cache size or inode count exceeds our limits.
  // target_inode_count of 0 indicates no inode limit.
  // TODO(jmarantz): gcc 4.1 warns about double/int64 comparisons here,
  // but this really should be factored into a settable member var.
  int64 size_clean_threshold = (target_size * 5) / 4;
  int64 inode_count_clean_threshold = (target_inode_count * 5) / 4;
  int64 cache_size = dir_info.size_bytes;
  int64 cache_inode_count = dir_info.inode_count;
  if (cache_size < size_clean_threshold &&
      (target_inode_count == 0 ||
       cache_inode_count < inode_count_clean_threshold)) {
    message_handler_->Message(kInfo,
                              "File cache size is %s and contains %s inodes; "
                              "no cleanup needed.",
                              Integer64ToString(cache_size).c_str(),
                              Integer64ToString(cache_inode_count).c_str());
    return true;
  }

  message_handler_->Message(kInfo,
                            "File cache size is %s and contains %s inodes; "
                            "beginning cleanup.",
                            Integer64ToString(cache_size).c_str(),
                            Integer64ToString(cache_inode_count).c_str());

  // Remove empty directories.
  StringVector::iterator it;
  for (it = dir_info.empty_dirs.begin(); it != dir_info.empty_dirs.end();
       ++it) {
    everything_ok &= file_system_->RemoveDir(it->c_str(), message_handler_);
    // Decrement cache_inode_count even if RemoveDir failed. This is likely
    // because the directory has already been removed.
    --cache_inode_count;
  }

  // Save original cache size to track how many bytes we've cleaned up.
  int64 orig_cache_size = cache_size;

  // Sort files by atime in ascending order to remove oldest files first.
  std::sort(dir_info.files.begin(), dir_info.files.end(), CompareByAtime());

  // Set the target size to clean to.
  // TODO(jmarantz): gcc 4.1 warns about double/int64 comparisons here,
  // but this really should be factored into a settable member var.
  target_size = (target_size * 3) / 4;
  target_inode_count = (target_inode_count * 3) / 4;

  // Delete files until we are under our targets.
  std::vector<FileSystem::FileInfo>::iterator file_itr = dir_info.files.begin();
  while (file_itr != dir_info.files.end() &&
         (cache_size > target_size ||
          (target_inode_count != 0 &&
           cache_inode_count > target_inode_count))) {
    FileSystem::FileInfo file = *file_itr;
    ++file_itr;
    // Don't clean the clean_time file! It ought to be the newest file (and very
    // small) so it would normally not be deleted anyway. But on some systems
    // (e.g. mounted noatime?) it was getting deleted.
    if (clean_time_path_.compare(file.name) == 0) {
      continue;
    }
    cache_size -= file.size_bytes;
    // Decrement inode_count even if RemoveFile fails. This is likely because
    // the file has already been removed.
    --cache_inode_count;
    everything_ok &= file_system_->RemoveFile(file.name.c_str(),
                                              message_handler_);
  }

  int64 bytes_freed = orig_cache_size - cache_size;
  message_handler_->Message(kInfo,
                            "File cache cleanup complete; freed %s bytes",
                            Integer64ToString(bytes_freed).c_str());
  return everything_ok;
}

bool FileCache::CleanWithLocking(int64 next_clean_time_ms) {
  bool to_return = false;

  GoogleString lock_name(path_);
  EnsureEndsInSlash(&lock_name);
  lock_name += kCleanLockName;
  if (file_system_->TryLockWithTimeout(
      lock_name, Timer::kHourMs, message_handler_).is_true()) {
    // Update the timestamp file..
    next_clean_ms_ = next_clean_time_ms;
    file_system_->WriteFile(clean_time_path_.c_str(),
                            Integer64ToString(next_clean_time_ms),
                            message_handler_);

    // Now actually clean.
    to_return = Clean(cache_policy_->target_size,
                      cache_policy_->target_inode_count);
    file_system_->Unlock(lock_name, message_handler_);
  }
  return to_return;
}

bool FileCache::ShouldClean(int64* suggested_next_clean_time_ms) {
  bool to_return = false;
  const int64 now_ms = cache_policy_->timer->NowMs();
  if (now_ms < next_clean_ms_) {
    *suggested_next_clean_time_ms = next_clean_ms_;  // No change yet.
    return false;
  }

  GoogleString clean_time_str;
  int64 clean_time_ms = 0;
  int64 new_clean_time_ms = now_ms + cache_policy_->clean_interval_ms;
  NullMessageHandler null_handler;
  if (file_system_->ReadFile(clean_time_path_.c_str(), &clean_time_str,
                              &null_handler)) {
    StringToInt64(clean_time_str, &clean_time_ms);
  } else {
    message_handler_->Message(
        kWarning, "Failed to read cache clean timestamp %s. "
        " Doing an extra cache clean to be safe.", clean_time_path_.c_str());
  }

  // If the "clean time" written in the file is older than now, we
  // clean.
  if (clean_time_ms < now_ms) {
    message_handler_->Message(
        kInfo, "Need to check cache size against target %s",
        Integer64ToString(cache_policy_->target_size).c_str());
    to_return = true;
  }
  // If the "clean time" is later than now plus one interval, something
  // went wrong (like the system clock moving backwards or the file
  // getting corrupt) so we clean and reset it.
  if (clean_time_ms > new_clean_time_ms) {
    message_handler_->Message(kError,
                              "Next scheduled file cache clean time %s"
                              " is implausibly remote.  Cleaning now.",
                              Integer64ToString(clean_time_ms).c_str());
    to_return = true;
  }

  *suggested_next_clean_time_ms = new_clean_time_ms;
  return to_return;
}

void FileCache::CleanIfNeeded() {
  if (worker_ != NULL) {
    int64 suggested_next_clean_time_ms;
    last_conditional_clean_result_ = false;
    if (ShouldClean(&suggested_next_clean_time_ms)) {
      worker_->Start();
      worker_->RunIfNotBusy(
          new CacheCleanFunction(this, suggested_next_clean_time_ms));
    } else {
      next_clean_ms_ = suggested_next_clean_time_ms;
    }
  }
}

}  // namespace net_instaweb
