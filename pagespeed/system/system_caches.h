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

#ifndef PAGESPEED_SYSTEM_SYSTEM_CACHES_H_
#define PAGESPEED_SYSTEM_SYSTEM_CACHES_H_

#include <map>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/sharedmem/shared_mem_cache.h"
#include "pagespeed/system/redis_cache.h"
#include "pagespeed/system/system_rewrite_options.h"

namespace net_instaweb {

class AbstractSharedMem;
class AprMemCache;
class NamedLockManager;
class QueuedWorkerPool;
class RewriteDriverFactory;
class ServerContext;
class SlowWorker;
class Statistics;
class SystemCachePath;

// Helps manage setup of cache backends provided by the PSOL library
// (LRU, File, Memcached, and shared memory metadata), as well as named lock
// managers. The expectation is that the RewriteDriverFactory for the server
// will invoke this class's methods in appropriate spots.
//
// It is also expected that the RootInit() method will be called during server
// setup before the server launches any additional processes, and ChildInit()
// will be called on any child process handling requests. If the server
// is single-process, both methods should be called.
//
// Keep in mind, however, that when fork() is involved a process may
// effectively see both calls, in which case the 'ChildInit' call would
// come second and override the previous root status.
class SystemCaches {
 public:
  // CacheStats prefixes.
  static const char kMemcachedAsync[];
  static const char kMemcachedBlocking[];
  static const char kRedisAsync[];
  static const char kRedisBlocking[];
  static const char kShmCache[];

  static const char kDefaultSharedMemoryPath[];

  enum StatFlags {
    kDefaultStatFlags = 0,
    kGlobalView = 1,
    kIncludeMemcached = 2,
    kIncludeRedis = 4
  };

  // Registers all statistics the cache backends may use.
  static void InitStats(Statistics* statistics);

  // thread_limit is an estimate of number of threads that may access the
  // cache at the same time. Does not take ownership of shm_runtime.
  SystemCaches(RewriteDriverFactory* factory,
               AbstractSharedMem* shm_runtime,
               int thread_limit);

  // Note that you must call ShutDown() before this is deleted.
  ~SystemCaches();

  bool is_root_process() const { return is_root_process_; }

  // Note: RegisterConfig must be called for all relevant configurations
  // before calling RootInit()
  void RegisterConfig(SystemRewriteOptions* config);
  void RootInit();
  void ChildInit();

  // Tries to block all asynchronous cache activity, causing lookups to
  // fail, to help quicker shutdown. Not 100% guaranteed to work, as not
  // all backends implement it.
  void StopCacheActivity();

  // Actually stops some of the work threads, and queues up deferred deletion of
  // various objects on the RewriteDriverFactory.
  void ShutDown(MessageHandler* message_handler);

  // Configures server_context's caches based on its configuration.
  void SetupCaches(ServerContext* server_context, bool enable_property_cache);

  // Creates & registers a shared memory metadata cache segment with given
  // name and size.
  //
  // Returns whether successful or not, and if not, *error_msg will contain
  // an error message.  Meant to be called from config parsing.
  bool CreateShmMetadataCache(
      StringPiece name, int64 size_kb, GoogleString* error_msg);

  // Returns, perhaps creating it, an appropriate named manager for this config
  // (potentially sharing with others as appropriate).
  NamedLockManager* GetLockManager(SystemRewriteOptions* config);

  // Print out stats appropriate for the given flags combination.
  void PrintCacheStats(StatFlags flags, GoogleString* out);

  // For cases where the thread limit isn't known at construction time, call
  // set_thread_limit() before calling any other methods.
  void set_thread_limit(int thread_limit) { thread_limit_ = thread_limit; }

  // Finds a Cache for the file_cache_path in the config.  If none exists,
  // creates one, using all the other parameters in the SystemRewriteOptions.
  // If multiple calls are made to get a file-cache with the same path, but
  // with different cleaning parameters, the parameters are merged based
  // on these rules:
  //   1. An explicitly configured option is selected over a default without
  //      warning.
  //   2. When there are two explicit settings, the higher size is picked,
  //      but the lower time-interval is picked.  A warning is issued to
  //      the server log, as this situation should be resolved by the server
  //      administrator.
  SystemCachePath* GetCache(SystemRewriteOptions* config);

 private:
  typedef SharedMemCache<64> MetadataShmCache;
  struct MetadataShmCacheInfo {
    MetadataShmCacheInfo()
        : cache_to_use(NULL), cache_backend(NULL), initialized(false) {}

    // Note that the fields may be NULL if e.g. initialization failed.
    CacheInterface* cache_to_use;  // may be CacheStats or such.
    GoogleString segment;
    MetadataShmCache* cache_backend;
    bool initialized;  // This is needed since in some scenarios we may
                       // not end up as far as calling ->Initialize() before
                       // we get shutdown.
  };

  struct ExternalCacheInterfaces {
    ExternalCacheInterfaces() : async(NULL), blocking(NULL) {}
    CacheInterface* async;
    CacheInterface* blocking;
  };

