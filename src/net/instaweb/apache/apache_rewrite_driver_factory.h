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
#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/apache/shared_mem_lifecycle.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/ref_counted_owner.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/shared_mem_referer_statistics.h"

struct apr_pool_t;
struct server_rec;

namespace net_instaweb {

class AbstractSharedMem;
class ApacheConfig;
class ApacheMessageHandler;
class SerfUrlAsyncFetcher;
class SharedMemLockManager;
class SharedMemStatistics;
class SharedMemRefererStatistics;
class SlowWorker;
class SyncFetcherAdapter;
class UrlPollableAsyncFetcher;

// Creates an Apache RewriteDriver.
class ApacheRewriteDriverFactory : public RewriteDriverFactory {
 public:
  ApacheRewriteDriverFactory(server_rec* server, const StringPiece& version);
  virtual ~ApacheRewriteDriverFactory();

  virtual Hasher* NewHasher();

  // Returns the fetcher that will be used by the filters to load any
  // resources they need. This either matches the resource manager's
  // async fetcher or is NULL in case we are configured in a way that
  // all fetches will succeed immediately. Must be called after the fetchers
  // have been computed
  UrlPollableAsyncFetcher* SubResourceFetcher();

  bool InitFileCachePath();

  GoogleString hostname_identifier() { return hostname_identifier_; }

  void SetStatistics(SharedMemStatistics* x);
  void set_owns_statistics(bool o) { owns_statistics_ = o; }

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

  ApacheConfig* config() { return config_.get(); }

 protected:
  virtual UrlFetcher* DefaultUrlFetcher();
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher();

  // Provide defaults.
  virtual MessageHandler* DefaultHtmlParseMessageHandler();
  virtual MessageHandler* DefaultMessageHandler();
  virtual FileSystem* DefaultFileSystem();
  virtual Timer* DefaultTimer();
  virtual CacheInterface* DefaultCacheInterface();
  virtual NamedLockManager* DefaultLockManager();

  virtual void ResourceManagerCreatedHook();

  // Disable the Resource Manager's filesystem since we have a
  // write-through http_cache.
  virtual bool ShouldWriteResourcesToFileSystem() { return false; }

  // As we use the cache for storage, locks should be scoped to it.
  virtual StringPiece LockFilePrefix() { return config_->file_cache_path(); }

  // Creates a shared memory lock manager for our settings, but doesn't
  // initialize it.
  SharedMemLockManager* CreateSharedMemLockManager();

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

 private:
  apr_pool_t* pool_;
  server_rec* server_rec_;
  SyncFetcherAdapter* serf_url_fetcher_;
  SerfUrlAsyncFetcher* serf_url_async_fetcher_;
  SharedMemStatistics* shared_mem_statistics_;
  scoped_ptr<AbstractSharedMem> shared_mem_runtime_;
  scoped_ptr<SharedCircularBuffer> shared_circular_buffer_;

  static RefCountedOwner<SlowWorker>::Family slow_worker_family_;
  RefCountedOwner<SlowWorker> slow_worker_;

  // TODO(jmarantz): These options could be consolidated in a protobuf or
  // some other struct, which would keep them distinct from the rest of the
  // state.  Note also that some of the options are in the base class,
  // RewriteDriverFactory, so we'd have to sort out how that worked.
  std::string version_;

  bool statistics_frozen_;
  bool owns_statistics_;  // If true, this particular factory is responsible
                          // for calling GlobalCleanup on the (global)
                          // statistics object (but not delete'ing it)
  bool is_root_process_;

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
  SharedMemLifecycle<SharedMemLockManager> shared_mem_lock_manager_lifecycler_;

  // These maps keeps are used by SharedMemSubsystemLifecycleManager to
  // keep track of which of ours instance ApacheRewriteDriverFactory
  // is responsible for cleanup of shared memory segments for given
  // cache path (the key).
  //
  // This map is only used from the root process.
  static SharedMemOwnerMap* lock_manager_owners_;

  scoped_ptr<ApacheConfig> config_;

  DISALLOW_COPY_AND_ASSIGN(ApacheRewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APACHE_REWRITE_DRIVER_FACTORY_H_
