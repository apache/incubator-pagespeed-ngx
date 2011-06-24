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
            FilenameEncoder* filename_encoder, CachePolicy* policy,
            MessageHandler* handler);
  virtual ~FileCache();

  virtual void Get(const GoogleString& key, Callback* callback);
  virtual void Put(const GoogleString& key, SharedString* value);
  virtual void Delete(const GoogleString& key);

  // Attempts to clean the cache.  Returns false if we failed and the
  // cache still needs to be cleaned.  Returns true if everything's
  // fine.  This may take a while.  It's OK for others to write and
  // read from the cache while this is going on, but try to avoid
  // Cleaning from two threads at the same time.
  bool Clean(int64 target_size);
  // Check to see if it's time to clean the cache, and if so start
  // cleaning.  Return true if we cleaned, false if we didn't.
  bool CheckClean();

 private:
  friend class FileCacheTest;

  bool EncodeFilename(const GoogleString& key, GoogleString* filename);

  const GoogleString path_;
  FileSystem* file_system_;
  FilenameEncoder* filename_encoder_;
  MessageHandler* message_handler_;
  const scoped_ptr<CachePolicy> cache_policy_;
  int64 next_clean_ms_;
  // The full path to our cleanup timestamp file.
  GoogleString clean_time_path_;

  // The filename where we keep the next scheduled cleanup time in seconds.
  static const char kCleanTimeName[];
  // The name of the global mutex protecting reads and writes to that file.
  static const char kCleanLockName[];
  DISALLOW_COPY_AND_ASSIGN(FileCache);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FILE_CACHE_H_
