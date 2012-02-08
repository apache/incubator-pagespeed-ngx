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

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_dump_url_fetcher.h"
#include "net/instaweb/http/public/http_dump_url_writer.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/url_fetcher.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/javascript_url_manager.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/filename_encoder.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class Statistics;

RewriteDriverFactory::RewriteDriverFactory(ThreadSystem* thread_system)
    : thread_system_(thread_system) {
  Init();
}

RewriteDriverFactory::RewriteDriverFactory()
    : thread_system_(ThreadSystem::CreateThreadSystem()) {
  Init();
}

void RewriteDriverFactory::Init() {
  url_fetcher_ = NULL;
  url_async_fetcher_ = NULL;
  force_caching_ = false;
  slurp_read_only_ = false;
  slurp_print_urls_ = false;
  enable_property_cache_ = false;
  SetStatistics(&null_statistics_);
  resource_manager_mutex_.reset(thread_system_->NewMutex());
  worker_pools_.assign(kNumWorkerPools, NULL);

  // Pre-initializes the default options.  IMPORTANT: subclasses overridding
  // NewRewriteOptions() should re-call this method from their constructor
  // so that the correct rewrite_options_ object gets reset.
  InitializeDefaultOptions();
}

void RewriteDriverFactory::InitializeDefaultOptions() {
  // We default to using the "core filters". Note that this is not
  // the only place the default is applied --- for directories with .htaccess
  // files it is given in create_dir_config in mod_instaweb.cc
  default_options_.reset(NewRewriteOptions());
  default_options_->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  default_options_->DisallowTroublesomeResources();
  // Note that we do not need to compute a signature on the default options.
  // We will never be serving requests with these options: they are just used
  // as a source for merging.
}

RewriteDriverFactory::~RewriteDriverFactory() {
  ShutDown();

  {
    ScopedMutex lock(resource_manager_mutex_.get());
    STLDeleteElements(&resource_managers_);
  }

  for (int c = 0; c < kNumWorkerPools; ++c) {
    delete worker_pools_[c];
    worker_pools_[c] = NULL;
  }

  // Avoid double-destructing the url fetchers if they were not overridden
  // programmatically
  if ((url_async_fetcher_ != NULL) &&
      (url_async_fetcher_ != base_url_async_fetcher_.get())) {
    delete url_async_fetcher_;
  }
  url_async_fetcher_ = NULL;
  if ((url_fetcher_ != NULL) && (url_fetcher_ != base_url_fetcher_.get())) {
    delete url_fetcher_;
  }
  url_fetcher_ = NULL;

  for (int i = 0, n = deferred_deletes_.size(); i < n; ++i) {
    deferred_deletes_[i]->CallRun();
  }
}

void RewriteDriverFactory::set_html_parse_message_handler(
    MessageHandler* message_handler) {
  html_parse_message_handler_.reset(message_handler);
}

void RewriteDriverFactory::set_message_handler(
    MessageHandler* message_handler) {
  message_handler_.reset(message_handler);
}

bool RewriteDriverFactory::FetchersComputed() const {
  return (url_fetcher_ != NULL) || (url_async_fetcher_ != NULL);
}

void RewriteDriverFactory::set_slurp_directory(const StringPiece& dir) {
  CHECK(!FetchersComputed())
      << "Cannot call set_slurp_directory "
      << " after ComputeUrl*Fetcher has been called";
  dir.CopyToString(&slurp_directory_);
}

void RewriteDriverFactory::set_slurp_read_only(bool read_only) {
  CHECK(!FetchersComputed())
      << "Cannot call set_slurp_read_only "
      << " after ComputeUrl*Fetcher has been called";
  slurp_read_only_ = read_only;
}

void RewriteDriverFactory::set_slurp_print_urls(bool print_urls) {
  CHECK(!FetchersComputed())
      << "Cannot call set_slurp_print_urls "
      << " after ComputeUrl*Fetcher has been called";
  slurp_print_urls_ = print_urls;
}

