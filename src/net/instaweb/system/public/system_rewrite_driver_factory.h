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

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_REWRITE_DRIVER_FACTORY_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_REWRITE_DRIVER_FACTORY_H_

#include <map>
#include <set>
#include <vector>

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"

#include "net/instaweb/util/public/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class AbstractSharedMem;
class FileSystem;
class Hasher;
class NamedLockManager;
class NonceGenerator;
class ServerContext;
class SharedCircularBuffer;
class SharedMemStatistics;
class Statistics;
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
  // On Posix systems implementors should leave shared_mem_runtime NULL,
  // otherwise they should implement AbstractSharedMem for their platform and
  // pass in an instance here.  The factory takes ownership of the shared memory
  // runtime if one is passed in.  Implementors who don't want to support shared
  // memory at all should set PAGESPEED_SUPPORT_POSIX_SHARED_MEM to false and
  // pass in NULL, and the factory will use a NullSharedMem.
  SystemRewriteDriverFactory(SystemThreadSystem* thread_system,
                             AbstractSharedMem* shared_mem_runtime,
                             StringPiece hostname, int port);
  virtual ~SystemRewriteDriverFactory();

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

  // Parses a comma-separated list of HTTPS options.  If successful, applies
  // the options to the fetcher and returns true.  If the options were invalid,
  // *error_message is populated and false is returned.
  //
  // It is *not* considered an error in this context to attempt to enable HTTPS
  // when support is not compiled in.  However, an error message will be logged
  // in the server log, and the option-setting will have no effect.
  bool SetHttpsOptions(StringPiece directive, GoogleString* error_message);

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
  virtual int requests_per_host() { return 4; }
  virtual int max_queue_size() { return 500 * requests_per_host(); }
  virtual int queued_per_host() { return 500 * requests_per_host(); }

  // By default statistics are collected separately for each virtual host.
  // Allow implementations to indicate that they don't support this.
  virtual bool use_per_vhost_statistics() const { return true; }

 protected:
  // Initializes all the statistics objects created transitively by
  // SystemRewriteDriverFactory.  Only subclasses should call this.
  static void InitStats(Statistics* statistics);

  virtual void SetupCaches(ServerContext* server_context);

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

  GoogleString https_options_;

  // The same as our parent's thread_system_, but without casting.
  SystemThreadSystem* system_thread_system_;

  DISALLOW_COPY_AND_ASSIGN(SystemRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_SYSTEM_REWRITE_DRIVER_FACTORY_H_
