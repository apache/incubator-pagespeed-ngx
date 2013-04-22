// Copyright 2013 Google Inc.
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
//         lsong@google.com (Libo Song)

#include "net/instaweb/system/public/system_caches.h"

#include <cstddef>
#include <cstdlib>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/write_through_http_cache.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/system/public/apr_mem_cache.h"
#include "net/instaweb/system/public/system_cache_path.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/util/public/async_cache.h"
#include "net/instaweb/util/public/cache_batcher.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/cache_stats.h"
#include "net/instaweb/util/public/compressed_cache.h"
#include "net/instaweb/util/public/fallback_cache.h"
#include "net/instaweb/util/public/file_cache.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/write_through_cache.h"

namespace net_instaweb {

const char SystemCaches::kMemcached[] = "memcached";
const char SystemCaches::kShmCache[] = "shm_cache";

SystemCaches::SystemCaches(
    RewriteDriverFactory* factory, AbstractSharedMem* shm_runtime,
    int thread_limit)
    : factory_(factory),
      shared_mem_runtime_(shm_runtime),
      thread_limit_(thread_limit),
      is_root_process_(true),
      was_shut_down_(false),
      cache_hasher_(20) {
}

SystemCaches::~SystemCaches() {
  DCHECK(was_shut_down_);
}

void SystemCaches::ShutDown(MessageHandler* message_handler) {
  DCHECK(!was_shut_down_);
  if (was_shut_down_) {
    return;
  }

  was_shut_down_ = true;

  // Shut down the cache cleaning thread so we no longer have to worry about
  // outstanding jobs in the slow_worker_ trying
  // to access FileCache and similar objects we're about to blow away.
  if (!is_root_process_) {
    slow_worker_->ShutDown();
  }

  // Take down any memcached threads.  Note that this may block
  // waiting for any wedged operations to terminate, possibly
  // requiring kill -9 to restart Apache if memcached is permanently
  // hung.  In pracice, the patches made in
  // src/third_party/aprutil/apr_memcache2.c make that very unlikely.
  //
  // The alternative scenario of exiting with pending I/O will often
  // crash and always leak memory.  Note that if memcached crashes, as
  // opposed to hanging, it will probably not appear wedged.
  memcached_pool_.reset(NULL);

  if (is_root_process_) {
    // Cleanup per-path shm resources.
    for (PathCacheMap::iterator p = path_cache_map_.begin(),
         e = path_cache_map_.end(); p != e; ++p) {
      p->second->GlobalCleanup(message_handler);
    }
  }
}

SystemCachePath* SystemCaches::GetCache(SystemRewriteOptions* config) {
  const GoogleString& path = config->file_cache_path();
  std::pair<PathCacheMap::iterator, bool> result = path_cache_map_.insert(
      PathCacheMap::value_type(path, static_cast<SystemCachePath*>(NULL)));
  PathCacheMap::iterator iter = result.first;
  if (result.second) {
    iter->second =
        new SystemCachePath(path, config, factory_, shared_mem_runtime_);
    factory_->TakeOwnership(iter->second);
  }
  return iter->second;
}

AprMemCache* SystemCaches::NewAprMemCache(const GoogleString& spec) {
  AprMemCache* mem_cache =
      new AprMemCache(spec, thread_limit_, &cache_hasher_,
                      factory_->statistics(), factory_->timer(),
                      factory_->message_handler());
  factory_->TakeOwnership(mem_cache);
  return mem_cache;
}

CacheInterface* SystemCaches::GetMemcached(SystemRewriteOptions* config) {
  CacheInterface* memcached = NULL;

  // Find a memcache that matches the current spec, or create a new one
  // if needed. Note that this means that two different VirtualHost's will
  // share a memcached if their specs are the same but will create their own
  // if the specs are different.
  if (!config->memcached_servers().empty()) {
    const GoogleString& server_spec = config->memcached_servers();
    std::pair<MemcachedMap::iterator, bool> result = memcached_map_.insert(
        MemcachedMap::value_type(server_spec, memcached));
    if (result.second) {
      AprMemCache* mem_cache = NewAprMemCache(server_spec);
      if (config->has_memcached_timeout_us()) {
        mem_cache->set_timeout_us(config->memcached_timeout_us());
      }
      memcache_servers_.push_back(mem_cache);

      int num_threads = config->memcached_threads();
      if (num_threads != 0) {
        if (num_threads != 1) {
          factory_->message_handler()->Message(
              kWarning, "ModPagespeedMemcachedThreads support for >1 thread "
              "is not supported yet; changing to 1 thread (was %d)",
              num_threads);
          num_threads = 1;
        }

        if (memcached_pool_.get() == NULL) {
          // Note -- we will use the first value of ModPagespeedMemCacheThreads
          // that we see in a VirtualHost, ignoring later ones.
          memcached_pool_.reset(
              new QueuedWorkerPool(num_threads, "memcached",
                                   factory_->thread_system()));
        }
        memcached = new AsyncCache(mem_cache, memcached_pool_.get());
        factory_->TakeOwnership(memcached);
      } else {
        memcached = mem_cache;
      }

      // Put the batcher above the stats so that the stats sees the MultiGets
      // and can show us the histogram of how they are sized.
#if CACHE_STATISTICS
      memcached = new CacheStats(kMemcached, memcached,
                                 factory_->timer(), factory_->statistics());
      factory_->TakeOwnership(memcached);
#endif
      CacheBatcher* batcher = new CacheBatcher(
          memcached, factory_->thread_system()->NewMutex(),
          factory_->statistics());
      factory_->TakeOwnership(batcher);
      if (num_threads != 0) {
        batcher->set_max_parallel_lookups(num_threads);
      }
      memcached = batcher;
      result.first->second = memcached;
    } else {
      memcached = result.first->second;
    }
  }
  return memcached;
}

bool SystemCaches::CreateShmMetadataCache(
    const GoogleString& name, int64 size_kb, GoogleString* error_msg) {
  MetadataShmCacheInfo* cache_info = NULL;
  std::pair<MetadataShmCacheMap::iterator, bool> result =
      metadata_shm_caches_.insert(
          MetadataShmCacheMap::value_type(name, cache_info));
  if (result.second) {
    int entries, blocks;
    int64 size_cap;
    const int kSectors = 128;
    MetadataShmCache::ComputeDimensions(
        size_kb, 2 /* block/entry ratio, based empirically off load tests */,
        kSectors, &entries, &blocks, &size_cap);

    // Make sure the size cap is not unusably low. In particular, with 2K
    // inlining thresholds, something like 3K is needed. (As of time of writing,
    // that required about 4.3MiB).
    if (size_cap < 3 * 1024) {
      metadata_shm_caches_.erase(result.first);
      *error_msg = "Shared memory cache unusably small.";
      return false;
    } else {
      cache_info = new MetadataShmCacheInfo;
      factory_->TakeOwnership(cache_info);
      cache_info->cache_backend =
          new SharedMemCache<64>(
              shared_mem_runtime_,
              StrCat(name, "/metadata_cache"),
              factory_->timer(),
              factory_->hasher(),
              kSectors,
              entries,  /* entries per sector */
              blocks /* blocks per sector*/,
              factory_->message_handler());
      factory_->TakeOwnership(cache_info->cache_backend);
      // We can't set cache_info->cache_to_use yet since statistics aren't ready
      // yet. It will happen in ::RootInit().
      result.first->second = cache_info;
      return true;
    }
  } else {
    *error_msg = StrCat("Cache named ", name, " already exists.");
    return false;
  }
}

NamedLockManager* SystemCaches::GetLockManager(SystemRewriteOptions* config) {
  return GetCache(config)->lock_manager();
}

CacheInterface* SystemCaches::GetShmMetadataCache(
    SystemRewriteOptions* config) {
  const GoogleString& name = config->file_cache_path();
  if (name.empty()) {
    return NULL;
  }
  MetadataShmCacheMap::iterator i = metadata_shm_caches_.find(name);
  if (i != metadata_shm_caches_.end()) {
    return i->second->cache_to_use;
  }
  return NULL;
}

CacheInterface* SystemCaches::GetFilesystemMetadataCache(
    SystemRewriteOptions* config) {
  // Reuse the memcached server(s) for the filesystem metadata cache. We need
  // to search for our config's entry in the vector of servers (not the more
  // obvious map) because the map's entries are wrapped in an AsyncCache, and
  // the filesystem metadata cache requires a blocking cache (like memcached).
  // Note that if we have a server spec we *know* it's in the searched vector.
  DCHECK_EQ(config->memcached_servers().empty(), memcache_servers_.empty());
  const GoogleString& server_spec = config->memcached_servers();
  for (int i = 0, n = memcache_servers_.size(); i < n; ++i) {
    if (server_spec == memcache_servers_[i]->server_spec()) {
      return memcache_servers_[i];
    }
  }

  return NULL;
}

void SystemCaches::SetupCaches(ServerContext* server_context) {
  SystemRewriteOptions* config = dynamic_cast<SystemRewriteOptions*>(
      server_context->global_options());
  DCHECK(config != NULL);
  SystemCachePath* caches_for_path = GetCache(config);
  CacheInterface* lru_cache = caches_for_path->lru_cache();
  CacheInterface* file_cache = caches_for_path->file_cache();
  CacheInterface* shm_metadata_cache = GetShmMetadataCache(config);
  CacheInterface* memcached = GetMemcached(config);
  if (memcached != NULL) {
    // Note that a distinct FallbackCache gets created for every VirtualHost
    // that employs memcached, even if the memcached and file-cache
    // specifications are identical.  This does no harm, because there
    // is no data in the cache object itself; just configuration.  Sharing
    // FallbackCache* objects would require making a map using the
    // memcache & file-cache specs as a key, so it's simpler to make a new
    // small FallbackCache object for each VirtualHost.
    memcached = new FallbackCache(memcached, file_cache,
                                  AprMemCache::kValueSizeThreshold,
                                  factory_->message_handler());
    server_context->DeleteCacheOnDestruction(memcached);
  }

  if (memcached != NULL) {
    server_context->set_filesystem_metadata_cache(
        GetFilesystemMetadataCache(config));
  }
  Statistics* stats = server_context->statistics();

  // Figure out our L1/L2 hierarchy for http cache.
  // TODO(jmarantz): consider moving ownership of the LRU cache into the
  // factory, rather than having one per vhost.
  //
  // Note that a user can disable the LRU cache by setting its byte-count
  // to 0.
  CacheInterface* http_l2 = (memcached != NULL) ? memcached : file_cache;
  int64 max_content_length = config->max_cacheable_response_content_length();
  HTTPCache* http_cache = NULL;
  if (lru_cache == NULL) {
    // No L1, and so backend is just the L2
    http_cache = new HTTPCache(http_l2, factory_->timer(),
                               factory_->hasher(), stats);
  } else {
    // L1 is LRU, with the L2 as computed above.
    WriteThroughHTTPCache* write_through_http_cache = new WriteThroughHTTPCache(
        lru_cache, http_l2, factory_->timer(), factory_->hasher(), stats);
    write_through_http_cache->set_cache1_limit(config->lru_cache_byte_limit());
    http_cache = write_through_http_cache;
  }

  http_cache->set_max_cacheable_response_content_length(max_content_length);
  server_context->set_http_cache(http_cache);

  // And now the metadata cache. If we only have one level, it will be in
  // metadata_l2, with metadata_l1 set to NULL.
  CacheInterface* metadata_l1 = NULL;
  CacheInterface* metadata_l2 = NULL;
  size_t l1_size_limit = WriteThroughCache::kUnlimited;
  if (shm_metadata_cache != NULL) {
    // Do we have both a local SHM cache and a memcached-backed cache? In that
    // case, it makes sense to go L1/L2 with them. If not, just use the SHM
    // cache and ignore the per-process LRU as it's basically strictly worse.
    if (memcached != NULL) {
      metadata_l1 = shm_metadata_cache;
      metadata_l2 = memcached;
    } else {
      metadata_l2 = shm_metadata_cache;
    }
  } else {
    l1_size_limit = config->lru_cache_byte_limit();
    metadata_l1 = lru_cache;  // may be NULL
    metadata_l2 = http_l2;  // memcached or file.
  }

  CacheInterface* metadata_cache;

  if (metadata_l1 != NULL) {
    WriteThroughCache* write_through_cache = new WriteThroughCache(
        metadata_l1, metadata_l2);
    server_context->DeleteCacheOnDestruction(write_through_cache);
    write_through_cache->set_cache1_limit(l1_size_limit);
    metadata_cache = write_through_cache;
  } else {
    metadata_cache = metadata_l2;
  }

  // TODO(jmarantz): We probably want to store HTTP cache compressed
  // even without this flag, but we should do it differently, storing
  // only the content compressed and putting in content-encoding:gzip
  // so that mod_gzip doesn't have to recompress on every request.
  if (config->compress_metadata_cache()) {
    metadata_cache = new CompressedCache(metadata_cache, stats);
    server_context->DeleteCacheOnDestruction(metadata_cache);
    CacheInterface* compressed_l2 = new CompressedCache(metadata_l2, stats);
    server_context->DeleteCacheOnDestruction(compressed_l2);
    server_context->MakePropertyCaches(compressed_l2);
  } else {
    server_context->MakePropertyCaches(metadata_l2);
  }
  server_context->set_metadata_cache(metadata_cache);
}

void SystemCaches::RegisterConfig(SystemRewriteOptions* config) {
  // Call GetCache and GetMemcached to fill in path_cache_map_ and
  // memcache_servers_ respectively.
  GetCache(config);
  GetMemcached(config);
}

void SystemCaches::RootInit() {
  for (MetadataShmCacheMap::iterator p = metadata_shm_caches_.begin(),
           e = metadata_shm_caches_.end(); p != e; ++p) {
    MetadataShmCacheInfo* cache_info = p->second;
    if (cache_info->cache_backend->Initialize()) {
      cache_info->cache_to_use =
          new CacheStats(kShmCache, cache_info->cache_backend,
                         factory_->timer(), factory_->statistics());
      factory_->TakeOwnership(cache_info->cache_to_use);
    } else {
      factory_->message_handler()->Message(
          kWarning, "Unable to initialize shared memory cache: %s.",
          p->first.c_str());
      cache_info->cache_backend = NULL;
      cache_info->cache_to_use = NULL;
    }
  }

  for (PathCacheMap::iterator p = path_cache_map_.begin(),
           e = path_cache_map_.end(); p != e; ++p) {
    SystemCachePath* cache = p->second;
    cache->RootInit();
  }
}

void SystemCaches::ChildInit() {
  is_root_process_ = false;

  slow_worker_.reset(
      new SlowWorker("slow_work_thread", factory_->thread_system()));
  for (MetadataShmCacheMap::iterator p = metadata_shm_caches_.begin(),
           e = metadata_shm_caches_.end(); p != e; ++p) {
    MetadataShmCacheInfo* cache_info = p->second;
    if ((cache_info->cache_backend != NULL) &&
        !cache_info->cache_backend->Attach()) {
      factory_->message_handler()->Message(
          kWarning, "Unable to attach to shared memory cache: %s.",
          p->first.c_str());
      delete cache_info->cache_backend;
      cache_info->cache_backend = NULL;
      cache_info->cache_to_use = NULL;
    }
  }

  for (PathCacheMap::iterator p = path_cache_map_.begin(),
           e = path_cache_map_.end(); p != e; ++p) {
    SystemCachePath* cache = p->second;
    cache->ChildInit(slow_worker_.get());
  }

  for (int i = 0, n = memcache_servers_.size(); i < n; ++i) {
    AprMemCache* mem_cache = memcache_servers_[i];
    if (!mem_cache->Connect()) {
      factory_->message_handler()->Message(kError, "Memory cache failed");
      abort();  // TODO(jmarantz): is there a better way to exit?
    }
  }
}

void SystemCaches::StopCacheActivity() {
  // Iterate through the map of CacheInterface* objects constructed for
  // the memcached.  Note that these are not typically AprMemCache* objects,
  // but instead are a hierarchy of CacheStats*, CacheBatcher*, AsyncCache*,
  // and AprMemCache*, all of which must be stopped.
  for (MemcachedMap::iterator p = memcached_map_.begin(),
           e = memcached_map_.end(); p != e; ++p) {
    CacheInterface* cache = p->second;
    cache->ShutDown();
  }

  // TODO(morlovich): Also shutdown shm caches
}

void SystemCaches::InitStats(Statistics* statistics) {
  AprMemCache::InitStats(statistics);
  FileCache::InitStats(statistics);
  CacheStats::InitStats(SystemCachePath::kFileCache, statistics);
  CacheStats::InitStats(SystemCachePath::kLruCache, statistics);
  CacheStats::InitStats(kShmCache, statistics);
  CacheStats::InitStats(kMemcached, statistics);
  CompressedCache::InitStats(statistics);
}

void SystemCaches::PrintCacheStats(StatFlags flags, GoogleString* out) {
  // We don't want to print this in per-vhost info since it would leak
  // all the declared caches.
  if (flags & kGlobalView) {
    for (MetadataShmCacheMap::iterator p = metadata_shm_caches_.begin(),
            e = metadata_shm_caches_.end(); p != e; ++p) {
      MetadataShmCacheInfo* cache_info = p->second;
      if (cache_info->cache_backend != NULL) {
        StrAppend(out, "Shared memory metadata cache '", p->first,
                  "' statistics:<br>");
        StrAppend(out, "<pre>", cache_info->cache_backend->DumpStats(),
                  "</pre>");
      }
    }
  }

  if (flags & kIncludeMemcached) {
    for (int i = 0, n = memcache_servers_.size(); i < n; ++i) {
      AprMemCache* mem_cache = memcache_servers_[i];
      if (!mem_cache->GetStatus(out)) {
        StrAppend(out, "\nError getting memcached server status for ",
                  mem_cache->server_spec());
      }
    }
  }
}

}  // namespace net_instaweb