void RewriteDriverFactory::set_file_system(FileSystem* file_system) {
  file_system_.reset(file_system);
}

// TODO(jmarantz): Change this to set_base_url_fetcher
void RewriteDriverFactory::set_base_url_fetcher(UrlFetcher* url_fetcher) {
  CHECK(!FetchersComputed())
      << "Cannot call set_base_url_fetcher "
      << " after ComputeUrl*Fetcher has been called";
  CHECK(base_url_async_fetcher_.get() == NULL)
      << "Only call one of set_base_url_fetcher and set_base_url_async_fetcher";
  base_url_fetcher_.reset(url_fetcher);
}

void RewriteDriverFactory::set_base_url_async_fetcher(
    UrlAsyncFetcher* url_async_fetcher) {
  CHECK(!FetchersComputed())
      << "Cannot call set_base_url_fetcher "
      << " after ComputeUrl*Fetcher has been called";
  CHECK(base_url_fetcher_.get() == NULL)
      << "Only call one of set_base_url_fetcher and set_base_url_async_fetcher";
  base_url_async_fetcher_.reset(url_async_fetcher);
}

void RewriteDriverFactory::set_hasher(Hasher* hasher) {
  hasher_.reset(hasher);
  DCHECK(resource_managers_.empty());
}

void RewriteDriverFactory::set_timer(Timer* timer) {
  timer_.reset(timer);
}

void RewriteDriverFactory::set_filename_encoder(FilenameEncoder* e) {
  filename_encoder_.reset(e);
}

void RewriteDriverFactory::set_url_namer(UrlNamer* url_namer) {
  url_namer_.reset(url_namer);
}

void RewriteDriverFactory::set_critical_images_finder(
    CriticalImagesFinder* finder) {
  critical_images_finder_.reset(finder);
}

void RewriteDriverFactory::set_enable_property_cache(bool enabled) {
  enable_property_cache_ = enabled;
  if (property_cache_.get() != NULL) {
    property_cache_->set_enabled(enabled);
  }
}

MessageHandler* RewriteDriverFactory::html_parse_message_handler() {
  if (html_parse_message_handler_ == NULL) {
    html_parse_message_handler_.reset(DefaultHtmlParseMessageHandler());
  }
  return html_parse_message_handler_.get();
}

MessageHandler* RewriteDriverFactory::message_handler() {
  if (message_handler_ == NULL) {
    message_handler_.reset(DefaultMessageHandler());
  }
  return message_handler_.get();
}

FileSystem* RewriteDriverFactory::file_system() {
  if (file_system_ == NULL) {
    file_system_.reset(DefaultFileSystem());
  }
  return file_system_.get();
}

Timer* RewriteDriverFactory::timer() {
  if (timer_ == NULL) {
    timer_.reset(DefaultTimer());
  }
  return timer_.get();
}

UrlNamer* RewriteDriverFactory::url_namer() {
  if (url_namer_ == NULL) {
    url_namer_.reset(DefaultUrlNamer());
  }
  return url_namer_.get();
}

JavascriptUrlManager* RewriteDriverFactory::javascript_url_manager() {
  if (javascript_url_manager_ == NULL) {
    javascript_url_manager_.reset(DefaultJavascriptUrlManager());
  }
  return javascript_url_manager_.get();
}

CriticalImagesFinder* RewriteDriverFactory::critical_images_finder() {
  if (critical_images_finder_ == NULL) {
    critical_images_finder_.reset(DefaultCriticalImagesFinder());
  }
  return critical_images_finder_.get();
}

Scheduler* RewriteDriverFactory::scheduler() {
  if (scheduler_ == NULL) {
    scheduler_.reset(CreateScheduler());
  }
  return scheduler_.get();
}

Hasher* RewriteDriverFactory::hasher() {
  if (hasher_ == NULL) {
    hasher_.reset(NewHasher());
  }
  return hasher_.get();
}

