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

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class CacheInterface;
class CacheUrlAsyncFetcher;
class CacheUrlFetcher;
class FileSystem;
class FilenameEncoder;
class Hasher;
class HtmlParse;
class HTTPCache;
class MessageHandler;
class NamedLockManager;
class ResourceManager;
class RewriteDriver;
class RewriteOptions;
class Statistics;
class ThreadSystem;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;

// A base RewriteDriverFactory.
class RewriteDriverFactory {
 public:
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

  RewriteOptions* options();
  MessageHandler* html_parse_message_handler();
  MessageHandler* message_handler();
  FileSystem* file_system();
  // TODO(sligocki): Remove hasher() and force people to make a NewHasher when
  // they need one.
  Hasher* hasher();
  FilenameEncoder* filename_encoder();
  Timer* timer();
  HTTPCache* http_cache();
  NamedLockManager* lock_manager();

  StringPiece filename_prefix();

  // Computes URL fetchers using the based fetcher, and optionally,
  // slurp_directory and slurp_read_only.
  virtual UrlFetcher* ComputeUrlFetcher();
  virtual UrlAsyncFetcher* ComputeUrlAsyncFetcher();
  ResourceManager* ComputeResourceManager();

  // Generates a new hasher.
  virtual Hasher* NewHasher() = 0;

  // See doc in resource_manager.cc.
  RewriteDriver* NewRewriteDriver();

  // Provides an optional hook for adding rewrite passes that are
  // specific to an implementation of RewriteDriverFactory.
  virtual void AddPlatformSpecificRewritePasses(RewriteDriver* driver);

  ThreadSystem* thread_system();

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

 protected:
  bool FetchersComputed() const;

  // Implementors of RewriteDriverFactory must supply default definitions
  // for each of these methods, although they may be overridden via set_
  // methods above
  virtual UrlFetcher* DefaultUrlFetcher() = 0;
  virtual UrlAsyncFetcher* DefaultAsyncUrlFetcher() = 0;
  virtual MessageHandler* DefaultHtmlParseMessageHandler() = 0;
  virtual MessageHandler* DefaultMessageHandler() = 0;
  virtual FileSystem* DefaultFileSystem() = 0;
  virtual Timer* DefaultTimer() = 0;
  virtual ThreadSystem* DefaultThreadSystem() = 0;

    // Note: Returned CacheInterface should be thread-safe.
  virtual CacheInterface* DefaultCacheInterface() = 0;

  // They may also supply a custom lock manager. The default implementation
  // will use the file system.
  virtual NamedLockManager* DefaultLockManager();

  // Overridable statistics (default is NullStatistics)
  virtual Statistics* statistics() { return &null_statistics_; }

  // Clean up all the resources. When shutdown Apache, and destroy the process
  // sub-pool.  The RewriteDriverFactory owns some elements that were created
  // from that sub-pool. The sub-pool is destroyed in ApacheRewriteFactory,
  // which happens before the destruction of the base class. When the base class
  // destroys, the sub-pool has been destroyed, but the elements in base class
  // are still trying to destroy the sub-pool of the sub-pool. Call this
  // function before destroying the process sub-pool.
  void ShutDown();

  // Called before creating the url fetchers.
  virtual void FetcherSetupHooks();

  // Called after creating the resource manager in ComputeResourceManager.
  virtual void ResourceManagerCreatedHook();

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

  // Registers the directory as having been created by us.
  void AddCreatedDirectory(const GoogleString& dir);

 private:
  void SetupSlurpDirectories();

  scoped_ptr<MessageHandler> html_parse_message_handler_;
  scoped_ptr<MessageHandler> message_handler_;
  scoped_ptr<FileSystem> file_system_;
  UrlFetcher* url_fetcher_;
  UrlAsyncFetcher* url_async_fetcher_;
  scoped_ptr<UrlFetcher> base_url_fetcher_;
  scoped_ptr<UrlAsyncFetcher> base_url_async_fetcher_;
  scoped_ptr<Hasher> hasher_;
  scoped_ptr<FilenameEncoder> filename_encoder_;
  scoped_ptr<Timer> timer_;

  HtmlParse* html_parse_;

  GoogleString filename_prefix_;
  GoogleString slurp_directory_;
  bool force_caching_;
  bool slurp_read_only_;
  bool slurp_print_urls_;
  bool async_rewrites_;

  scoped_ptr<ResourceManager> resource_manager_;

  // Prior to computing the resource manager, which requires some options
  // to be set, we need a place to write the options.  These will be
  // permanently transferred to the ResourceManager when it is created.
  // This two-phase creation is needed to deal a variety of order-of-startup
  // issues across tests, Apache, and internal Google infrastructure.
  scoped_ptr<RewriteOptions> temp_options_;

  // Caching support
  scoped_ptr<HTTPCache> http_cache_;
  CacheInterface* http_cache_backend_;  // Pointer owned by http_cache_
  scoped_ptr<CacheUrlFetcher> cache_fetcher_;
  scoped_ptr<CacheUrlAsyncFetcher> cache_async_fetcher_;

  // Keep track of authorized domains, sharding, and mappings.
  DomainLawyer domain_lawyer_;

  // Manage locks for output resources.
  scoped_ptr<NamedLockManager> lock_manager_;

  scoped_ptr<ThreadSystem> thread_system_;

  // Default statistics implementation, which can be overridden by children.
  NullStatistics null_statistics_;

  StringSet created_directories_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_FACTORY_H_
