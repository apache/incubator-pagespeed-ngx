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

#ifndef NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
#define NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/apache/shared_mem_lifecycle.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/ref_counted_owner.h"

struct apr_pool_t;
struct server_rec;

namespace net_instaweb {

class AbstractSharedMem;
class SerfUrlAsyncFetcher;
class SharedMemLockManager;
class SharedMemStatistics;
class SlowWorker;
class SyncFetcherAdapter;
class UrlPollableAsyncFetcher;

// Creates an Apache RewriteDriver.
class ApacheRewriteDriverFactory : public RewriteDriverFactory {
 public:
  explicit ApacheRewriteDriverFactory(server_rec* server,
                                      const StringPiece& version);
  virtual ~ApacheRewriteDriverFactory();

  virtual Hasher* NewHasher();
  virtual AbstractMutex* NewMutex();

  // Returns the fetcher that will be used by the filters to load any
  // resources they need. This either matches the resource manager's
  // async fetcher or is NULL in case we are configured in a way that
  // all fetches will succeed immediately. Must be called after the fetchers
  // have been computed
  UrlPollableAsyncFetcher* SubResourceFetcher();

  void set_lru_cache_kb_per_process(int64 x) { lru_cache_kb_per_process_ = x; }
  void set_lru_cache_byte_limit(int64 x) { lru_cache_byte_limit_ = x; }
  void set_slurp_flush_limit(int64 x) { slurp_flush_limit_ = x; }
  int slurp_flush_limit() const { return slurp_flush_limit_; }
  void set_file_cache_clean_interval_ms(int64 x) {
    file_cache_clean_interval_ms_ = x;
  }
  void set_file_cache_clean_size_kb(int64 x) { file_cache_clean_size_kb_ = x; }
  void set_fetcher_time_out_ms(int64 x) { fetcher_time_out_ms_ = x; }
  bool set_file_cache_path(const StringPiece& x);

  void set_fetcher_proxy(const StringPiece& x) {
    x.CopyToString(&fetcher_proxy_);
  }

  // Controls whether we act as a rewriting proxy, fetching
  // URLs from origin without managing a slurp dump.
  void set_test_proxy(bool p) { test_proxy_ = p; }
  bool test_proxy() const { return test_proxy_; }

  // Whether to use shared memory locking or not.
  void set_use_shared_mem_locking(bool x) { use_shared_mem_locking_ = x; }

  StringPiece file_cache_path() { return file_cache_path_; }
  int64 file_cache_clean_size_kb() { return file_cache_clean_size_kb_; }
  int64 fetcher_time_out_ms() { return fetcher_time_out_ms_; }
  virtual Statistics* statistics();
  void SetStatistics(SharedMemStatistics* x);
  void set_statistics_enabled(bool x) { statistics_enabled_ = x; }
  void set_owns_statistics(bool o) { owns_statistics_ = o; }
  bool statistics_enabled() const { return statistics_enabled_; }

  AbstractSharedMem* shared_mem_runtime() const {
    return shared_mem_runtime_.get();
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

 protected:
  virtual UrlFetcher* DefaultUrlFetcher();
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();

  // Provide defaults.
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual FileSystem* DefaultFileSystem();
  virtual HtmlParse* DefaultHtmlParse();
  virtual Timer* DefaultTimer();
  virtual CacheInterface* DefaultCacheInterface();
  virtual NamedLockManager* DefaultLockManager();
  virtual ThreadSystem* DefaultThreadSystem();

  // Disable the Resource Manager's filesystem since we have a
  // write-through http_cache.
  virtual bool ShouldWriteResourcesToFileSystem() { return false; }

  // As we use the cache for storage, locks should be scoped to it.
  virtual StringPiece LockFilePrefix() { return file_cache_path_; }

  // Creates a shared memory lock manager for our settings, but doesn't
  // initialize it.
  SharedMemLockManager* CreateSharedMemLockManager();

  // Release all the resources. It also calls the base class ShutDown to release
  // the base class resources.
  void ShutDown();

 private:
  apr_pool_t* pool_;
  server_rec* server_rec_;
  SyncFetcherAdapter* serf_url_fetcher_;
  SerfUrlAsyncFetcher* serf_url_async_fetcher_;
  SharedMemStatistics* shared_mem_statistics_;
  scoped_ptr<AbstractSharedMem> shared_mem_runtime_;

  static RefCountedOwner<SlowWorker>::Family slow_worker_family_;
  RefCountedOwner<SlowWorker> slow_worker_;

  // TODO(jmarantz): These options could be consolidated in a protobuf or
  // some other struct, which would keep them distinct from the rest of the
  // state.  Note also that some of the options are in the base class,
  // RewriteDriverFactory, so we'd have to sort out how that worked.
  int64 lru_cache_kb_per_process_;
  int64 lru_cache_byte_limit_;
  int64 file_cache_clean_interval_ms_;
  int64 file_cache_clean_size_kb_;
  int64 fetcher_time_out_ms_;
  int64 slurp_flush_limit_;
  std::string file_cache_path_;
  std::string fetcher_proxy_;
  std::string version_;
  bool statistics_enabled_;
  bool statistics_frozen_;
  bool owns_statistics_;  // If true, this particular factory is responsible
                          // for calling GlobalCleanup on the (global)
                          // statistics object (but not delete'ing it)
  bool test_proxy_;
  bool is_root_process_;

  // Shared memory locking is enabled.
  bool use_shared_mem_locking_;

  SharedMemLifecycle<SharedMemLockManager> shared_mem_lock_manager_lifecycler_;

  // These maps keeps are used by SharedMemSubsystemLifecycleManager to
  // keep track of which of ours instance ApacheRewriteDriverFactory
  // is responsible for cleanup of shared memory segments for given
  // cache path (the key).
  //
  // This map is only used from the root process.
  static SharedMemOwnerMap* lock_manager_owners_;

  DISALLOW_COPY_AND_ASSIGN(ApacheRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
