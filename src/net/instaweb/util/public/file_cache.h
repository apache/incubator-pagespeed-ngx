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
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {
class FileSystem;
class FilenameEncoder;
class MessageHandler;
class SharedString;
class SlowWorker;
class Timer;

// Simple C++ implementation of file cache.
class FileCache : public CacheInterface {
 public:

  struct CachePolicy {
    CachePolicy(Timer* timer, int64 clean_interval_ms, int64 target_size)
        : timer(timer), clean_interval_ms(clean_interval_ms),
          target_size(target_size) {}
    const Timer* timer;
    const int64 clean_interval_ms;
    const int64 target_size;
   private:
    DISALLOW_COPY_AND_ASSIGN(CachePolicy);
  };

  FileCache(const GoogleString& path, FileSystem* file_system,
            SlowWorker* worker, FilenameEncoder* filename_encoder,
            CachePolicy* policy, MessageHandler* handler);
  virtual ~FileCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);

 private:
  class CacheCleanClosure;
  friend class FileCacheTest;
  friend class CacheCleanClosure;

  // Attempts to clean the cache.  Returns false if we failed and the
  // cache still needs to be cleaned.  Returns true if everything's
  // fine.  This may take a while.  It's OK for others to write and
  // read from the cache while this is going on, but try to avoid
  // Cleaning from two threads at the same time.
  bool Clean(int64 target_size);

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
  // The full path to our cleanup timestamp file.
  GoogleString clean_time_path_;
  bool last_conditional_clean_result_;

  // The filename where we keep the next scheduled cleanup time in seconds.
  static const char kCleanTimeName[];
  // The name of the global mutex protecting reads and writes to that file.
  static const char kCleanLockName[];
  DISALLOW_COPY_AND_ASSIGN(FileCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FILE_CACHE_H_
