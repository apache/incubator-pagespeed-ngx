// Copyright 2010 Google Inc.
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

#ifndef NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
#define NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_

#include <map>
#include <set>
#include <vector>

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

struct apr_pool_t;
struct request_rec;
struct server_rec;

namespace net_instaweb {

class AbstractSharedMem;
class ApacheCache;
class ApacheConfig;
class ApacheMessageHandler;
class ApacheServerContext;
class AprMemCache;
class AsyncCache;
class CacheInterface;
class FileSystem;
class Hasher;
class MessageHandler;
class ModSpdyFetchController;
class NamedLockManager;
class QueuedWorkerPool;
class RewriteDriver;
class RewriteOptions;
class SerfUrlAsyncFetcher;
class ServerContext;
class SharedCircularBuffer;
class SharedMemRefererStatistics;
class SharedMemStatistics;
class SlowWorker;
class StaticJavascriptManager;
class Statistics;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class UrlPollableAsyncFetcher;
class Writer;

// Creates an Apache RewriteDriver.
class ApacheRewriteDriverFactory : public RewriteDriverFactory {
 public:
  static const char kMemcached[];
  static const char kStaticJavaScriptPrefix[];

  ApacheRewriteDriverFactory(server_rec* server, const StringPiece& version);
  virtual ~ApacheRewriteDriverFactory();

  virtual Hasher* NewHasher();

  // Returns the fetcher that will be used by the filters to load any
  // resources they need. This either matches the resource manager's
  // async fetcher or is NULL in case we are configured in a way that
  // all fetches will succeed immediately. Must be called after the fetchers
  // have been computed
  UrlPollableAsyncFetcher* SubResourceFetcher();

  GoogleString hostname_identifier() { return hostname_identifier_; }

  AbstractSharedMem* shared_mem_runtime() const {
    return shared_mem_runtime_.get();
  }
  SharedMemRefererStatistics* shared_mem_referer_statistics() const {
    return shared_mem_referer_statistics_.get();
  }
  // Give access to apache_message_handler_ for the cases we need
  // to use ApacheMessageHandler rather than MessageHandler.
  // e.g. Use ApacheMessageHandler::Dump()
  // This is a better choice than cast from MessageHandler.
  ApacheMessageHandler* apache_message_handler() {
    return apache_message_handler_;
  }
  // For shared memory resources the general setup we follow is to have the
  // first running process (aka the root) create the necessary segments and
  // fill in their shared data structures, while processes created to actually
  // handle requests attach to already existing shared data structures.
  //
  // During normal server startup[1], RootInit() is called from the Apache hooks
  // in the root process for the first task, and then ChildInit() is called in
  // any child process.
  //
  // Keep in mind, however, that when fork() is involved a process may
  // effectively see both calls, in which case the 'ChildInit' call would
  // come second and override the previous root status. Both calls are also
  // invoked in the debug single-process mode (httpd -X).
  //
  // Note that these are not static methods --- they are invoked on every
  // ApacheRewriteDriverFactory instance, which exist for the global
  // configuration as well as all the vhosts.
  //
  // [1] Besides normal startup, Apache also uses a temporary process to
  // syntax check the config file. That basically looks like a complete
  // normal startup and shutdown to the code.
  bool is_root_process() const { return is_root_process_; }
  void RootInit();
  void ChildInit();

  void DumpRefererStatistics(Writer* writer);

  SlowWorker* slow_worker() { return slow_worker_.get(); }

  // Build global shared-memory statistics.  This is invoked if at least
  // one server context (global or VirtualHost) enables statistics.
  Statistics* MakeGlobalSharedMemStatistics(bool logging,
                                            int64 logging_interval_ms,
                                            const GoogleString& logging_file);

  // Creates and ::Initializes a shared memory statistics object.
  SharedMemStatistics* AllocateAndInitSharedMemStatistics(
      const StringPiece& name, const bool logging,
      const int64 logging_interval_ms, const GoogleString& logging_file);

  ApacheServerContext* MakeApacheServerContext(server_rec* server);

