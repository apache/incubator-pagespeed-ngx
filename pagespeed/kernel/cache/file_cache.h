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

#ifndef PAGESPEED_KERNEL_CACHE_FILE_CACHE_H_
#define PAGESPEED_KERNEL_CACHE_FILE_CACHE_H_

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

class Hasher;
class MessageHandler;
class SlowWorker;
class Statistics;
class Timer;
class Variable;

// Simple C++ implementation of file cache.
class FileCache : public CacheInterface {
 public:
  struct CachePolicy {
    CachePolicy(Timer* timer, Hasher* hasher, int64 clean_interval_ms,
                int64 target_size_bytes, int64 target_inode_count)
        : timer(timer), hasher(hasher), clean_interval_ms(clean_interval_ms),
          target_size_bytes(target_size_bytes),
          target_inode_count(target_inode_count) {}
    const Timer* timer;
    const Hasher* hasher;
    int64 clean_interval_ms;
    int64 target_size_bytes;
    int64 target_inode_count;
    bool cleaning_enabled() { return clean_interval_ms != kDisableCleaning; }
   private:
    DISALLOW_COPY_AND_ASSIGN(CachePolicy);
  };

  FileCache(const GoogleString& path, FileSystem* file_system,
            ThreadSystem* thread_system, SlowWorker* worker,
            CachePolicy* policy, Statistics* stats, MessageHandler* handler);
  virtual ~FileCache();

  static void InitStats(Statistics* statistics);

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, const SharedString& value);
  virtual void Delete(const GoogleString& key);
  void set_worker(SlowWorker* worker) { worker_ = worker; }
  SlowWorker* worker() { return worker_; }

  static GoogleString FormatName() { return "FileCache"; }
  virtual GoogleString Name() const { return FormatName(); }

  virtual bool IsBlocking() const { return true; }
  virtual bool IsHealthy() const { return true; }
  virtual void ShutDown() {}  // TODO(jmarantz): implement.

  const CachePolicy* cache_policy() const { return cache_policy_.get(); }
  CachePolicy* mutable_cache_policy() { return cache_policy_.get(); }
  const GoogleString& path() const { return path_; }

  // Variable names.
  static const char kBytesFreedInCleanup[];
  // Number of times we actually cleaned cache because usage was high enough.
  static const char kCleanups[];
  // Number of times we checked disk usage in preparation from cleanup.
  static const char kDiskChecks[];
  // Files evicted from cache during cleanup.
  static const char kEvictions[];
  // Number of times we didn't kick off cleaning because a previous cleaning run
  // was still going.
  static const char kSkippedCleanups[];
  // Number of times we scanned the cache to see if it needed cleaning.
  static const char kStartedCleanups[];
  static const char kWriteErrors[];

  // What to set clean_interval_ms to in order to disable cleaning.  This needs
  // to be -1, because that's what we have in our public documentation.
  static const int kDisableCleaning = -1;

 private:
  class CacheCleanFunction;
  friend class FileCacheTest;
  friend class CacheCleanFunction;

  // Attempts to clean the cache. Returns false if we failed and the cache still
  // needs to be cleaned. Returns true if everything's fine. This may take a
  // while. It's OK for others to write and read from the cache while this is
  // going on, but try to avoid Cleaning from two threads at the same time. A
  // target_inode_count of 0 means no inode limit is applied.
  bool Clean(int64 target_size_bytes, int64 target_inode_count);

  // Clean the cache, taking care of interprocess locking, as well as timestamp
  // update.
  void CleanWithLocking(int64 next_clean_time_ms) LOCKS_EXCLUDED(mutex_);

  // Return true if we need to clean the cache, and updates the next
  // clean time if cleaning does not need to be run.
  bool ShouldClean(int64* suggested_next_clean_time_ms) LOCKS_EXCLUDED(mutex_);

  // Check to see if it's time to clean the cache, and if so ask
  // worker_ to do it in a thread if it's not busy. Stores 'true' into
  // last_conditional_clean_result_ if it actually cleaned successfully,
  // false otherwise.
  void CleanIfNeeded() LOCKS_EXCLUDED(mutex_);

  bool EncodeFilename(const GoogleString& key, GoogleString* filename);

  const GoogleString path_;
  FileSystem* file_system_;
  SlowWorker* worker_;
  MessageHandler* message_handler_;
  const scoped_ptr<CachePolicy> cache_policy_;
  scoped_ptr<AbstractMutex> mutex_;
  int64 next_clean_ms_ GUARDED_BY(mutex_);
  int path_length_limit_;  // Maximum total length of path file_system_ supports
  // The full paths to our cleanup timestamp and lock files.
  GoogleString clean_time_path_;
  GoogleString clean_lock_path_;
  // If set, we use this instead of the default LockBumpingProgressNotifier.  We
  // do not take ownership.
  FileSystem::ProgressNotifier* notifier_for_tests_;

  Variable* disk_checks_;
  Variable* cleanups_;
  Variable* evictions_;
  Variable* bytes_freed_in_cleanup_;
  Variable* skipped_cleanups_;
  Variable* started_cleanups_;
  Variable* write_errors_;

  // The filename where we keep the next scheduled cleanup time in seconds.
  static const char kCleanTimeName[];
  // The name of the global mutex protecting reads and writes to that file.
  static const char kCleanLockName[];

  // How long a cache cleaner has to go without bumping it's lock before it
  // might be usurped.
  static const int kLockTimeoutMs;

  DISALLOW_COPY_AND_ASSIGN(FileCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_FILE_CACHE_H_
