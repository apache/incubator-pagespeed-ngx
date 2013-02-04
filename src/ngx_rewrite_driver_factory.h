/*
 * Copyright 2012 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)

#ifndef NGX_REWRITE_DRIVER_FACTORY_H_
#define NGX_REWRITE_DRIVER_FACTORY_H_

#include <set>

#include "apr_pools.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/simple_stats.h"

// TODO(oschaaf): We should reparent ApacheRewriteDriverFactory and
// NgxRewriteDriverFactory to a new class OriginRewriteDriverFactory and factor
// out as much as possible.

namespace net_instaweb {

class AbstractSharedMem;
class AprMemCache;
class AsyncCache;
class CacheInterface;
class NgxServerContext;
class NgxCache;
class NgxRewriteOptions;
class SlowWorker;
class StaticJavaScriptManager;

class NgxRewriteDriverFactory : public RewriteDriverFactory {
 public:
  static const char kStaticJavaScriptPrefix[];
  static const char kMemcached[];

  // main_conf will have only options set in the main block.  It may be NULL,
  // and we do not take ownership.
  explicit NgxRewriteDriverFactory();
  virtual ~NgxRewriteDriverFactory();
  virtual Hasher* NewHasher();
  virtual UrlFetcher* DefaultUrlFetcher();
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual FileSystem* DefaultFileSystem();
  virtual Timer* DefaultTimer();
  virtual NamedLockManager* DefaultLockManager();
  virtual void SetupCaches(ServerContext* server_context);
  virtual Statistics* statistics();
  // Create a new RewriteOptions.  In this implementation it will be an
  // NgxRewriteOptions.
  virtual RewriteOptions* NewRewriteOptions();
  // Initializes the StaticJavascriptManager.
  virtual void InitStaticJavascriptManager(
      StaticJavascriptManager* static_js_manager);
  // Release all the resources. It also calls the base class ShutDown to
  // release the base class resources.
  virtual void ShutDown();
  virtual void StopCacheActivity();
  NgxServerContext* MakeNgxServerContext();
  // Finds a Cache for the file_cache_path in the config.  If none exists,
  // creates one, using all the other parameters in the NgxRewriteOptions.
  // Currently, no checking is done that the other parameters (e.g. cache
  // size, cleanup interval, etc.) are consistent.
  NgxCache* GetCache(NgxRewriteOptions* rewrite_options);

  AbstractSharedMem* shared_mem_runtime() const {
    return shared_mem_runtime_.get();
  }

  SlowWorker* slow_worker() { return slow_worker_.get(); }

  // Create a new AprMemCache from the given hostname[:port] specification.
  AprMemCache* NewAprMemCache(const GoogleString& spec);

  // Makes a memcached-based cache if the configuration contains a
  // memcached server specification.  The l2_cache passed in is used
  // to handle puts/gets for huge (>1M) values.  NULL is returned if
  // memcached is not specified for this server.
  //
  // If a non-null CacheInterface* is returned, its ownership is transferred
  // to the caller and must be freed on destruction.
  CacheInterface* GetMemcached(NgxRewriteOptions* options,
                               CacheInterface* l2_cache);

  // Returns the filesystem metadata cache for the given config's specification
  // (if it has one). NULL is returned if no cache is specified.
  CacheInterface* GetFilesystemMetadataCache(NgxRewriteOptions* config);

  // Starts pagespeed threads if they've not been started already.  Must be
  // called after the caller has finished any forking it intends to do.
  void StartThreads();
  // This helper method contains init procedures invoked by both RootInit()
  // and ChildInit()
  void ParentOrChildInit();
  // For shared memory resources the general setup we follow is to have the
  // first running process (aka the root) create the necessary segments and
  // fill in their shared data structures, while processes created to actually
  // handle requests attach to already existing shared data structures.
  //
  // During normal server startup[1], RootInit() is called from the nginx hooks
  // in the root process for the first task, and then ChildInit() is called in
  // any child process.
  //
  // Keep in mind, however, that when fork() is involved a process may
  // effectively see both calls, in which case the 'ChildInit' call would
  // come second and override the previous root status. Both calls are also
  // invoked in the debug single-process mode.
  //
  // [1] Besides normal startup, nginx also uses a temporary process to
  // syntax check the config file. That basically looks like a complete
  // normal startup and shutdown to the code.
  bool is_root_process() const { return is_root_process_; }
  void RootInit();
  void ChildInit();
  void set_main_conf(NgxRewriteOptions* main_conf){  main_conf_ = main_conf; }
 private:
  SimpleStats simple_stats_;
  Timer* timer_;
  scoped_ptr<SlowWorker> slow_worker_;
  scoped_ptr<AbstractSharedMem> shared_mem_runtime_;
  typedef std::map<GoogleString, NgxCache*> PathCacheMap;
  PathCacheMap path_cache_map_;
  MD5Hasher cache_hasher_;
  NgxRewriteOptions* main_conf_;
  typedef std::set<NgxServerContext*> NgxServerContextSet;
  NgxServerContextSet uninitialized_server_contexts_;

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
  std::vector<AsyncCache*> async_caches_;
  bool threads_started_;
  bool is_root_process_;

  DISALLOW_COPY_AND_ASSIGN(NgxRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NGX_REWRITE_DRIVER_FACTORY_H_
