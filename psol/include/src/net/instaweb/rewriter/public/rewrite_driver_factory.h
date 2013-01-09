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

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/null_statistics.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractClientState;
class AbstractMutex;
class BlinkCriticalLineDataFinder;
class CriticalImagesFinder;
class FileSystem;
class FilenameEncoder;
class FlushEarlyInfoFinder;
class FuriousMatcher;
class Hasher;
class LogRecord;
class MessageHandler;
class NamedLockManager;
class PropertyCache;
class QueuedWorkerPool;
class ServerContext;
class RewriteDriver;
class RewriteOptions;
class RewriteStats;
class Scheduler;
class StaticJavascriptManager;
class Statistics;
class ThreadSystem;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class UrlNamer;
class UsageDataReporter;
class UserAgentMatcher;

// Manages the construction and ownership of most objects needed to create
// RewriteDrivers. If you have your own versions of these classes (specific
// implementations of UrlAsyncFetcher, Hasher, etc.) you can make your own
// subclass of RewriteDriverFactory to use these by default.
class RewriteDriverFactory {
 public:
  // Helper for users of defer_cleanup; see below.
  template<class T> class Deleter;

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

  // Initializes default options we want to hard-code into the
  // base-class to get consistency across deployments.  Subclasses
  // that override NewRewriteOptions() should call this method from
  // their constructor.  It is safe to call this multiple times.
  void InitializeDefaultOptions();

  virtual ~RewriteDriverFactory();

  // The RewriteDriveFactory will create objects of default type through the
  // New* method from drived classes.  Here are the objects that can be
  // replaced before creating the RewriteDriver.
  // Note: RewriteDriver takes ownership of these.
  void set_html_parse_message_handler(MessageHandler* message_handler);
  void set_message_handler(MessageHandler* message_handler);
  void set_file_system(FileSystem* file_system);
  void set_hasher(Hasher* hasher);
  void set_filename_encoder(FilenameEncoder* filename_encoder);
  void set_url_namer(UrlNamer* url_namer);
  void set_timer(Timer* timer);
  void set_usage_data_reporter(UsageDataReporter* reporter);

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

  MessageHandler* html_parse_message_handler();
  MessageHandler* message_handler();
  FileSystem* file_system();
  // TODO(sligocki): Remove hasher() and force people to make a NewHasher when
  // they need one.
  Hasher* hasher();
  FilenameEncoder* filename_encoder() { return filename_encoder_.get(); }
  UrlNamer* url_namer();
  UserAgentMatcher* user_agent_matcher();
  StaticJavascriptManager* static_javascript_manager();
  RewriteOptions* default_options() { return default_options_.get(); }

  // These accessors are *not* thread-safe.  They must be called once prior
  // to forking threads, e.g. via ComputeUrlFetcher().
  Timer* timer();
  NamedLockManager* lock_manager();
  QueuedWorkerPool* WorkerPool(WorkerPoolName pool);
  Scheduler* scheduler();
  UsageDataReporter* usage_data_reporter();

  // Computes URL fetchers using the based fetcher, and optionally,
  // slurp_directory and slurp_read_only.  These are not thread-safe;
  // they must be called once prior to spawning threads, e.g. via
  // CreateServerContext.
  virtual UrlFetcher* ComputeUrlFetcher();
  virtual UrlAsyncFetcher* ComputeUrlAsyncFetcher();

  // Threadsafe mechanism to create a managed ServerContext.  The
  // ServerContext is owned by the factory, and should not be
  // deleted directly.  Currently it is not possible to delete a
  // server context except by deleting the entire factory.
  ServerContext* CreateServerContext();

  // Initializes a ServerContext that has been new'd directly.  This
  // allows 2-phase initialization if required.  There is no need to
  // call this if you use CreateServerContext.
  void InitServerContext(ServerContext* server_context);

  // Called from InitServerContext, but virtualized separately as it is
  // platform-specific.  This method must call on the server context:
  // set_http_cache, set_metadata_cache, set_filesystem_metadata_cache, and
  // MakePropertyCaches.
  virtual void SetupCaches(ServerContext* server_context) = 0;

  // Provides an optional hook for adding rewrite passes to the HTML filter
  // chain.  This should be used for filters that are specific to a particular
  // RewriteDriverFactory implementation.
  virtual void AddPlatformSpecificRewritePasses(RewriteDriver* driver);

