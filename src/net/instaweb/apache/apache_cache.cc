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

#include "net/instaweb/apache/apache_cache.h"
#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/apache/apr_mem_cache.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/util/public/file_cache.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/shared_mem_lock_manager.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/write_through_cache.h"

// We have an experimental implementations of a memcached interface based
// on apr_memcache.  So far it looks like apr_memcache is reasonably robust,
// though its performance might not be optimal because:
//    1. It requires we create/delete a pool on every fetch.
//    2. It lacks an asynchronous interface.
// An alternate implementation based on libmemcached appears
// non-robust under load and needs investigation.  apr_memcache
// appears robust, but load-tests at around 1500 qps, as opposed to
// 3500 qps for tmpfs.  However we are not using multi-key gets yet in
// our implementation, and it's plausible the memcached performance will
// improve once we implement that.

// TODO(jmarantz): remove the ifdefs and add pagespeed.conf configuration and
// documentation.
#include "ap_mpm.h"

namespace net_instaweb {

class Timer;

// The ApacheCache encapsulates a cache-sharing model where a user specifies
// a file-cache path per virtual-host.  With each file-cache object we keep
// a locking mechanism and an optional per-process LRUCache.
ApacheCache::ApacheCache(const StringPiece& path,
                         const ApacheConfig& config,
                         ApacheRewriteDriverFactory* factory)
    : path_(path.data(), path.size()),
      factory_(factory),
      lock_manager_(NULL),
      file_cache_(NULL),
      mem_cache_(NULL),
      l2_cache_(NULL) {
  if (config.use_shared_mem_locking()) {
    shared_mem_lock_manager_.reset(new SharedMemLockManager(
        factory->shared_mem_runtime(), StrCat(path, "/named_locks"),
        factory->scheduler(), factory->hasher(), factory->message_handler()));
    lock_manager_ = shared_mem_lock_manager_.get();
  } else {
    FallBackToFileBasedLocking();
  }

  const GoogleString& memcached_servers = config.memcached_servers();
  if (!memcached_servers.empty()) {
    // Note that the thread_limit must be queried from this file,
    // which is built with an include-path that includes the server.
    // We can't call ap_mpm_query from apr_mem_cache.cc without
    // forcing a server dependency and making it harder to run it in
    // unit tests.
    int thread_limit;
    ap_mpm_query(AP_MPMQ_HARD_LIMIT_THREADS, &thread_limit);
    thread_limit += factory->num_rewrite_threads() +
        factory->num_expensive_rewrite_threads();
    mem_cache_ = new AprMemCache(memcached_servers, thread_limit,
                                 factory->hasher(), factory->message_handler());
    if (!mem_cache_->valid_server_spec()) {
      abort();  // TODO(jmarantz): is there a better way to exit?
    }
    l2_cache_ = mem_cache_;  // apr_memcache is threadsafe.  If not we'd do:
    // l2_cache_ = new ThreadsafeCache(mem_cache_,
    //                                 factory->thread_system()->NewMutex());
  } else {
    FileCache::CachePolicy* policy = new FileCache::CachePolicy(
        factory->timer(),
        factory->hasher(),
        config.file_cache_clean_interval_ms(),
        config.file_cache_clean_size_kb() * 1024);
    file_cache_ = new FileCache(
        config.file_cache_path(), factory->file_system(), NULL,
        factory->filename_encoder(), policy, factory->message_handler());
    l2_cache_ = file_cache_;
  }
  if (config.lru_cache_kb_per_process() != 0) {
    LRUCache* lru_cache = new LRUCache(
        config.lru_cache_kb_per_process() * 1024);

    // We only add the threadsafe-wrapper to the LRUCache.  The FileCache
    // is naturally thread-safe because it's got no writable member variables.
    // And surrounding that slower-running class with a mutex would likely
    // cause contention.
    ThreadsafeCache* ts_cache =
        new ThreadsafeCache(lru_cache, factory->thread_system()->NewMutex());
    WriteThroughCache* write_through_cache =
        new WriteThroughCache(ts_cache, l2_cache_);
    // By default, WriteThroughCache does not limit the size of entries going
    // into its front cache.
    if (config.lru_cache_byte_limit() != 0) {
      write_through_cache->set_cache1_limit(config.lru_cache_byte_limit());
    }
    cache_.reset(write_through_cache);
  } else {
    cache_.reset(l2_cache_);
  }
  http_cache_.reset(new HTTPCache(cache_.get(), factory->timer(),
                                  factory->hasher(), factory->statistics()));
  page_property_cache_.reset(factory->MakePropertyCache(
      PropertyCache::kPagePropertyCacheKeyPrefix, l2_cache_));
  client_property_cache_.reset(factory->MakePropertyCache(
      PropertyCache::kClientPropertyCacheKeyPrefix, l2_cache_));
}

ApacheCache::~ApacheCache() {
}

void ApacheCache::RootInit() {
  factory_->message_handler()->Message(
      kInfo, "Initializing shared memory for path: %s.", path_.c_str());
  if ((shared_mem_lock_manager_.get() != NULL) &&
      !shared_mem_lock_manager_->Initialize()) {
    FallBackToFileBasedLocking();
  }
}

void ApacheCache::ChildInit() {
  factory_->message_handler()->Message(
      kInfo, "Reusing shared memory for path: %s.", path_.c_str());
  if ((shared_mem_lock_manager_.get() != NULL) &&
      !shared_mem_lock_manager_->Attach()) {
    FallBackToFileBasedLocking();
  }
  if (file_cache_ != NULL) {
    file_cache_->set_worker(factory_->slow_worker());
  }
  if (mem_cache_ != NULL) {
    if (!mem_cache_->Connect()) {
      factory_->message_handler()->Message(kError, "Memory cache failed");
      abort();  // TODO(jmarantz): is there a better way to exit?
    }
  }
}

void ApacheCache::FallBackToFileBasedLocking() {
  if ((shared_mem_lock_manager_.get() != NULL) || (lock_manager_ == NULL)) {
    shared_mem_lock_manager_.reset(NULL);
    file_system_lock_manager_.reset(new FileSystemLockManager(
        factory_->file_system(), path_,
        factory_->scheduler(), factory_->message_handler()));
    lock_manager_ = file_system_lock_manager_.get();
  }
}

}  // namespace net_instaweb
