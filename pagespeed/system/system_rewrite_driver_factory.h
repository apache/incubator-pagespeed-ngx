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
// Author: jefftk@google.com (Jeff Kaufman)

#ifndef PAGESPEED_SYSTEM_SYSTEM_REWRITE_DRIVER_FACTORY_H_
#define PAGESPEED_SYSTEM_SYSTEM_REWRITE_DRIVER_FACTORY_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/controller/central_controller.h"
#include "pagespeed/controller/central_controller_rpc_client.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"

namespace net_instaweb {

class AbstractSharedMem;
class FileSystem;
class MessageHandler;
class NamedLockManager;
class NonceGenerator;
class ProcessContext;
class ServerContext;
class SharedCircularBuffer;
class SharedMemStatistics;
class StaticAssetManager;
class SystemCaches;
class SystemRewriteOptions;
class SystemServerContext;
class SystemThreadSystem;
class Timer;
class UrlAsyncFetcher;

// A server context with features specific to a psol port on a unix system.
class SystemRewriteDriverFactory : public RewriteDriverFactory {
 public:
  // Takes ownership of thread_system.
  //
  // On Posix systems implementers should leave shared_mem_runtime NULL,
  // otherwise they should implement AbstractSharedMem for their platform and
  // pass in an instance here.  The factory takes ownership of the shared memory
  // runtime if one is passed in.  Implementers who don't want to support shared
  // memory at all should set PAGESPEED_SUPPORT_POSIX_SHARED_MEM to false and
  // pass in NULL, and the factory will use a NullSharedMem.
  //
  // After construction, you must call Init() to finish the initialization.
  SystemRewriteDriverFactory(const ProcessContext& process_context,
                             SystemThreadSystem* thread_system,
                             AbstractSharedMem* shared_mem_runtime,
                             StringPiece hostname, int port);
  virtual ~SystemRewriteDriverFactory();
  void Init();

  // If the server using this isn't using APR natively, call this to initialize
  // the APR library.
  static void InitApr();

  AbstractSharedMem* shared_mem_runtime() const {
    return shared_mem_runtime_.get();
  }

  // Creates and ::Initializes a shared memory statistics object.
  SharedMemStatistics* AllocateAndInitSharedMemStatistics(
      bool local, const StringPiece& name, const SystemRewriteOptions& options);

  // Hook for subclasses to init their stats and call
  // SystemRewriteDriverFactory::InitStats().
  virtual void NonStaticInitStats(Statistics* statistics) = 0;

  // Creates a HashedNonceGenerator initialized with data from /dev/random.
  NonceGenerator* DefaultNonceGenerator();

  // For shared memory resources the general setup we follow is to have the
  // first running process (aka the root) create the necessary segments and
  // fill in their shared data structures, while processes created to actually
  // handle requests attach to already existing shared data structures.
  //
  // During normal server startup[1], RootInit() must be called from the
  // root process and ChildInit() in every child process.
  //
  // Keep in mind, however, that when fork() is involved a process may
  // effectively see both calls, in which case the 'ChildInit' call would
  // come second and override the previous root status. Both calls are also
  // invoked in the debug single-process mode (In Apache, "httpd -X".).
  //
  // Note that these are not static methods -- they are invoked on every
  // SystemRewriteDriverFactory instance, which exist for the global
  // configuration as well as all the virtual hosts.
  //
  // Implementations should override RootInit and ChildInit for their setup.
  // See ApacheRewriteDriverFactory for an example.
  //
  // [1] Besides normal startup, Apache also uses a temporary process to
  // syntax check the config file. That basically looks like a complete
  // normal startup and shutdown to the code.
  bool is_root_process() const { return is_root_process_; }
  virtual void RootInit();
  virtual void ChildInit();

  // This helper method contains init procedures invoked by both RootInit()
  // and ChildInit()
  virtual void ParentOrChildInit();