  // Provides an optional hook for adding rewriters to the .pagespeed. resource
  // decoding chain.  This should be used for rewriters that are specific to a
  // particular RewriteDriverFactory implementation.  The caller should only use
  // the resulting driver for reconstructing a .pagespeed. resource, not for
  // transforming HTML.  Therefore, implementations should add any
  // platform-specific rewriter whose id might appear in a .pagespeed. URL.
  virtual void AddPlatformSpecificDecodingPasses(RewriteDriver* driver);

  // Provides an optional hook for customizing the RewriteDriver object
  // using the options set on it. This is called before
  // RewriteDriver::AddFilters() and AddPlatformSpecificRewritePasses().
  virtual void ApplyPlatformSpecificConfiguration(RewriteDriver* driver);

  ThreadSystem* thread_system() { return thread_system_.get(); }

  // Returns the set of directories that we (our our subclasses) have created
  // thus far.
  const StringSet& created_directories() const {
    return created_directories_;
  }

  bool async_rewrites() { return true; }

  // Collection of global statistics objects.  This is thread-unsafe:
  // it must be called prior to spawning threads, and after any calls
  // to SetStatistics.  Failing that, it will be initialized in the
  // first call to InitServerContext(), which is thread-safe.
  RewriteStats* rewrite_stats();

  // statistics (default is NullStatistics).  This can be overridden by calling
  // SetStatistics, either from subclasses or externally.
  Statistics* statistics() { return statistics_; }

  // Initializes statistics variables.  This must be done at process
  // startup to enable shared memory segments in Apache to be set up.
  static void InitStats(Statistics* statistics);

  // Initializes static variables.  Initialize/Terminate calls must be paired.
  static void Initialize();
  static void Terminate();

  // Does *not* take ownership of Statistics.
  void SetStatistics(Statistics* stats);

  // Clean up all the factory-owned resources: fetchers, pools,
  // Server Contexts, the Drivers owned by the Server Contexts,
  // and worker threads.
  virtual void ShutDown();

  // Registers the directory as having been created by us.
  void AddCreatedDirectory(const GoogleString& dir);

  // Creates a new empty RewriteOptions object, with no default settings.
  // Note that InitResourceManager() will copy the factory's default_options()
  // into the server context's global_options(), but this method just provides
  // a blank set of options.
  virtual RewriteOptions* NewRewriteOptions();

  // Creates a new empty RewriteOptions object meant for use for
  // custom options from queries or headers. Default implementation just
  // forwards to NewRewriteOptions().
  virtual RewriteOptions* NewRewriteOptionsForQuery();

  // Creates a new LogRecord object. The caller of this method has to take
  // ownership of the returned LogRecord instance.
  virtual LogRecord* NewLogRecord();

  // get/set the version placed into the X-[Mod-]Page(s|-S)peed header.
  const GoogleString& version_string() const { return version_string_; }
  void set_version_string(const StringPiece& version_string) {
    version_string.CopyToString(&version_string_);
  }

  // Causes the given function to be Run after all the threads are shutdown,
  // in order to do any needed resource cleanups. The Deleter<T> template below
  // may be useful for object deletion cleanups.
  void defer_cleanup(Function* f) { deferred_cleanups_.push_back(f); }

  // Base method that returns true if the given ip is a debug ip.
  virtual bool IsDebugClient(const GoogleString& ip) const {
    return false;
  }

  // Creates a new AbstractClientState object that must be populated.
  // Subclasses can override this to create an appropriate AbstractClientState
  // subclass if the default isn't acceptable.
  virtual AbstractClientState* NewClientState();

  // Creates a FuriousMatcher, which is used to match clients or sessions to
  // a specific furious experiment.
  virtual FuriousMatcher* NewFuriousMatcher();

 protected:
  bool FetchersComputed() const;
  virtual void StopCacheActivity();
  StringPiece filename_prefix();

  // Used by subclasses to indicate that a ResourceManager has been
  // terminated.  Returns true if this was the last server context
  // known to this factory.
  bool TerminateServerContext(ServerContext* rm);

