/*
 * Copyright 2010 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_FACTORY_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_FACTORY_H_

#include <set>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class CacheInterface;
class FileSystem;
class FilenameEncoder;
class Hasher;
class HTTPCache;
class MessageHandler;
class NamedLockManager;
class QueuedWorkerPool;
class ResourceManager;
class RewriteDriver;
class RewriteOptions;
class RewriteStats;
class Scheduler;
class Statistics;
class ThreadSystem;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class UrlNamer;

// A base RewriteDriverFactory.
class RewriteDriverFactory {
 public:
  enum WorkerPoolName {
    kHtmlWorkers,
    kRewriteWorkers,
    kLowPriorityRewriteWorkers,
    // Make sure to insert new values above this line.
    kNumWorkerPools
  };

  // Takes ownership of thread_system.
  explicit RewriteDriverFactory(ThreadSystem* thread_system);

  // Initializes thread_system_ using ThreadSystem::ComputeThreadSystem().
  RewriteDriverFactory();

  virtual ~RewriteDriverFactory();

  // The RewriteDriveFactory will create objects of default type through the
  // New* method from drived classs.  Here are the objects that can be
  // replaced before creating the RewriteDriver.
  // Note: RewriteDriver takes ownership of these.
  void set_html_parse_message_handler(MessageHandler* message_handler);
  void set_message_handler(MessageHandler* message_handler);
  void set_file_system(FileSystem* file_system);
  void set_hasher(Hasher* hasher);
  void set_filename_encoder(FilenameEncoder* filename_encoder);
  void set_url_namer(UrlNamer* url_namer);
  void set_timer(Timer* timer);

  // Set up a directory for slurped files for HTML and resources.  If
  // read_only is true, then it will only read from these files, and
  // this will eliminate the usage of any other url_fetcher.  If
  // read_only is false, then the existing url fetcher will be used as
  // a fallback if the slurped file is not found, and slurped files will
  // be subsequently written so they don't have to be fetched from
  // the Internet again.
  //
  // You must set the slurp directory prior to calling ComputeUrlFetcher
  // or ComputeUrlAsyncFetcher.
  void set_slurp_directory(const StringPiece& directory);
  void set_slurp_read_only(bool read_only);
  void set_slurp_print_urls(bool read_only);

  // Setting HTTP caching on causes both the fetcher and the async
  // fecher to return cached versions.
  void set_force_caching(bool u) { force_caching_ = u; }

  // You should either call set_base_url_fetcher,
  // set_base_url_async_fetcher, or neither.  Do not set both.  If you
  // want to enable real async fetching, because you are serving or
  // want to model live traffic, then call set_base_url_async_fetcher
  // before calling url_fetcher.
  //
  // These fetchers may be used directly when serving traffic, or they
  // may be aggregated with other fetchers (e.g. for slurping).
  //
  // You cannot set either base URL fetcher once ComputeUrlFetcher has
  // been called.
  void set_base_url_fetcher(UrlFetcher* url_fetcher);
  void set_base_url_async_fetcher(UrlAsyncFetcher* url_fetcher);

  bool set_filename_prefix(StringPiece p);

  // Determines whether Slurping is enabled.
  bool slurping_enabled() const { return !slurp_directory_.empty(); }

  // Deprecated method to get an options structure for the first
  // ResourceManager.  If a resource maanger has not been not created
  // yet, an options structure wll be returned anyway and applied
  // when a ResourceManager is eventually created.
  RewriteOptions* options();  // thread-safe

  MessageHandler* html_parse_message_handler();
  MessageHandler* message_handler();
  FileSystem* file_system();
  // TODO(sligocki): Remove hasher() and force people to make a NewHasher when
  // they need one.
  Hasher* hasher();
  FilenameEncoder* filename_encoder() { return filename_encoder_.get(); }
  UrlNamer* url_namer();

  // These accessors are *not* thread-safe.  They must be called once prior
  // to forking threads, e.g. via ComputeUrlFetcher().
  Timer* timer();
  HTTPCache* http_cache();
  NamedLockManager* lock_manager();
  QueuedWorkerPool* WorkerPool(WorkerPoolName pool);
  Scheduler* scheduler();

  StringPiece filename_prefix();

  // Computes URL fetchers using the based fetcher, and optionally,
  // slurp_directory and slurp_read_only.  These are not thread-safe;
  // they must be called once prior to spawning threads, e.g. via
  // CreateResourceManager.
  virtual UrlFetcher* ComputeUrlFetcher();
  virtual UrlAsyncFetcher* ComputeUrlAsyncFetcher();

  // Threadsafe mechanism to create a managed ResourceManager.  The
  // ResourceManager is owned by the factory, and should not be
  // deleted directly.  Currently it is not possible to delete a
  // resource manager except by deleting the entire factory.
  ResourceManager* CreateResourceManager();

  // Deprecated method that returns the first resource manager, creating one if
  // needed.  This method is thread-safe.
  ResourceManager* ComputeResourceManager();

  // See doc in resource_manager.cc.
  RewriteDriver* NewRewriteDriver();

  // Provides an optional hook for adding rewrite passes that are
  // specific to an implementation of RewriteDriverFactory.
  virtual void AddPlatformSpecificRewritePasses(RewriteDriver* driver);

  ThreadSystem* thread_system() { return thread_system_.get(); }

  // Returns the set of directories that we (our our subclasses) have created
  // thus far.
  const StringSet& created_directories() const {
    return created_directories_;
  }

  bool async_rewrites() { return async_rewrites_; }

  // Sets the resource manager into async_rewrite mode.  This can be
  // called before or after ComputeResourceManager, but will only
  // affect RewriteDrivers that are created after the call is made.
  void SetAsyncRewrites(bool x);

  // Collection of global statistics objects.  This is thread-unsafe:
  // it must be called prior to spawning threads, and after any calls
  // to SetStatistics.  Failing that, it will be initialized in the
  // first call to ComputeResourceManager, which is thread-safe.
  RewriteStats* rewrite_stats();

  // statistics (default is NullStatistics).  This can be overridden by calling
  // SetStatistics, either from subclasses or externally.
  Statistics* statistics() { return statistics_; }

  static void Initialize(Statistics* statistics);

  // Does *not* take ownership of Statistics.
  void SetStatistics(Statistics* stats);

  // Clean up all the factory-owned resources: fetchers, pools,
  // Resource Managers, the Drivers owned by the Resource Managers,
  // and worker threads.
  virtual void ShutDown();

  // Registers the directory as having been created by us.
  void AddCreatedDirectory(const GoogleString& dir);

 protected:
  bool FetchersComputed() const;
  void StopCacheWrites();

  // Implementors of RewriteDriverFactory must supply default definitions
  // for each of these methods, although they may be overridden via set_
  // methods above
  virtual UrlFetcher* DefaultUrlFetcher() = 0;
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher() = 0;
  virtual MessageHandler* DefaultHtmlParseMessageHandler() = 0;
  virtual MessageHandler* DefaultMessageHandler() = 0;
  virtual FileSystem* DefaultFileSystem() = 0;
  virtual Timer* DefaultTimer() = 0;
  virtual Hasher* NewHasher() = 0;

    // Note: Returned CacheInterface should be thread-safe.
  virtual CacheInterface* DefaultCacheInterface() = 0;

  // They may also supply a custom lock manager. The default implementation
  // will use the file system.
  virtual NamedLockManager* DefaultLockManager();

  // They may also supply a custom Url namer. The default implementation
  // performs sharding and appends '.pagespeed.<filter>.<hash>.<extension>'.
  virtual UrlNamer* DefaultUrlNamer();

  // Subclasses can override this to create an appropriately-sized thread
  // pool for their environment. The default implementation will always
  // make one with a single thread.
  virtual QueuedWorkerPool* CreateWorkerPool(WorkerPoolName name);

  // Subclasses can override this to create an appropriate Scheduler
  // subclass if the default isn't acceptable.
  virtual Scheduler* CreateScheduler();

  // Called before creating the url fetchers.
  virtual void FetcherSetupHooks();

  // Override this to return true if you do want the resource manager
  // to write resources to the filesystem.
  //
  // TODO(sligocki): Do we ever want that? Or is it a relic from a
  // forgotten time? I think we never want to write OutputResources
  // written to disk automatically any more.
  virtual bool ShouldWriteResourcesToFileSystem() { return false; }

  // Override this if you want to change what directory locks go into
  // when using the default filesystem-based lock manager. The default is
  // filename_prefix()
  virtual StringPiece LockFilePrefix();

 private:
  ResourceManager* CreateResourceManagerLockHeld();
  void SetupSlurpDirectories();
  void Init();  // helper-method for constructors.

  scoped_ptr<MessageHandler> html_parse_message_handler_;
  scoped_ptr<MessageHandler> message_handler_;
  scoped_ptr<FileSystem> file_system_;
  UrlFetcher* url_fetcher_;
  UrlAsyncFetcher* url_async_fetcher_;
  scoped_ptr<UrlFetcher> base_url_fetcher_;
  scoped_ptr<UrlAsyncFetcher> base_url_async_fetcher_;
  scoped_ptr<Hasher> hasher_;
  scoped_ptr<FilenameEncoder> filename_encoder_;
  scoped_ptr<UrlNamer> url_namer_;
  scoped_ptr<Timer> timer_;
  scoped_ptr<Scheduler> scheduler_;

  GoogleString filename_prefix_;
  GoogleString slurp_directory_;
  bool force_caching_;
  bool slurp_read_only_;
  bool slurp_print_urls_;
  bool async_rewrites_;

  // protected by resource_manager_mutex_;
  typedef std::set<ResourceManager*> ResourceManagerSet;
  ResourceManagerSet resource_managers_;
  scoped_ptr<AbstractMutex> resource_manager_mutex_;

  // Prior to computing the resource manager, which requires some options
  // to be set, we need a place to write the options.  These will be
  // permanently transferred to the ResourceManager when it is created.
  // This two-phase creation is needed to deal a variety of order-of-startup
  // issues across tests, Apache, and internal Google infrastructure.
  scoped_ptr<RewriteOptions> temp_options_;

  // Caching support
  scoped_ptr<HTTPCache> http_cache_;
  CacheInterface* http_cache_backend_;  // Pointer owned by http_cache_

  // Manage locks for output resources.
  scoped_ptr<NamedLockManager> lock_manager_;

  scoped_ptr<ThreadSystem> thread_system_;

  // Default statistics implementation which can be overridden by children
  // by calling SetStatistics().
  NullStatistics null_statistics_;
  Statistics* statistics_;

  StringSet created_directories_;

  std::vector<QueuedWorkerPool*> worker_pools_;

  // These must be initialized after the RewriteDriverFactory subclass has been
  // constructed so it can use a the statistics() override.
  scoped_ptr<RewriteStats> rewrite_stats_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_FACTORY_H_