  // Makes fetches from PSA to origin-server request
  // accept-encoding:gzip, even when used in a context when we want
  // cleartext.  We'll decompress as we read the content if needed.
  void set_fetch_with_gzip(bool x) { fetch_with_gzip_ = x; }
  bool fetch_with_gzip() const { return fetch_with_gzip_; }

  // Tracks the size of resources fetched from origin and populates the
  // X-Original-Content-Length header for resources derived from them.
  void set_track_original_content_length(bool x) {
    track_original_content_length_ = x;
  }
  bool track_original_content_length() const {
    return track_original_content_length_;
  }

  void set_num_rewrite_threads(int x) { num_rewrite_threads_ = x; }
  int num_rewrite_threads() const { return num_rewrite_threads_; }
  void set_num_expensive_rewrite_threads(int x) {
    num_expensive_rewrite_threads_ = x;
  }
  int num_expensive_rewrite_threads() const {
    return num_expensive_rewrite_threads_;
  }

  void set_message_buffer_size(int x) {
    message_buffer_size_ = x;
  }

  // When Serf gets a system error during polling, to avoid spamming
  // the log we just print the number of outstanding fetch URLs.  To
  // debug this it's useful to print the complete set of URLs, in
  // which case this should be turned on.
  void list_outstanding_urls_on_error(bool x) {
    list_outstanding_urls_on_error_ = x;
  }

  bool use_per_vhost_statistics() const {
    return use_per_vhost_statistics_;
  }

  void set_use_per_vhost_statistics(bool x) {
    use_per_vhost_statistics_ = x;
  }

  bool enable_property_cache() const {
    return enable_property_cache_;
  }

  void set_enable_property_cache(bool x) {
    enable_property_cache_ = x;
  }

  // If true, virtual hosts should inherit global configuration.
  bool inherit_vhost_config() const {
    return inherit_vhost_config_;
  }

  void set_inherit_vhost_config(bool x) {
    inherit_vhost_config_ = x;
  }

  bool disable_loopback_routing() const {
    return disable_loopback_routing_;
  }

  void set_disable_loopback_routing(bool x) {
    disable_loopback_routing_ = x;
  }

  bool install_crash_handler() const {
    return install_crash_handler_;
  }

  void set_install_crash_handler(bool x) {
    install_crash_handler_ = x;
  }

  // Finds a Cache for the file_cache_path in the config.  If none exists,
  // creates one, using all the other parameters in the ApacheConfig.
  // Currently, no checking is done that the other parameters (e.g. cache
  // size, cleanup interval, etc.) are consistent.
  ApacheCache* GetCache(ApacheConfig* config);

  // Create a new AprMemCache from the given hostname[:port] specification.
  AprMemCache* NewAprMemCache(const GoogleString& spec);

  // Makes a memcached-based cache if the configuration contains a
  // memcached server specification.  The l2_cache passed in is used
  // to handle puts/gets for huge (>1M) values.  NULL is returned if
  // memcached is not specified for this server.
  //
  // If a non-null CacheInterface* is returned, its ownership is transferred
  // to the caller and must be freed on destruction.
  CacheInterface* GetMemcached(ApacheConfig* config, CacheInterface* l2_cache);

  // Returns the filesystem metadata cache for the given config's specification
  // (if it has one). NULL is returned if no cache is specified.
  CacheInterface* GetFilesystemMetadataCache(ApacheConfig* config);

  // Stops any further Gets from occuring in the Async cache.  This is used to
  // help wind down activity during a shutdown.
  void StopAsyncGets();

  // Finds a fetcher for the settings in this config, sharing with
  // existing fetchers if possible, otherwise making a new one (and
  // its required thread).
  UrlAsyncFetcher* GetFetcher(ApacheConfig* config);

  // As above, but just gets a Serf fetcher --- not a slurp fetcher or a rate
  // limiting one, etc.
  SerfUrlAsyncFetcher* GetSerfFetcher(ApacheConfig* config);