  // After the whole configuration has been read, we need to do additional
  // configuration that requires a global view.
  // - server_contexts should be all server contexts on the system
  // - error_message and error_index are set if there's an error, and
  //   error_index is the index in server_contexts of the one with the issue.
  // - global_statistics is lazily initialized to a shared memory statistics
  //   owned by this factory if any of the server contexts require it.
  void PostConfig(const std::vector<SystemServerContext*>& server_contexts,
                  GoogleString* error_message,
                  int* error_index,
                  Statistics** global_statistics);

  // Initialize SharedCircularBuffer and pass it to SystemMessageHandler and
  // SystemHtmlParseMessageHandler. is_root is true if this is invoked from
  // root (ie. parent) process.
  void SharedCircularBufferInit(bool is_root);

  // Most options are parsed by and applied to the RewriteOptions via
  // ParseAndSetOptionFromNameN, but process-scope options need to be set on the
  // rewrite driver factory.
  //
  // ParseAndSetOptionN will only apply changes to the rewrite driver factory if
  // process_scope is true, but it should be called regardless in order to give
  // more helpful error messages ("wrong scope" vs "no such option").  If an
  // option is used out of scope an appropriate message is put in msg and either
  // kOptionValueInvalid or kOptionOk is returned: invalid if the parser should
  // abort with an error, ok if parsing should continue past the error.
  virtual RewriteOptions::OptionSettingResult ParseAndSetOption1(
      StringPiece option,
      StringPiece arg,
      bool process_scope,
      GoogleString* msg,
      MessageHandler* handler);
  virtual RewriteOptions::OptionSettingResult ParseAndSetOption2(
      StringPiece option,
      StringPiece arg1,
      StringPiece arg2,
      bool process_scope,
      GoogleString* msg,
      MessageHandler* handler);

  virtual Hasher* NewHasher();
  virtual Timer* DefaultTimer();
  virtual ServerContext* NewServerContext();

  // Hook so implementations may disable the property cache.
  virtual bool enable_property_cache() const {
    return true;
  }

  GoogleString hostname_identifier() { return hostname_identifier_; }

  // Release all the resources. It also calls the base class ShutDown to release
  // the base class resources.
  virtual void ShutDown();

  virtual void StopCacheActivity();

  SystemCaches* caches() { return caches_.get(); }

  virtual void set_message_buffer_size(int x) {
    message_buffer_size_ = x;
  }

  // Finds a fetcher for the settings in this config, sharing with
  // existing fetchers if possible, otherwise making a new one (and
  // its required thread).
  UrlAsyncFetcher* GetFetcher(SystemRewriteOptions* config);

  // Tracks the size of resources fetched from origin and populates the
  // X-Original-Content-Length header for resources derived from them.
  void set_track_original_content_length(bool x) {
    track_original_content_length_ = x;
  }
  bool track_original_content_length() const {
    return track_original_content_length_;
  }

  // When Serf gets a system error during polling, to avoid spamming
  // the log we just print the number of outstanding fetch URLs.  To
  // debug this it's useful to print the complete set of URLs, in
  // which case this should be turned on.
  void list_outstanding_urls_on_error(bool x) {
    list_outstanding_urls_on_error_ = x;
  }

  // When RateLimitBackgroundFetches is enabled the fetcher needs to apply some
  // limits.  An implementation may need to tune these based on conditions only
  // observable at startup, in which case they can override these.
  virtual int max_queue_size() { return 500 * requests_per_host(); }
  virtual int queued_per_host() { return 500 * requests_per_host(); }
  virtual int requests_per_host();  // Normally 4, or #threads if that's more.

  void set_static_asset_prefix(StringPiece s) {
    s.CopyToString(&static_asset_prefix_);
  }
  const GoogleString& static_asset_prefix() { return static_asset_prefix_; }

