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

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_CACHES_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_CACHES_H_

#include <map>
#include <vector>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/shared_mem_cache.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AbstractSharedMem;
class AprMemCache;
class CacheInterface;
class MessageHandler;
class NamedLockManager;
class QueuedWorkerPool;
class RewriteDriverFactory;
class ServerContext;
class SlowWorker;
class Statistics;
class SystemCachePath;
class SystemRewriteOptions;

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
  static const char kMemcached[];
  static const char kShmCache[];

  static const char kDefaultSharedMemoryPath[];

  enum StatFlags {
    kDefaultStatFlags = 0,
    kGlobalView = 1,
    kIncludeMemcached = 2
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
  void SetupCaches(ServerContext* server_context);

  // Creates & registers a shared memory metadata cache segment with given
  // name and size.
  //
  // Returns whether successful or not, and if not, *error_msg will contain
  // an error message.  Meant to be called from config parsing.
  bool CreateShmMetadataCache(const GoogleString& name, int64 size_kb,
                              GoogleString* error_msg);

  // Returns, perhaps creating it, an appropriate named manager for this config
  // (potentially sharing with others as appropriate).
  NamedLockManager* GetLockManager(SystemRewriteOptions* config);

  // Print out stats appropriate for the given flags combination.
  void PrintCacheStats(StatFlags flags, GoogleString* out);

  // For cases where the thread limit isn't known at construction time, call
  // set_thread_limit() before calling any other methods.
  void set_thread_limit(int thread_limit) { thread_limit_ = thread_limit; }

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

  // Create a new AprMemCache from the given hostname[:port] specification.
  AprMemCache* NewAprMemCache(const GoogleString& spec);

  // Finds a Cache for the file_cache_path in the config.  If none exists,
  // creates one, using all the other parameters in the SystemRewriteOptions.
  // Currently, no checking is done that the other parameters (e.g. cache
  // size, cleanup interval, etc.) are consistent.
  SystemCachePath* GetCache(SystemRewriteOptions* config);

  // Makes a memcached-based cache if the configuration contains a
  // memcached server specification. NULL is returned if
  // memcached is not specified for this server.
  //
  // Always owns the return value.
  CacheInterface* GetMemcached(SystemRewriteOptions* config);

  // Returns the filesystem metadata cache for the given config's specification
  // (if it has one). NULL is returned if no cache is specified.
  CacheInterface* GetFilesystemMetadataCache(SystemRewriteOptions* config);

  // Returns any shared memory metadata cache configured for the given name, or
  // NULL.
  CacheInterface* LookupShmMetadataCache(const GoogleString& name);

  // Returns the shared metadata cache explicitly configured for this config if
  // it exists, otherwise return the default one, creating it if necessary.
  // Returns NULL if shared memory isn't supported or if the default cache is
  // disabled and this server context didn't explicitly configure its own.
  CacheInterface* GetShmMetadataCacheOrDefault(SystemRewriteOptions* config);

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

  // memcache connections are expensive.  Just allocate one per
  // distinct server-list.  At the moment there is no consistency
  // checking for other parameters.  Note that each memcached
  // interface share the thread allocation, based on the
  // ModPagespeedMemcachedThreads settings first encountered for
  // a particular server-set.
  //
  // The QueuedWorkerPool for async cache-gets is shared among all
  // memcached connections.
  //
  // The CacheInterface* value in the MemcacheMap now includes,
  // depending on options, instances of CacheBatcher, AsyncCache,
  // and CacheStats.  Explicit lists of AprMemCache instances and
  // AsyncCache objects are also included, as they require extra
  // treatment during startup and shutdown.
  typedef std::map<GoogleString, CacheInterface*> MemcachedMap;
  MemcachedMap memcached_map_;
  scoped_ptr<QueuedWorkerPool> memcached_pool_;
  std::vector<AprMemCache*> memcache_servers_;

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

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_CACHES_H_
