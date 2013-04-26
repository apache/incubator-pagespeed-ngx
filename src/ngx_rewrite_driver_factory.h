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
  #include <ngx_log.h>
}

#include <set>

#include "apr_pools.h"
#include "net/instaweb/system/public/system_rewrite_driver_factory.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/scoped_ptr.h"

// TODO(oschaaf): We should reparent ApacheRewriteDriverFactory and
// NgxRewriteDriverFactory to a new class OriginRewriteDriverFactory and factor
// out as much as possible.

namespace net_instaweb {

class AbstractSharedMem;
class NgxMessageHandler;
class NgxRewriteOptions;
class NgxServerContext;
class NgxThreadSystem;
class NgxUrlAsyncFetcher;
class SharedCircularBuffer;
class SharedMemRefererStatistics;
class SharedMemStatistics;
class SlowWorker;
class StaticAssetManager;
class Statistics;
class SystemCaches;

class NgxRewriteDriverFactory : public SystemRewriteDriverFactory {
 public:
  static const char kStaticAssetPrefix[];

  // We take ownership of the thread system.
  explicit NgxRewriteDriverFactory(NgxThreadSystem* ngx_thread_system);
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
  // Initializes the StaticAssetManager.
  virtual void InitStaticAssetManager(
      StaticAssetManager* static_asset_manager);
  // Print out details of all the connections to memcached servers.
  void PrintMemCacheStats(GoogleString* out);
  bool InitNgxUrlAsyncFetcher();
  // Check resolver configured or not.
  bool CheckResolver();

  // Release all the resources. It also calls the base class ShutDown to
  // release the base class resources.
  // Initializes all the statistics objects created transitively by
  // NgxRewriteDriverFactory, including nginx-specific and
  // platform-independent statistics.
  static void InitStats(Statistics* statistics);
  virtual void ShutDown();
  virtual void StopCacheActivity();
  NgxServerContext* MakeNgxServerContext();
  ServerContext* NewServerContext();
  AbstractSharedMem* shared_mem_runtime() const {
    return shared_mem_runtime_.get();
  }

  SystemCaches* caches() { return caches_.get(); }

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
  void set_resolver(ngx_resolver_t* resolver) {
    resolver_ = resolver;
  }
  void set_resolver_timeout(ngx_msec_t resolver_timeout) {
    resolver_timeout_ = resolver_timeout == NGX_CONF_UNSET_MSEC ?
        1000 : resolver_timeout;
  }
  bool use_native_fetcher() {
    return use_native_fetcher_;
  }
  void set_use_native_fetcher(bool x) {
    use_native_fetcher_ = x;
  }

  // We use a beacon handler to collect data for critical images,
  // css, etc., so filters should be configured accordingly.
  //
  // TODO(jefftk): move to SystemRewriteDriverFactory
  virtual bool UseBeaconResultsInFilters() const {
    return true;
  }

 private:
  NgxThreadSystem* ngx_thread_system_;
  Timer* timer_;
  scoped_ptr<AbstractSharedMem> shared_mem_runtime_;

  // main_conf will have only options set in the main block.  It may be NULL,
  // and we do not take ownership.
  NgxRewriteOptions* main_conf_;
  typedef std::set<NgxServerContext*> NgxServerContextSet;
  NgxServerContextSet uninitialized_server_contexts_;

  // Manages all our caches & lock managers.
  scoped_ptr<SystemCaches> caches_;

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

  NgxUrlAsyncFetcher* ngx_url_async_fetcher_;
  ngx_log_t* log_;
  ngx_msec_t resolver_timeout_;
  ngx_resolver_t* resolver_;
  bool use_native_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(NgxRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NGX_REWRITE_DRIVER_FACTORY_H_
