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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_FILE_CACHE_H_
#define NET_INSTAWEB_UTIL_PUBLIC_FILE_CACHE_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class FileSystem;
class FilenameEncoder;
class Hasher;
class MessageHandler;
class SharedString;
class SlowWorker;
class Statistics;
class Timer;
class Variable;

// Simple C++ implementation of file cache.
class FileCache : public CacheInterface {
 public:
  struct CachePolicy {
    CachePolicy(Timer* timer, Hasher* hasher, int64 clean_interval_ms,
                int64 target_size, int64 target_inode_count)
        : timer(timer), hasher(hasher), clean_interval_ms(clean_interval_ms),
          target_size(target_size), target_inode_count(target_inode_count) {}
    const Timer* timer;
    const Hasher* hasher;
    const int64 clean_interval_ms;
    const int64 target_size;
    const int64 target_inode_count;
   private:
    DISALLOW_COPY_AND_ASSIGN(CachePolicy);
  };

  FileCache(const GoogleString& path, FileSystem* file_system,
            SlowWorker* worker, FilenameEncoder* filename_encoder,
            CachePolicy* policy, Statistics* stats, MessageHandler* handler);
  virtual ~FileCache();

  static void InitStats(Statistics* statistics);

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);
  void set_worker(SlowWorker* worker) { worker_ = worker; }
  SlowWorker* worker() { return worker_; }

  virtual const char* Name() const { return "FileCache"; }
  virtual bool IsBlocking() const { return true; }
  virtual bool IsHealthy() const { return true; }
  virtual void ShutDown() {}  // TODO(jmarantz): implement.

  const CachePolicy* cache_policy() const { return cache_policy_.get(); }
  const GoogleString& path() const { return path_; }

  // Variable names.
  // Number of times we checked disk usage in preparation from cleanup.
  static const char kDiskChecks[];
  // Number of times we actually cleaned cache because usage was high enough.
  static const char kCleanups[];
  // Files evicted from cache during cleanup.
  static const char kEvictions[];
  static const char kBytesFreedInCleanup[];

 private:
  class CacheCleanFunction;
  friend class FileCacheTest;
  friend class CacheCleanFunction;

  // Attempts to clean the cache. Returns false if we failed and the cache still
  // needs to be cleaned. Returns true if everything's fine. This may take a
  // while. It's OK for others to write and read from the cache while this is
  // going on, but try to avoid Cleaning from two threads at the same time. A
  // target_inode_count of 0 means no inode limit is applied.
  bool Clean(int64 target_size, int64 target_inode_count);

  // Clean the cache, taking care of interprocess locking, as well as
  // timestamp update. Returns true if the cache was actually cleaned.
  bool CleanWithLocking(int64 next_clean_time_ms);

  // Return true if we need to clean the cache, and outputs the next
  // clean time to use should we follow its advice.
  bool ShouldClean(int64* suggested_next_clean_time_ms);

  // Check to see if it's time to clean the cache, and if so ask
  // worker_ to do it in a thread if it's not busy. Stores 'true' into
  // last_conditional_clean_result_ if it actually cleaned successfully,
  // false otherwise.
  void CleanIfNeeded();

  bool EncodeFilename(const GoogleString& key, GoogleString* filename);

  const GoogleString path_;
  FileSystem* file_system_;
  SlowWorker* worker_;
  FilenameEncoder* filename_encoder_;
  MessageHandler* message_handler_;
  const scoped_ptr<CachePolicy> cache_policy_;
  int64 next_clean_ms_;
  int path_length_limit_;  // Maximum total length of path file_system_ supports
  // The full paths to our cleanup timestamp and lock files.
  GoogleString clean_time_path_;
  GoogleString clean_lock_path_;
  bool last_conditional_clean_result_;

  Variable* disk_checks_;
  Variable* cleanups_;
  Variable* evictions_;
  Variable* bytes_freed_in_cleanup_;

  // The filename where we keep the next scheduled cleanup time in seconds.
  static const char kCleanTimeName[];
  // The name of the global mutex protecting reads and writes to that file.
  static const char kCleanLockName[];

  DISALLOW_COPY_AND_ASSIGN(FileCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FILE_CACHE_H_
