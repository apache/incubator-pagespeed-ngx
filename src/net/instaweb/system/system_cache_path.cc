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

#include "net/instaweb/system/public/system_cache_path.h"

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/cache_stats.h"
#include "net/instaweb/util/public/file_cache.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/shared_mem_lock_manager.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/threadsafe_cache.h"

namespace net_instaweb {

const char SystemCachePath::kFileCache[] = "file_cache";
const char SystemCachePath::kLruCache[] = "lru_cache";

// The SystemCachePath encapsulates a cache-sharing model where a user specifies
// a file-cache path per virtual-host.  With each file-cache object we keep
// a locking mechanism and an optional per-process LRUCache.
SystemCachePath::SystemCachePath(const StringPiece& path,
                                 const SystemRewriteOptions* config,
                                 RewriteDriverFactory* factory,
                                 AbstractSharedMem* shm_runtime)
    : path_(path.data(), path.size()),
      factory_(factory),
      shm_runtime_(shm_runtime),
      lock_manager_(NULL),
      file_cache_backend_(NULL),
      lru_cache_(NULL),
      file_cache_(NULL) {
  if (config->use_shared_mem_locking()) {
    shared_mem_lock_manager_.reset(new SharedMemLockManager(
        shm_runtime, LockManagerSegmentName(),
        factory->scheduler(), factory->hasher(), factory->message_handler()));
    lock_manager_ = shared_mem_lock_manager_.get();
  } else {
    FallBackToFileBasedLocking();
  }

  FileCache::CachePolicy* policy = new FileCache::CachePolicy(
      factory->timer(),
      factory->hasher(),
      config->file_cache_clean_interval_ms(),
      config->file_cache_clean_size_kb() * 1024,
      config->file_cache_clean_inode_limit());
  file_cache_backend_ = new FileCache(
      config->file_cache_path(), factory->file_system(), NULL,
      factory->filename_encoder(), policy, factory->statistics(),
      factory->message_handler());
  factory->TakeOwnership(file_cache_backend_);
  file_cache_ = new CacheStats(kFileCache, file_cache_backend_,
                               factory->timer(), factory->statistics());
  factory->TakeOwnership(file_cache_);

  if (config->lru_cache_kb_per_process() != 0) {
    LRUCache* lru_cache = new LRUCache(
        config->lru_cache_kb_per_process() * 1024);
    factory->TakeOwnership(lru_cache);

    // We only add the threadsafe-wrapper to the LRUCache.  The FileCache
    // is naturally thread-safe because it's got no writable member variables.
    // And surrounding that slower-running class with a mutex would likely
    // cause contention.
    ThreadsafeCache* ts_cache =
        new ThreadsafeCache(lru_cache, factory->thread_system()->NewMutex());
    factory->TakeOwnership(ts_cache);
#if CACHE_STATISTICS
    lru_cache_ = new CacheStats(kLruCache, ts_cache, factory->timer(),
                                factory->statistics());
    factory->TakeOwnership(lru_cache_);
#else
    lru_cache_ = ts_cache;
#endif
  }
}

SystemCachePath::~SystemCachePath() {
}

void SystemCachePath::RootInit() {
  factory_->message_handler()->Message(
      kInfo, "Initializing shared memory for path: %s.", path_.c_str());
  if ((shared_mem_lock_manager_.get() != NULL) &&
      !shared_mem_lock_manager_->Initialize()) {
    FallBackToFileBasedLocking();
  }
}

void SystemCachePath::ChildInit(SlowWorker* cache_clean_worker) {
  factory_->message_handler()->Message(
      kInfo, "Reusing shared memory for path: %s.", path_.c_str());
  if ((shared_mem_lock_manager_.get() != NULL) &&
      !shared_mem_lock_manager_->Attach()) {
    FallBackToFileBasedLocking();
  }
  if (file_cache_backend_ != NULL) {
    file_cache_backend_->set_worker(cache_clean_worker);
  }
}

void SystemCachePath::GlobalCleanup(MessageHandler* handler) {
  if (shared_mem_lock_manager_.get() != NULL) {
    shared_mem_lock_manager_->GlobalCleanup(
        shm_runtime_, LockManagerSegmentName(), handler);
  }
}

void SystemCachePath::FallBackToFileBasedLocking() {
  if ((shared_mem_lock_manager_.get() != NULL) || (lock_manager_ == NULL)) {
    shared_mem_lock_manager_.reset(NULL);
    file_system_lock_manager_.reset(new FileSystemLockManager(
        factory_->file_system(), path_,
        factory_->scheduler(), factory_->message_handler()));
    lock_manager_ = file_system_lock_manager_.get();
  }
}

GoogleString SystemCachePath::LockManagerSegmentName() const {
  return StrCat(path_, "/named_locks");
}

}  // namespace net_instaweb