  // Implementors of RewriteDriverFactory must supply default definitions
  // for each of these methods, although they may be overridden via set_
  // methods above.
  virtual UrlFetcher* DefaultUrlFetcher() = 0;
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher() = 0;
  virtual MessageHandler* DefaultHtmlParseMessageHandler() = 0;
  virtual MessageHandler* DefaultMessageHandler() = 0;
  virtual FileSystem* DefaultFileSystem() = 0;
  virtual Timer* DefaultTimer() = 0;

  virtual Hasher* NewHasher() = 0;

  virtual CriticalImagesFinder* DefaultCriticalImagesFinder();

  // Default implementation returns NULL.
  virtual BlinkCriticalLineDataFinder* DefaultBlinkCriticalLineDataFinder(
      PropertyCache* cache);

  // Default implementation returns NULL.
  virtual FlushEarlyInfoFinder* DefaultFlushEarlyInfoFinder();

  // They may also supply a custom lock manager. The default implementation
  // will use the file system.
  virtual NamedLockManager* DefaultLockManager();

  // They may also supply a custom Url namer. The default implementation
  // performs sharding and appends '.pagespeed.<filter>.<hash>.<extension>'.
  virtual UrlNamer* DefaultUrlNamer();

  virtual UserAgentMatcher* DefaultUserAgentMatcher();
  virtual UsageDataReporter* DefaultUsageDataReporter();

  // Subclasses can override this to create an appropriately-sized thread
  // pool for their environment. The default implementation will always
  // make one with a single thread.
  virtual QueuedWorkerPool* CreateWorkerPool(WorkerPoolName name);

  // Subclasses can override this method to request load-shedding to happen
  // if the low-priority work pool has too many inactive sequences queued up
  // waiting (the returned value will be a threshold beyond which things
  // will start getting dropped). The default implementation returns
  // kNoLoadShedding, which disables the feature. See also
  // QueuedWorkerPool::set_load_shedding_threshold
  virtual int LowPriorityLoadSheddingThreshold() const;

  // Subclasses can override this to create an appropriate Scheduler
  // subclass if the default isn't acceptable.
  virtual Scheduler* CreateScheduler();

  // Called before creating the url fetchers.
  virtual void FetcherSetupHooks();

  // Override this if you want to change what directory locks go into
  // when using the default filesystem-based lock manager. The default is
  // filename_prefix()
  virtual StringPiece LockFilePrefix();

  // Initializes the StaticJavascriptManager.
  virtual void InitStaticJavascriptManager(
      StaticJavascriptManager* static_js_manager) {}

 private:
  // Creates a StaticJavascriptManager instance. Default implementation creates
  // an instance that disables serving of filter javascript via gstatic
  // (gstatic.com is the domain google uses for serving static content).
  StaticJavascriptManager* DefaultStaticJavascriptManager();

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
  scoped_ptr<UserAgentMatcher> user_agent_matcher_;
  scoped_ptr<StaticJavascriptManager> static_javascript_manager_;
  scoped_ptr<Timer> timer_;
  scoped_ptr<Scheduler> scheduler_;
  scoped_ptr<UsageDataReporter> usage_data_reporter_;

  GoogleString filename_prefix_;
  GoogleString slurp_directory_;
  bool force_caching_;
  bool slurp_read_only_;
  bool slurp_print_urls_;

  // protected by server_context_mutex_;
  typedef std::set<ServerContext*> ServerContextSet;
  ServerContextSet server_contexts_;
  scoped_ptr<AbstractMutex> server_context_mutex_;

  // Stores options with hard-coded defaults and adjustments from
  // the core system, subclasses, and command-line.
  scoped_ptr<RewriteOptions> default_options_;

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

  // To assist with subclass destruction-order, subclasses can register
  // functions to run late in the destructor.
  std::vector<Function*> deferred_cleanups_;

  // Version string to put into HTTP response headers.
  // TODO(sligocki): Remove. Redundant with RewriteOptions::x_header_value().
  GoogleString version_string_;

  // The hostname we're running on. Used to set the same field in ServerContext.
  GoogleString hostname_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriverFactory);
};

// Helper for users of RewriterDriverFactory::defer_cleanup --- instantiates
// into objects that call the appropriate delete operator when Run.
template<class T> class RewriteDriverFactory::Deleter : public Function {
 public:
  explicit Deleter(T* obj) : obj_(obj) {}
  virtual void Run() { delete obj_; }
 private:
  T* obj_;
  DISALLOW_COPY_AND_ASSIGN(Deleter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_FACTORY_H_
