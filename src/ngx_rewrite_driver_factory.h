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
  #include <ngx_log.h>
}

#include <set>

#include "apr_pools.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/md5_hasher.h"

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
class NgxMessageHandler;
class NgxRewriteOptions;
class SharedCircularBuffer;
class SharedMemRefererStatistics;
class SharedMemStatistics;
class SlowWorker;
class StaticJavaScriptManager;
class Statistics;

class NgxRewriteDriverFactory : public RewriteDriverFactory {
 public:
  static const char kStaticJavaScriptPrefix[];
  static const char kMemcached[];

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
  // Create a new RewriteOptions.  In this implementation it will be an
  // NgxRewriteOptions.
  virtual RewriteOptions* NewRewriteOptions();
  // Print out details of all the connections to memcached servers.
  void PrintMemCacheStats(GoogleString* out);
  // Initializes the StaticJavascriptManager.
  virtual void InitStaticJavascriptManager(
      StaticJavascriptManager* static_js_manager);
  // Release all the resources. It also calls the base class ShutDown to
  // release the base class resources.
  // Initializes all the statistics objects created transitively by
  // NgxRewriteDriverFactory, including nginx-specific and
  // platform-independent statistics.
  static void InitStats(Statistics* statistics);
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
  void ParentOrChildInit(ngx_log_t* log);
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
  void RootInit(ngx_log_t* log);
  void ChildInit(ngx_log_t* log);
  void SharedCircularBufferInit(bool is_root);
  // Build global shared-memory statistics.  This is invoked if at least
  // one server context (global or VirtualHost) enables statistics.
  Statistics* MakeGlobalSharedMemStatistics(bool logging,
                                            int64 logging_interval_ms,
                                            const GoogleString& logging_file);

  // Creates and ::Initializes a shared memory statistics object.
  SharedMemStatistics* AllocateAndInitSharedMemStatistics(
      const StringPiece& name, const bool logging,
      const int64 logging_interval_ms, const GoogleString& logging_file);

  NgxMessageHandler* ngx_message_handler() { return ngx_message_handler_; }
  void set_main_conf(NgxRewriteOptions* main_conf) {  main_conf_ = main_conf; }

  bool use_per_vhost_statistics() const {
    return use_per_vhost_statistics_;
  }
  void set_use_per_vhost_statistics(bool x) {
    use_per_vhost_statistics_ = x;
  }
  bool install_crash_handler() const {
    return install_crash_handler_;
  }
  void set_install_crash_handler(bool x) {
    install_crash_handler_ = x;
  }
  bool message_buffer_size() const {
    return message_buffer_size_;
  }
  void set_message_buffer_size(int x) {
    message_buffer_size_ = x;
  }

 private:
  Timer* timer_;
  scoped_ptr<SlowWorker> slow_worker_;
  scoped_ptr<AbstractSharedMem> shared_mem_runtime_;
  typedef std::map<GoogleString, NgxCache*> PathCacheMap;
  PathCacheMap path_cache_map_;
  MD5Hasher cache_hasher_;
  // main_conf will have only options set in the main block.  It may be NULL,
  // and we do not take ownership.
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
  // If true, we'll have a separate statistics object for each vhost
  // (along with a global aggregate), rather than just a single object
  // aggregating all of them.
  bool use_per_vhost_statistics_;
  bool is_root_process_;
  NgxMessageHandler* ngx_message_handler_;
  NgxMessageHandler* ngx_html_parse_message_handler_;
  bool install_crash_handler_;
  int message_buffer_size_;
  scoped_ptr<SharedCircularBuffer> shared_circular_buffer_;
  scoped_ptr<SharedMemStatistics> shared_mem_statistics_;
  bool statistics_frozen_;

  DISALLOW_COPY_AND_ASSIGN(NgxRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NGX_REWRITE_DRIVER_FACTORY_H_
