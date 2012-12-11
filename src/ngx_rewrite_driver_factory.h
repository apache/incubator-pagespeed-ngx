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

extern "C" {
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_config.h>
}

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "apr_pools.h"

// TODO (oschaaf):
// We should reparent ApacheRewriteDriverFactory and NgxRewriteDriverFactory
// to a new class OriginRewriteDriverFactory & factor out as much as possible.

namespace net_instaweb {

class AbstractSharedMem;
class SlowWorker;
class StaticJavaScriptManager;
class NgxServerContext;
class AprMemCache;
class NgxCache;
class NgxRewriteOptions;
class AprMemCache;
class CacheInterface;
class AsyncCache;
class NgxUrlAsyncFetcher;

class NgxRewriteDriverFactory : public RewriteDriverFactory {
 public:
  static const char kStaticJavaScriptPrefix[];
  static const char kMemcached[];

  NgxRewriteDriverFactory(ngx_log_t* log, ngx_msec_t resolver_timeout,
      ngx_resolver_t* resolver);
  virtual ~NgxRewriteDriverFactory();
  virtual Hasher* NewHasher();
  virtual UrlFetcher* DefaultUrlFetcher();
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual FileSystem* DefaultFileSystem();
  virtual Timer* DefaultTimer();
  virtual NamedLockManager* DefaultLockManager();
  virtual void SetupCaches(ServerContext* resource_manager);
  virtual Statistics* statistics();
  // Create a new RewriteOptions.  In this implementation it will be an
  // NgxRewriteOptions.
  virtual RewriteOptions* NewRewriteOptions();
  // Initializes the StaticJavascriptManager.
  virtual void InitStaticJavascriptManager(
      StaticJavascriptManager* static_js_manager);
  bool InitNgxUrlAsyncFecther();

  AbstractSharedMem* shared_mem_runtime() const {
    return shared_mem_runtime_.get();
  }

  SlowWorker* slow_worker() { return slow_worker_.get(); }

  // Finds a Cache for the file_cache_path in the config.  If none exists,
  // creates one, using all the other parameters in the ApacheConfig.
  // Currently, no checking is done that the other parameters (e.g. cache
  // size, cleanup interval, etc.) are consistent.
  NgxCache* GetCache(NgxRewriteOptions* config);

  // Create a new AprMemCache from the given hostname[:port] specification.
  AprMemCache* NewAprMemCache(const GoogleString& spec);

  // Makes a memcached-based cache if the configuration contains a
  // memcached server specification.  The l2_cache passed in is used
  // to handle puts/gets for huge (>1M) values.  NULL is returned if
  // memcached is not specified for this server.
  //
  // If a non-null CacheInterface* is returned, its ownership is transferred
  // to the caller and must be freed on destruction.
  CacheInterface* GetMemcached(NgxRewriteOptions* options, CacheInterface* l2_cache);

  // Returns the filesystem metadata cache for the given config's specification
  // (if it has one). NULL is returned if no cache is specified.
  CacheInterface* GetFilesystemMetadataCache(NgxRewriteOptions* config);
private:
  SimpleStats simple_stats_;
  Timer* timer_;
  apr_pool_t* pool_;
  scoped_ptr<SlowWorker> slow_worker_;
  scoped_ptr<AbstractSharedMem> shared_mem_runtime_;
  typedef std::map<GoogleString, NgxCache*> PathCacheMap;
  PathCacheMap path_cache_map_;
  MD5Hasher cache_hasher_;

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

  NgxUrlAsyncFetcher* ngx_url_async_fetcher_;
  ngx_log_t* log_;
  ngx_msec_t resolver_timeout_;
  ngx_resolver_t* resolver_;
  DISALLOW_COPY_AND_ASSIGN(NgxRewriteDriverFactory);
};

} // namespace net_instaweb

#endif  // NGX_REWRITE_DRIVER_FACTORY_H_
