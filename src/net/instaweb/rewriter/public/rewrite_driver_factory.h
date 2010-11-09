/**
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
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class CacheInterface;
class CacheUrlAsyncFetcher;
class CacheUrlFetcher;
class DelayController;
class FileDriver;
class FileSystem;
class FilenameEncoder;
class Hasher;
class HtmlParse;
class HTTPCache;
class LRUCache;
class MessageHandler;
class ResourceManager;
class RewriteDriver;
class Statistics;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class Variable;

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

  // Determines whether Slurping is enabled.
  bool slurping_enabled() const { return !slurp_directory_.empty(); }

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

  void set_filename_prefix(StringPiece p) { p.CopyToString(&filename_prefix_); }
  void set_url_prefix(StringPiece p) { p.CopyToString(&url_prefix_); }

  // TODO(jmarantz):
  // Remove all these methods in favor of simply exposing the RewriteOptions*.
  void set_num_shards(int num_shards) { options_.set_num_shards(num_shards); }
  void set_css_outline_min_bytes(int64 t) {
    options_.set_css_outline_min_bytes(t);
  }
  void set_js_outline_min_bytes(int64 t) {
    options_.set_js_outline_min_bytes(t);
  }
  void set_img_inline_max_bytes(int64 x) {
    options_.set_img_inline_max_bytes(x);
  }
  void set_css_inline_max_bytes(int64 x) {
    options_.set_css_inline_max_bytes(x);
  }
  void set_js_inline_max_bytes(int64 x) {
    options_.set_js_inline_max_bytes(x);
  }
  void set_beacon_url(const StringPiece& p) {
    options_.set_beacon_url(p);
  }

  MessageHandler* html_parse_message_handler();
  MessageHandler* message_handler();
  FileSystem* file_system();
  // TODO(sligocki): Remove hasher() and force people to make a NewHasher when
  // they need one.
  Hasher* hasher();
  FilenameEncoder* filename_encoder();
  Timer* timer();
  HTTPCache* http_cache();

  StringPiece filename_prefix();
  StringPiece url_prefix();
  int num_shards() const { return options_.num_shards(); }

  // Sets the rewrite level.
  void SetRewriteLevel(RewriteOptions::RewriteLevel level);

  // Adds an additional set of filters the enabled set.  Returns false
  // if any of the filter names are invalid, but all the valid ones
  // will be added anyway.
  bool AddEnabledFilters(const StringPiece& filter_names);

  // Adds an additional set of filters the disabled set.  Returns false
  // if any of the filter names are invalid, but all the valid ones
  // will be added anyway.
  bool AddDisabledFilters(const StringPiece& filter_names);

  // Computes URL fetchers using the based fetcher, and optionally,
  // slurp_directory and slurp_read_only.
  virtual UrlFetcher* ComputeUrlFetcher();
  virtual UrlAsyncFetcher* ComputeUrlAsyncFetcher();
  virtual ResourceManager* ComputeResourceManager();

  // Generates a new mutex, hasher.
  virtual AbstractMutex* NewMutex() = 0;
  virtual Hasher* NewHasher() = 0;

  // Generates a new managed RewriteDriver using the RewriteOptions
  // managed by this class.  Each RewriteDriver is not thread-safe,
  // but you can generate a RewriteDriver* for each thread.  The
  // returned drivers are deleted by the factory; they do not need to
  // be deleted by the allocator.
  RewriteDriver* NewRewriteDriver();

  // Releases a rewrite driver back into the pool.  These are free-listed
  // because they are not cheap to construct.
  void ReleaseRewriteDriver(RewriteDriver* rewrite_driver);

  // Generates a custom RewriteDriver using the passed-in options.  This
  // driver is *not* managed by the factory: you must delete it after
  // you are done with it.
  RewriteDriver* NewCustomRewriteDriver(const RewriteOptions& options);

  // Initialize statistics variables for 404 responses.
  static void Initialize(Statistics* statistics);
  // Increment the count of resource returning 404.
  void Increment404Count();
  // Increment the cournt of slurp returning 404.
  void IncrementSlurpCount();

  DomainLawyer* domain_lawyer() { return &domain_lawyer_; }
  const DomainLawyer* domain_lawyer() const { return &domain_lawyer_; }

 protected:
  virtual void AddPlatformSpecificRewritePasses(RewriteDriver* driver);
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
  virtual CacheInterface* DefaultCacheInterface() = 0;

  // Implementors of RewriteDriverFactory must supply two mutexes.
  virtual AbstractMutex* cache_mutex() = 0;
  virtual AbstractMutex* rewrite_drivers_mutex() = 0;

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

  // Override this to return false if you don't want the resource
  // manager to write resources to the filesystem.
  virtual bool ShouldWriteResourcesToFileSystem() { return true; }

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

  std::string filename_prefix_;
  std::string url_prefix_;
  std::string slurp_directory_;
  RewriteOptions options_;
  bool force_caching_;
  bool slurp_read_only_;

  scoped_ptr<ResourceManager> resource_manager_;

  // RewriteDrivers that were previously allocated, but have
  // been released with ReleaseRewriteDriver, and are ready
  // for re-use with NewRewriteDriver.
  std::vector<RewriteDriver*> available_rewrite_drivers_;

  // RewriteDrivers that are currently in use.  This is retained
  // as a sanity check to make sure our system is coherent,
  // and to facilitate complete cleanup if a Shutdown occurs
  // while a request is in flight.
  std::set<RewriteDriver*> active_rewrite_drivers_;

  // Caching support
  scoped_ptr<HTTPCache> http_cache_;
  scoped_ptr<CacheUrlFetcher> cache_fetcher_;
  scoped_ptr<CacheUrlAsyncFetcher> cache_async_fetcher_;
  Variable* resource_404_count_;
  Variable* slurp_404_count_;

  // Keep track of authorized domains, sharding, and mappings.
  DomainLawyer domain_lawyer_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriverFactory);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_DRIVER_FACTORY_H_