NamedLockManager* RewriteDriverFactory::DefaultLockManager() {
  return new FileSystemLockManager(file_system(), LockFilePrefix(),
                                   scheduler(), message_handler());
}

UrlNamer* RewriteDriverFactory::DefaultUrlNamer() {
  return new UrlNamer();
}

JavascriptUrlManager* RewriteDriverFactory::DefaultJavascriptUrlManager() {
  return new JavascriptUrlManager(url_namer(), false, "");
}

CriticalImagesFinder* RewriteDriverFactory::DefaultCriticalImagesFinder() {
  return NULL;
}

QueuedWorkerPool* RewriteDriverFactory::CreateWorkerPool(WorkerPoolName pool) {
  return new QueuedWorkerPool(1, thread_system());
}

int RewriteDriverFactory::LowPriorityLoadSheddingThreshold() const {
  return QueuedWorkerPool::kNoLoadShedding;
}

Scheduler* RewriteDriverFactory::CreateScheduler() {
  return new Scheduler(thread_system(), timer());
}

NamedLockManager* RewriteDriverFactory::lock_manager() {
  if (lock_manager_ == NULL) {
    lock_manager_.reset(DefaultLockManager());
  }
  return lock_manager_.get();
}

QueuedWorkerPool* RewriteDriverFactory::WorkerPool(WorkerPoolName pool) {
  if (worker_pools_[pool] == NULL) {
    worker_pools_[pool] = CreateWorkerPool(pool);
    worker_pools_[pool]->set_queue_size_stat(
        rewrite_stats()->thread_queue_depth(pool));
    if (pool == kLowPriorityRewriteWorkers) {
      worker_pools_[pool]->SetLoadSheddingThreshold(
          LowPriorityLoadSheddingThreshold());
    }
  }

  return worker_pools_[pool];
}

bool RewriteDriverFactory::set_filename_prefix(StringPiece p) {
  p.CopyToString(&filename_prefix_);
  if (file_system()->IsDir(filename_prefix_.c_str(),
                           message_handler()).is_true()) {
    return true;
  }

  if (!file_system()->RecursivelyMakeDir(filename_prefix_, message_handler())) {
    message_handler()->FatalError(
        filename_prefix_.c_str(), 0,
        "Directory does not exist and cannot be created");
    return false;
  }

  AddCreatedDirectory(filename_prefix_);
  return true;
}

StringPiece RewriteDriverFactory::filename_prefix() {
  return filename_prefix_;
}


CacheInterface* RewriteDriverFactory::cache_backend() {
  if (cache_backend_.get() == NULL) {
    cache_backend_.reset(DefaultCacheInterface());
  }
  return cache_backend_.get();
}


HTTPCache* RewriteDriverFactory::http_cache() {
  if (http_cache_.get() == NULL) {
    http_cache_.reset(ComputeHTTPCache());
  }
  return http_cache_.get();
}

HTTPCache* RewriteDriverFactory::ComputeHTTPCache() {
  HTTPCache* http_cache = new HTTPCache(
      cache_backend(), timer(), hasher(), statistics());
  http_cache->set_force_caching(force_caching_);
  return http_cache;
}

PropertyCache* RewriteDriverFactory::MakePropertyCache(
    CacheInterface* cache) const {
  PropertyCache* pcache = new PropertyCache(cache, timer_.get(),
                                            thread_system_.get());
  pcache->set_enabled(enable_property_cache_);
  return pcache;
}

PropertyCache* RewriteDriverFactory::property_cache() {
  if (property_cache_.get() == NULL) {
    property_cache_.reset(MakePropertyCache(cache_backend()));
  }
  return property_cache_.get();
}

ResourceManager* RewriteDriverFactory::CreateResourceManager() {
  CHECK(!filename_prefix_.empty())
      << "Must specify --filename_prefix or call "
      << "RewriteDriverFactory::set_filename_prefix.";

  ResourceManager* resource_manager = new ResourceManager(this);
  InitResourceManager(resource_manager);
  return resource_manager;
}

