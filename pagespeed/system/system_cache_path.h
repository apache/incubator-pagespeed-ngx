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

#ifndef PAGESPEED_SYSTEM_SYSTEM_CACHE_PATH_H_
#define PAGESPEED_SYSTEM_SYSTEM_CACHE_PATH_H_

#include <set>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/util/copy_on_write.h"

namespace net_instaweb {

class AbstractMutex;
class AbstractSharedMem;
class CacheInterface;
class FileCache;
class FileSystemLockManager;
class MessageHandler;
class NamedLockManager;
class PurgeContext;
class PurgeSet;
class RewriteDriverFactory;
class SharedMemLockManager;
class SlowWorker;
class SystemServerContext;
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

  // Computes a key suitable for building a map to help share common cache
  // objects between vhosts.  This key is given to the constructor as 'path'.
  static GoogleString CachePath(SystemRewriteOptions* config);

  // Per-process in-memory LRU, with any stats/thread safety wrappers, or NULL.
  CacheInterface* lru_cache() { return lru_cache_; }

  // Per-machine file cache with any stats wrappers.
  CacheInterface* file_cache() { return file_cache_; }

  // Access to backend for testing.  Do not use this directly in production
  // as it lacks statistics wrappers, etc.
  FileCache* file_cache_backend() { return file_cache_backend_; }
  NamedLockManager* lock_manager() { return lock_manager_; }

  // See comments in SystemCaches for calling conventions on these.
  void RootInit();
  void ChildInit(SlowWorker* cache_clean_worker);
  void GlobalCleanup(MessageHandler* handler);  // only called in root process

  // When there are multiple configurations which specify the same cache
  // path, we must merge the other settings: the cleaning interval, size, and
  // inode count.
  void MergeConfig(const SystemRewriteOptions* config);

  // Associates a ServerContext with this CachePath, enabling cache purges
  // to propagate into the ServerContext's global options.
  void AddServerContext(SystemServerContext* server_context);

  // Disassociates a server context with this CachePath -- used on shutdown.
  void RemoveServerContext(SystemServerContext* server_context);

  // Entry-point for flushing the cache, either via the legacy method
  // of "touch .../cache.flush" or the newer method of purging via
  // /pagespeed_admin/cache?purge=... or a PURGE method, depending on whether
  // the EnableCachePurge method is set.
  void FlushCacheIfNecessary();

  PurgeContext* purge_context() { return purge_context_.get(); }

 private:
  typedef std::set<SystemServerContext*> ServerContextSet;

  void FallBackToFileBasedLocking();
  GoogleString LockManagerSegmentName() const;

  // Merge a value taken from a config file against the value already
  // initialized in a cache policy, reporting a Warning if they were
  // explicitly set and have conflicting values.  Whenever one of the
  // values was taken from the options defaults, we select the explicit
  // one without issuing a warning.
  //
  // For the interval, we take the minimum of the two values
  // (take_larger==false), and for the sizes we take the larger
  // (take_larger==true).
  //
  // If necessary, *policy_value is updated with the resolved value,
  // which is computed from the old *policy_value and config_value.
  //
  // 'name' is used in a warning message printed whenever resolution was
  // required.
  void MergeEntries(int64 config_value, bool config_was_set,
                    bool take_larger,
                    const char* name,
                    int64* policy_value, bool* has_explicit_policy);


  // Transmits cache-purge-set updates to all live server contexts.
  void UpdateCachePurgeSet(const CopyOnWrite<PurgeSet>& purge_set);

  GoogleString path_;

  RewriteDriverFactory* factory_;
  AbstractSharedMem* shm_runtime_;
  scoped_ptr<SharedMemLockManager> shared_mem_lock_manager_;
  scoped_ptr<FileSystemLockManager> file_system_lock_manager_;
  NamedLockManager* lock_manager_;
  FileCache* file_cache_backend_;  // owned by file_cache_
  CacheInterface* lru_cache_;
  CacheInterface* file_cache_;
  GoogleString cache_flush_filename_;
  bool unplugged_;
  bool enable_cache_purge_;
  bool clean_interval_explicitly_set_;
  bool clean_size_explicitly_set_;
  bool clean_inode_limit_explicitly_set_;

  scoped_ptr<PurgeContext> purge_context_;

  scoped_ptr<AbstractMutex> mutex_;
  ServerContextSet server_context_set_ GUARDED_BY(mutex_);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_SYSTEM_CACHE_PATH_H_