  // Notification of apache tearing down a context (vhost or top-level)
  // corresponding to given ApacheServerContext. Returns true if it was
  // the last context.
  bool PoolDestroyed(ApacheServerContext* rm);

  // Create a new RewriteOptions.  In this implementation it will be an
  // ApacheConfig.
  virtual RewriteOptions* NewRewriteOptions();

  // As above, but set a name on the ApacheConfig noting that it came from
  // a query.
  virtual RewriteOptions* NewRewriteOptionsForQuery();

  // Initializes all the statistics objects created transitively by
  // ApacheRewriteDriverFactory, including apache-specific and
  // platform-independent statistics.
  static void InitStats(Statistics* statistics);
  static void Initialize();
  static void Terminate();

  // Print out details of all the connections to memcached servers.
  void PrintMemCacheStats(GoogleString* out);

  // If needed, sets session fetchers on the driver to do the following:
  // a) Adds custom headers when configured in RewriteOptions.
  // b) Route requests directly to this very server when they are not
  //    configured to be external.
  // c) Route requests to mod_spdy's slave connection code if configured to.
  void ApplySessionFetchers(ApacheServerContext* manager,
                            RewriteDriver* driver, request_rec* req);

  // Returns true if we should handle request as SPDY.
  // This happens in two cases:
  // 1) It's actually a SPDY request using mod_spdy
  // 2) The header X-PSA-Optimize-For-SPDY is present, with any value.
  static bool TreatRequestAsSpdy(request_rec* req);

  // Parses a comma-separated list of HTTPS options.  If successful, applies
  // the options to the fetcher and returns true.  If the options were invalid,
  // *error_message is populated and false is returned.
  //
  // It is *not* considered an error in this context to attempt to enable HTTPS
  // when support is not compiled in.  However, an error message will be logged
  // in the server log, and the option-setting will have no effect.
  bool SetHttpsOptions(StringPiece directive, GoogleString* error_message);

 protected:
  virtual UrlFetcher* DefaultUrlFetcher();
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();
  virtual void StopCacheActivity();

  // Provide defaults.
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual FileSystem* DefaultFileSystem();
  virtual Timer* DefaultTimer();
  virtual void SetupCaches(ServerContext* resource_manager);
  virtual NamedLockManager* DefaultLockManager();
  virtual QueuedWorkerPool* CreateWorkerPool(WorkerPoolName name);

  // Disable the Resource Manager's filesystem since we have a
  // write-through http_cache.
  virtual bool ShouldWriteResourcesToFileSystem() { return false; }

  // This helper method contains init procedures invoked by both RootInit()
  // and ChildInit()
  void ParentOrChildInit();
  // Initialize SharedCircularBuffer and pass it to ApacheMessageHandler and
  // ApacheHtmlParseMessageHandler. is_root is true if this is invoked from
  // root (ie. parent) process.
  void SharedCircularBufferInit(bool is_root);
  // Initialize shared_mem_referer_statistics_; is_root should be true if this
  // is invoked from the root (i.e. parent) process
  void SharedMemRefererStatisticsInit(bool is_root);

  // Release all the resources. It also calls the base class ShutDown to release
  // the base class resources.
  virtual void ShutDown();

  // Initializes the StaticJavascriptManager.
  virtual void InitStaticJavascriptManager(
      StaticJavascriptManager* static_js_manager);

 private:
  // Updates num_rewrite_threads_ and num_expensive_rewrite_threads_
  // with sensible values if they are not explicitly set.
  void AutoDetectThreadCounts();

  apr_pool_t* pool_;
  server_rec* server_rec_;
  scoped_ptr<SharedMemStatistics> shared_mem_statistics_;
  scoped_ptr<AbstractSharedMem> shared_mem_runtime_;
  scoped_ptr<SharedCircularBuffer> shared_circular_buffer_;
  scoped_ptr<SlowWorker> slow_worker_;

  // TODO(jmarantz): These options could be consolidated in a protobuf or
  // some other struct, which would keep them distinct from the rest of the
  // state.  Note also that some of the options are in the base class,
  // RewriteDriverFactory, so we'd have to sort out how that worked.
  GoogleString version_;