void RewriteDriverFactory::InitResourceManager(
    ResourceManager* resource_manager) {
  ScopedMutex lock(resource_manager_mutex_.get());

  resource_manager->ComputeSignature(resource_manager->global_options());
  resource_manager->set_scheduler(scheduler());
  resource_manager->set_statistics(statistics());
  resource_manager->set_rewrite_stats(rewrite_stats());
  if (resource_manager->http_cache() == NULL) {
    // In Apache we can potentially have distinct caches per
    // VirtualHost, which must be set prior to calling Init.
    resource_manager->set_http_cache(http_cache());
  }
  if (resource_manager->property_cache() == NULL) {
    resource_manager->set_property_cache(property_cache());
  }
  if (resource_manager->metadata_cache() == NULL) {
    resource_manager->set_metadata_cache(cache_backend());
  }
  if (resource_manager->lock_manager() == NULL) {
    resource_manager->set_lock_manager(lock_manager());
  }
  if (resource_manager->url_async_fetcher() == NULL) {
    resource_manager->set_url_async_fetcher(ComputeUrlAsyncFetcher());
  }
  resource_manager->set_url_namer(url_namer());
  resource_manager->set_filename_encoder(filename_encoder());
  resource_manager->set_file_system(file_system());
  resource_manager->set_filename_prefix(filename_prefix_);
  resource_manager->set_hasher(hasher());
  resource_manager->set_message_handler(message_handler());
  resource_manager->set_javascript_url_manager(
      javascript_url_manager());
  resource_manager->InitWorkersAndDecodingDriver();
  resource_managers_.insert(resource_manager);
}

void RewriteDriverFactory::AddPlatformSpecificDecodingPasses(
    RewriteDriver* driver) {
}

void RewriteDriverFactory::AddPlatformSpecificRewritePasses(
    RewriteDriver* driver) {
}

UrlFetcher* RewriteDriverFactory::ComputeUrlFetcher() {
  if (url_fetcher_ == NULL) {
    // Run any hooks like setting up slurp directory.
    FetcherSetupHooks();
    if (slurp_directory_.empty()) {
      if (base_url_fetcher_.get() == NULL) {
        url_fetcher_ = DefaultUrlFetcher();
      } else {
        url_fetcher_ = base_url_fetcher_.get();
      }
    } else {
      SetupSlurpDirectories();
    }
  }
  return url_fetcher_;
}

UrlAsyncFetcher* RewriteDriverFactory::ComputeUrlAsyncFetcher() {
  if (url_async_fetcher_ == NULL) {
    // Run any hooks like setting up slurp directory.
    FetcherSetupHooks();
    if (slurp_directory_.empty()) {
      if (base_url_async_fetcher_.get() == NULL) {
        url_async_fetcher_ = DefaultAsyncUrlFetcher();
      } else {
        url_async_fetcher_ = base_url_async_fetcher_.get();
      }
    } else {
      SetupSlurpDirectories();
    }
  }
  return url_async_fetcher_;
}

void RewriteDriverFactory::SetupSlurpDirectories() {
  CHECK(!FetchersComputed());
  if (slurp_read_only_) {
    CHECK(!FetchersComputed());
    HttpDumpUrlFetcher* dump_fetcher = new HttpDumpUrlFetcher(
        slurp_directory_, file_system(), timer());
    dump_fetcher->set_print_urls(slurp_print_urls_);
    url_fetcher_ = dump_fetcher;
  } else {
    // Check to see if the factory already had set_base_url_fetcher
    // called on it.  If so, then we'll want to use that fetcher
    // as the mechanism for the dump-writer to retrieve missing
    // content from the internet so it can be saved in the slurp
    // directory.
    url_fetcher_ = base_url_fetcher_.get();
    if (url_fetcher_ == NULL) {
      url_fetcher_ = DefaultUrlFetcher();
    }
    HttpDumpUrlWriter* dump_writer = new HttpDumpUrlWriter(
        slurp_directory_, url_fetcher_, file_system(), timer());
    dump_writer->set_print_urls(slurp_print_urls_);
    url_fetcher_ = dump_writer;
  }

  // We do not use real async fetches when slurping.
  url_async_fetcher_ = new FakeUrlAsyncFetcher(url_fetcher_);
}