  int num_rewrite_threads() const { return num_rewrite_threads_; }
  void set_num_rewrite_threads(int x) { num_rewrite_threads_ = x; }
  int num_expensive_rewrite_threads() const {
    return num_expensive_rewrite_threads_;
  }
  void set_num_expensive_rewrite_threads(int x) {
    num_expensive_rewrite_threads_ = x;
  }
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

  // mod_pagespeed uses a beacon handler to collect data for critical images,
  // css, etc., so filters should be configured accordingly.
  virtual bool UseBeaconResultsInFilters() const {
    return true;
  }

  // Check whether the server is threaded.  For example, Nginx uses an event
  // loop and can keep with the default of false, while Apache with a threaded
  // multiprocessing module (MPM) overrides this method to return true.
  virtual bool IsServerThreaded() {
    return false;  // Most new servers are non-threaded nowadays.
  }

  // Threaded implementing servers should return the maximum number of threads
  // that might be used for handling user requests.
  virtual int LookupThreadLimit() {
    return 1;
  }

  // By default this uses the ControllerManager to fork off some processes to
  // handle the Controller.  If you're on a system where fork doesn't make
  // sense or running the Controller in its own process doesn't make sense, this
  // is a hook where you can start the controller in whatever way makes sense
  // for your platform.
  virtual void StartController(const SystemRewriteOptions& options);

  // Set the name of this process, for debugging visibility.
  virtual void NameProcess(const char* name);

  // Hook for handling any process-specific initialization the host webserver
  // might need when we manually fork off a process.  Children should call the
  // superclass method when overriding (so it can set the process name).  See
  // NgxRewriteDriverFactory::PrepareForkedProcess.
  virtual void PrepareForkedProcess(const char* name);

  // Once we've created the controller process, we need to initialize it like we
  // would one of our normal parent or child processes.  The controller manager
  // will call this once it has a process it needs prepared.
  virtual void PrepareControllerProcess();

 protected:
  // Initializes all the statistics objects created transitively by
  // SystemRewriteDriverFactory.  Only subclasses should call this.
  static void InitStats(Statistics* statistics);

  // Initializes the StaticAssetManager.
  virtual void InitStaticAssetManager(StaticAssetManager* static_asset_manager);

  virtual void SetupCaches(ServerContext* server_context);
  virtual QueuedWorkerPool* CreateWorkerPool(WorkerPoolCategory pool,
                                             StringPiece name);

  // TODO(jefftk): create SystemMessageHandler and get rid of these hooks.
  virtual void SetupMessageHandlers() {}
  virtual void ShutDownMessageHandlers() {}
  virtual void SetCircularBuffer(SharedCircularBuffer* buffer) {}

  // Can be overridden by subclasses to shutdown any fetchers we don't
  // know about.
  virtual void ShutDownFetchers() {}

  // Once ServerContexts are initialized via
  // RewriteDriverFactory::InitServerContext, they will be
  // managed by the RewriteDriverFactory.  But in the root process
  // the ServerContexts will never be initialized.  We track these here
  // so that SystemRewriteDriverFactory::ChildInit can iterate over all
  // the server contexts that need to be ChildInit'd, and so that we can free
  // them in the Root process that does not run ChildInit.
  typedef std::set<SystemServerContext*> SystemServerContextSet;
  SystemServerContextSet uninitialized_server_contexts_;

  // Allocates a serf fetcher.  Implementations may override this method to
  // supply other kinds of fetchers.  For example, ngx_pagespeed may return
  // either a serf fetcher or an nginx-native fetcher depending on options.
  virtual UrlAsyncFetcher* AllocateFetcher(SystemRewriteOptions* config);

  virtual FileSystem* DefaultFileSystem();
  virtual NamedLockManager* DefaultLockManager();

  // Updates num_rewrite_threads_ and num_expensive_rewrite_threads_
  // with sensible values if they are not explicitly set.
  virtual void AutoDetectThreadCounts();

  bool thread_counts_finalized() { return thread_counts_finalized_; }