  bool statistics_frozen_;
  bool is_root_process_;
  bool fetch_with_gzip_;
  bool track_original_content_length_;
  bool list_outstanding_urls_on_error_;

  scoped_ptr<SharedMemRefererStatistics> shared_mem_referer_statistics_;

  // hostname_identifier_ equals to "server_hostname:port" of Apache,
  // it's used to distinguish the name of shared memory,
  // so that each vhost has its own SharedCircularBuffer.
  const GoogleString hostname_identifier_;
  // This will be assigned to message_handler_ when message_handler() or
  // html_parse_message_handler is invoked for the first time.
  // We keep an extra link because we need to refer them as
  // ApacheMessageHandlers rather than just MessageHandler in initialization
  // process.
  ApacheMessageHandler* apache_message_handler_;
  // This will be assigned to html_parse_message_handler_ when
  // html_parse_message_handler() is invoked for the first time.
  // Note that apache_message_handler_ and apache_html_parse_message_handler
  // writes to the same shared memory which is owned by the factory.
  ApacheMessageHandler* apache_html_parse_message_handler_;

  // Once ServerContexts are initialized via
  // RewriteDriverFactory::InitServerContext, they will be
  // managed by the RewriteDriverFactory.  But in the root Apache process
  // the ServerContexts will never be initialized.  We track these here
  // so that ApacheRewriteDriverFactory::ChildInit can iterate over all
  // the managers that need to be ChildInit'd, and so that we can free
  // the managers in the Root process that were never ChildInit'd.
  typedef std::set<ApacheServerContext*> ApacheServerContextSet;
  ApacheServerContextSet uninitialized_managers_;

  // If true, we'll have a separate statistics object for each vhost
  // (along with a global aggregate), rather than just a single object
  // aggregating all of them.
  bool use_per_vhost_statistics_;

  // Enable the property cache.
  bool enable_property_cache_;

  // Inherit configuration from global context into vhosts.
  bool inherit_vhost_config_;

  // If false (default) we will redirect all fetches to unknown hosts to
  // localhost.
  bool disable_loopback_routing_;

  // If true, we'll install a signal handler that prints backtraces.
  bool install_crash_handler_;

  // true iff we ran through AutoDetectThreadCounts()
  bool thread_counts_finalized_;

  // These are <= 0 if we should autodetect.
  int num_rewrite_threads_;
  int num_expensive_rewrite_threads_;

  int max_mod_spdy_fetch_threads_;

  // Size of shared circular buffer for displaying Info messages in
  // /mod_pagespeed_messages.
  int message_buffer_size_;

  // File-Caches are expensive.  Just allocate one per distinct file-cache path.
  // At the moment there is no consistency checking for other parameters.  Note
  // that the LRUCache is instantiated inside the ApacheCache, so we get a new
  // LRUCache for each distinct file-cache path.  Also note that only the
  // file-cache path is used as the key in this map.  Other parameters changed,
  // such as lru cache size or file cache clean interval, are taken from the
  // first file-cache found configured to one address.
  //
  // TODO(jmarantz): Consider instantiating one LRUCache per process.
  typedef std::map<GoogleString, ApacheCache*> PathCacheMap;
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
  std::vector<AsyncCache*> async_caches_;

  // Serf fetchers are expensive -- they each cost a thread. Allocate
  // one for each proxy/slurp-setting.  Currently there is no
  // consistency checking for fetcher timeout.
  typedef std::map<GoogleString, UrlAsyncFetcher*> FetcherMap;
  FetcherMap fetcher_map_;
  typedef std::map<GoogleString, SerfUrlAsyncFetcher*> SerfFetcherMap;
  SerfFetcherMap serf_fetcher_map_;
  MD5Hasher cache_hasher_;

  // Helps coordinate direct-to-mod_spdy fetches.
  scoped_ptr<ModSpdyFetchController> mod_spdy_fetch_controller_;

  GoogleString https_options_;

  DISALLOW_COPY_AND_ASSIGN(ApacheRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