void RewriteDriverFactory::FetcherSetupHooks() {
}

StringPiece RewriteDriverFactory::LockFilePrefix() {
  return filename_prefix_;
}

void RewriteDriverFactory::StopCacheWrites() {
  ScopedMutex lock(resource_manager_mutex_.get());

  // Make sure we tell HTTP cache not to write out fetch failures, as
  // fetcher shutdown may create artificial ones, and we don't want to
  // remember those.
  //
  // Note that we also cannot access our own http_cache_ since it may be
  // NULL in case like Apache where resource managers get their own.
  for (ResourceManagerSet::iterator p = resource_managers_.begin();
       p != resource_managers_.end(); ++p) {
    HTTPCache* cache = (*p)->http_cache();
    if (cache != NULL) {
      cache->SetIgnoreFailurePuts();
    }
  }

  // Similarly stop metadata cache writes.
  for (ResourceManagerSet::iterator p = resource_managers_.begin();
       p != resource_managers_.end(); ++p) {
    ResourceManager* resource_manager = *p;
    resource_manager->set_metadata_cache_readonly();
  }
}

void RewriteDriverFactory::ShutDown() {
  StopCacheWrites();  // Maybe already stopped, but no harm stopping them twice.

  // We first shutdown the low-priority rewrite threads, as they're meant to
  // be robust against cancellation, and it will make the jobs wrap up
  // much quicker.
  if (worker_pools_[kLowPriorityRewriteWorkers] != NULL) {
    worker_pools_[kLowPriorityRewriteWorkers]->ShutDown();
  }

  // Now get active RewriteDrivers for each manager to wrap up.
  for (ResourceManagerSet::iterator p = resource_managers_.begin();
       p != resource_managers_.end(); ++p) {
    ResourceManager* resource_manager = *p;
    resource_manager->ShutDownDrivers();
  }

  // Shut down the remaining worker threads, to quiesce the system while
  // leaving the QueuedWorkerPool & QueuedWorkerPool::Sequence objects
  // live.  The QueuedWorkerPools will be deleted when the ResourceManager
  // is destructed.
  for (int i = 0, n = worker_pools_.size(); i < n; ++i) {
    QueuedWorkerPool* worker_pool = worker_pools_[i];
    if (worker_pool != NULL) {
      worker_pool->ShutDown();
    }
  }
}

void RewriteDriverFactory::AddCreatedDirectory(const GoogleString& dir) {
  created_directories_.insert(dir);
}

void RewriteDriverFactory::Initialize(Statistics* statistics) {
  if (statistics != NULL) {
    RewriteStats::Initialize(statistics);
    HTTPCache::Initialize(statistics);
    RewriteDriver::Initialize(statistics);
  }
}

void RewriteDriverFactory::Terminate() {
  RewriteDriver::Terminate();
}

void RewriteDriverFactory::SetStatistics(Statistics* statistics) {
  statistics_ = statistics;
  rewrite_stats_.reset(NULL);
}

RewriteStats* RewriteDriverFactory::rewrite_stats() {
  if (rewrite_stats_.get() == NULL) {
    rewrite_stats_.reset(new RewriteStats(statistics_, thread_system_.get(),
                                          timer()));
  }
  return rewrite_stats_.get();
}

RewriteOptions* RewriteDriverFactory::NewRewriteOptions() {
  return new RewriteOptions;
}

}  // namespace net_instaweb