  // Delegate from RewriteDriverFactory to construct CentralController.
  std::shared_ptr<CentralController> GetCentralController(
      NamedLockManager* lock_manager) override;

 private:
  // Build global shared-memory statistics, taking ownership.  This is invoked
  // if at least one server context (global or VirtualHost) enables statistics.
  Statistics* SetUpGlobalSharedMemStatistics(
      const SystemRewriteOptions& options);

  // Generates a cache-key incorporating all the parameters from config that
  // might be relevant to fetching.  When include_slurping_config, then
  // slurping-related options are ignored for the fetch-key.
  GoogleString GetFetcherKey(bool include_slurping_config,
                             const SystemRewriteOptions* config);

  // GetFetcher returns fetchers wrapped in various kinds of filtering (rate
  // limiting and slurping).  Because the underlying fetchers are expensive, and
  // you can wrap the same base fetcher in multiple ways, we provide a helper
  // method for GetFetcher that reuses fetchers as much as possible.
  UrlAsyncFetcher* GetBaseFetcher(SystemRewriteOptions* config);

  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();

  scoped_ptr<SharedMemStatistics> shared_mem_statistics_;
  // While split statistics in the ServerContext cleans up the actual objects,
  // we do the segment cleanup for local stats here.
  StringVector local_shm_stats_segment_names_;
  scoped_ptr<AbstractSharedMem> shared_mem_runtime_;
  scoped_ptr<SharedCircularBuffer> shared_circular_buffer_;

  bool statistics_frozen_;
  bool is_root_process_;

  // hostname_identifier_ equals to "server_hostname:port" of the webserver.
  // It's used to distinguish the name of shared memory, so that each virtual
  // host has its own SharedCircularBuffer.
  const GoogleString hostname_identifier_;

  // Size of shared circular buffer for displaying Info messages in
  // /pagespeed_messages (or /mod_pagespeed_messages, /ngx_pagespeed_messages)
  int message_buffer_size_;

  // Manages all our caches & lock managers.
  scoped_ptr<SystemCaches> caches_;

  bool track_original_content_length_;
  bool list_outstanding_urls_on_error_;

  // Fetchers are expensive--they each cost a thread.  Instead of allocating one
  // for every server context we keep a cache of defined fetchers with various
  // configurations.  There are two caches depending on whether the underlying
  // fetcher (the thing that takes a thread) needs to know about various
  // options.  The inner cache is base_fetcher_map_ which GetBaseFetcher() uses
  // to keep track of what fetchers it has requested from AllocateFetcher().
  // Base fetchers are all serf fetchers with various options unless an
  // implementation overrides AllocateFetcher() to return other kinds of
  // fetchers.  The outer cache is fetcher_map_, used by GetFetcher(), and is
  // fragmented on every option that affects fetching.  All of these fetchers
  // are either exactly as returned by GetBaseFetcher() or first wrapped in
  // slurping or rate-limiting.
  typedef std::map<GoogleString, UrlAsyncFetcher*> FetcherMap;
  FetcherMap base_fetcher_map_;
  FetcherMap fetcher_map_;

  // URL prefix for support files required by pagespeed.
  GoogleString static_asset_prefix_;

  // The same as our parent's thread_system_, but without casting.
  SystemThreadSystem* system_thread_system_;

  // If true, we'll have a separate statistics object for each vhost
  // (along with a global aggregate), rather than just a single object
  // aggregating all of them.
  bool use_per_vhost_statistics_;

  // If true, we'll install a signal handler that prints backtraces.
  bool install_crash_handler_;

  // true iff we ran through AutoDetectThreadCounts().
  bool thread_counts_finalized_;

  // These are <= 0 if we should autodetect.
  int num_rewrite_threads_;
  int num_expensive_rewrite_threads_;

  std::shared_ptr<CentralControllerRpcClient> central_controller_;

  DISALLOW_COPY_AND_ASSIGN(SystemRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_SYSTEM_REWRITE_DRIVER_FACTORY_H_
