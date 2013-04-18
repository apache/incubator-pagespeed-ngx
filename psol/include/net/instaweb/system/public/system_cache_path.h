// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_CACHE_PATH_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_CACHE_PATH_H_

#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractSharedMem;
class CacheInterface;
class FileCache;
class FileSystemLockManager;
class MessageHandler;
class NamedLockManager;
class RewriteDriverFactory;
class SharedMemLockManager;
class SlowWorker;
class SystemRewriteOptions;

// The SystemCachePath encapsulates a cache-sharing model where a user specifies
// a file-cache path per virtual-host.  With each file-cache object we keep
// a locking mechanism and an optional per-process LRUCache.
class SystemCachePath {
 public:
  // CacheStats prefixes.
  static const char kFileCache[];
  static const char kLruCache[];

  SystemCachePath(const StringPiece& path,
                  const SystemRewriteOptions* config,
                  RewriteDriverFactory* factory,
                  AbstractSharedMem* shm_runtime);
  ~SystemCachePath();

  // Per-process in-memory LRU, with any stats/thread safety wrappers, or NULL.
  CacheInterface* lru_cache() { return lru_cache_; }

  // Per-machine file cache with any stats wrappers.
  CacheInterface* file_cache() { return file_cache_; }

  NamedLockManager* lock_manager() { return lock_manager_; }

  // See comments in SystemCaches for calling conventions on these.
  void RootInit();
  void ChildInit(SlowWorker* cache_clean_worker);
  void GlobalCleanup(MessageHandler* handler);  // only called in root process

 private:
  void FallBackToFileBasedLocking();
  GoogleString LockManagerSegmentName() const;

  GoogleString path_;

  RewriteDriverFactory* factory_;
  AbstractSharedMem* shm_runtime_;
  scoped_ptr<SharedMemLockManager> shared_mem_lock_manager_;
  scoped_ptr<FileSystemLockManager> file_system_lock_manager_;
  NamedLockManager* lock_manager_;
  FileCache* file_cache_backend_;  // owned by file_cache_
  CacheInterface* lru_cache_;
  CacheInterface* file_cache_;
};

// CACHE_STATISTICS is #ifdef'd to facilitate experiments with whether
// tracking the detailed stats & histograms has a QPS impact.  Set it
// to 0 to turn it off.
#define CACHE_STATISTICS 1

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_CACHE_PATH_H_