  // Given a blocking cache, prepares a fully functional ExternalCacheInterfaces
  // with both blocking and async versions. Async version is obtained by
  // wrapping blocking cache in AsyncCache with given worker pool.
  // If pool is NULL, this wrapping is omitted, effictively yielding a blocking
  // cache instead of async (this is used for compatibility with
  // MemcachedThreads 0 config option). Async version is then wrapped in
  // CacheBatcher. The value of batcher_max_parallel_lookups will be used to
  // override the batcher's max_parallel_lookups. If you don't want to override
  // it, pass in -1.
  //
  // Each cache is also wrapped in CacheStatistics with given name. All newly
  // created wrappers are owned by SystemCaches.
  ExternalCacheInterfaces ConstructExternalCacheInterfacesFromBlocking(
      CacheInterface* backend, QueuedWorkerPool* pool,
      int batcher_max_parallel_lookups, const char* async_stats_name,
      const char* blocking_stats_name);

  // Constructs external cache interfaces for a configuration. Both blocking
  // and (potentially) non-blocking interfaces are constructed, and given
  // separate stats. The returned interfaces are owned by SystemCaches, and must
  // not be freed by the caller.
  //
  // The corresponding external cache should be enabled in the config.
  ExternalCacheInterfaces NewMemcached(SystemRewriteOptions* config);
  ExternalCacheInterfaces NewRedis(SystemRewriteOptions* config);

  // Either constructs a new external cache (memcached/redis) based on
  // configuration or retrieves it from external_caches_map_ if it was already
  // constructed. Note that not all objects are stored in the map, some may be
  // created on each individual run (see impl for details).
  ExternalCacheInterfaces NewExternalCache(SystemRewriteOptions* config);

  // Returns any shared memory metadata cache configured for the given name, or
  // NULL.
  MetadataShmCacheInfo* LookupShmMetadataCache(const GoogleString& name);

  // Returns the shared metadata cache explicitly configured for this config if
  // it exists, otherwise return the default one, creating it if necessary.
  // Returns NULL if shared memory isn't supported or if the default cache is
  // disabled and this server context didn't explicitly configure its own.
  MetadataShmCacheInfo* GetShmMetadataCacheOrDefault(
      SystemRewriteOptions* config);

  // Establishes common cohorts for the property cache.
  void SetupPcacheCohorts(ServerContext* server_context,
                          bool enable_property_cache);

  scoped_ptr<SlowWorker> slow_worker_;

  RewriteDriverFactory* factory_;
  AbstractSharedMem* shared_mem_runtime_;
  int thread_limit_;
  bool is_root_process_;
  bool was_shut_down_;

  // File-Caches are expensive.  Just allocate one per distinct file-cache path.
  // At the moment there is no consistency checking for other parameters.  Note
  // that the LRUCache is instantiated inside the SystemCachePath, so we get a
  // new LRUCache for each distinct file-cache path.  Also note that only the
  // file-cache path is used as the key in this map.  Other parameters changed,
  // such as lru cache size or file cache clean interval, are taken from the
  // first file-cache found configured to one address.
  //
  // TODO(jmarantz): Consider instantiating one LRUCache per process.
  typedef std::map<GoogleString, SystemCachePath*> PathCacheMap;
  PathCacheMap path_cache_map_;

  // The QueuedWorkerPool for async cache-gets is shared among all memcached
  // connections. We have similar pool for Redis.
  // TODO(yeputons): consider reducing to a single pool. Potential problem: if
  // both memcached and Redis are enabled and one of them goes down, it blocks
  // requests to both servers. Actually, we have that problem already if there
  // different vhosts use different external cache servers.
  scoped_ptr<QueuedWorkerPool> memcached_pool_;
  scoped_ptr<QueuedWorkerPool> redis_pool_;

  // Explicit lists of AprMemCache/RedisCache instances are stored individually,
  // as they require extra treatment during startup and shutdown.
  // TODO(yeputons): consider reducing to a single vector when these classes
  // have common base class. Potential problem: users may want to enable
  // statistics for only memcached or only Redis (see kIncludeMemcached flag).
  std::vector<AprMemCache*> memcache_servers_;
  std::vector<RedisCache*> redis_servers_;

  // As each external cache object typically holds a TCP connection, we do not
  // want to allocate one per vhost (there can be tens of thousands of vhosts).
  // So, we create only one object per each external cache configuration and
  // store them in a map. See GetExternalCache() for details.
  //
  // All ExternalCacheInterfaces pairs stored already include (depending on
  // options) instances of CacheBatcher, AsyncCache and CacheStats.
  typedef std::map<GoogleString, ExternalCacheInterfaces> ExternalCachesMap;
  ExternalCachesMap external_caches_map_;

  // Map of any shared memory metadata caches we have + their CacheStats
  // wrappers. These are named explicitly to make configuration comprehensible.
  typedef std::map<GoogleString, MetadataShmCacheInfo*> MetadataShmCacheMap;

  // Note that entries here may be NULL in cases of config errors.
  MetadataShmCacheMap metadata_shm_caches_;

  MD5Hasher cache_hasher_;

  bool default_shm_metadata_cache_creation_failed_;

  DISALLOW_COPY_AND_ASSIGN(SystemCaches);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_SYSTEM_CACHES_H_
