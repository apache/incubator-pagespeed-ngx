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
//
// Author: jmarantz@google.com (Joshua Marantz)
//         lsong@google.com (Libo Song)

#include "net/instaweb/apache/apache_rewrite_driver_factory.h"

#include "apr_pools.h"
#include "httpd.h"

#include "net/instaweb/apache/apache_cache.h"
#include "net/instaweb/apache/apache_message_handler.h"
#include "net/instaweb/apache/apache_thread_system.h"
#include "net/instaweb/apache/apr_file_system.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/http_dump_url_fetcher.h"
#include "net/instaweb/http/public/http_dump_url_writer.h"
#include "net/instaweb/http/public/sync_fetcher_adapter.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#ifndef NDEBUG
#include "net/instaweb/util/public/checking_thread_system.h"
#endif
#include "net/instaweb/util/public/file_cache.h"
#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/hashed_referer_statistics.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/null_shared_mem.h"
#include "net/instaweb/util/public/pthread_shared_mem.h"
#include "net/instaweb/util/public/shared_mem_lock_manager.h"
#include "net/instaweb/util/public/shared_mem_referer_statistics.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/write_through_cache.h"

namespace net_instaweb {

namespace {

const size_t kRefererStatisticsNumberOfPages = 1024;
const size_t kRefererStatisticsAverageUrlLength = 64;

// Statistics histogram names.
const char* kHtmlRewriteTimeHistogram = "Html Time us Histogram";

}  // namespace

ApacheRewriteDriverFactory::ApacheRewriteDriverFactory(
    server_rec* server, const StringPiece& version)
    : RewriteDriverFactory(
#ifdef NDEBUG
        new ApacheThreadSystem
#else
        new CheckingThreadSystem(new ApacheThreadSystem)
#endif
                           ),
      server_rec_(server),
#ifdef PAGESPEED_SUPPORT_POSIX_SHARED_MEM
      shared_mem_runtime_(new PthreadSharedMem()),
#else
      shared_mem_runtime_(new NullSharedMem()),
#endif
      shared_circular_buffer_(NULL),
      version_(version.data(), version.size()),
      statistics_frozen_(false),
      is_root_process_(true),
      shared_mem_referer_statistics_(NULL),
      hostname_identifier_(StrCat(server->server_hostname,
                                  ":",
                                  IntegerToString(server->port))),
      apache_message_handler_(new ApacheMessageHandler(
          server_rec_, version_, timer())),
      apache_html_parse_message_handler_(new ApacheMessageHandler(
          server_rec_, version_, timer())),
      html_rewrite_time_us_histogram_(NULL),
      message_buffer_size_(0) {
  apr_pool_create(&pool_, NULL);

  // Make sure the ownership of apache_message_handler_ and
  // apache_html_parse_message_handler_ is given to scoped pointer.
  // Otherwise may result in leak error in test.
  message_handler();
  html_parse_message_handler();
  InitializeDefaultOptions();
}

ApacheRewriteDriverFactory::~ApacheRewriteDriverFactory() {
  // Finish up any background tasks and stop accepting new ones. This ensures
  // that as soon as the first ApacheRewriteDriverFactory is shutdown we
  // no longer have to worry about outstanding jobs in the slow_worker_ trying
  // to access FileCache and similar objects we're about to blow away.
  if (!is_root_process_) {
    slow_worker_->ShutDown();
  }

  // We free all the resources before destroying the pool, because some of the
  // resource uses the sub-pool and will need that pool to be around to
  // clean up properly.
  ShutDown();

  apr_pool_destroy(pool_);

  // We still have registered a pool deleter here, right?  This seems risky...
  STLDeleteElements(&uninitialized_managers_);

  for (PathCacheMap::iterator p = path_cache_map_.begin(),
           e = path_cache_map_.end(); p != e; ++p) {
    ApacheCache* cache = p->second;
    defer_delete(new Deleter<ApacheCache>(cache));
  }
  shared_mem_statistics_.reset(NULL);
}

ApacheCache* ApacheRewriteDriverFactory::GetCache(ApacheConfig* config) {
  const GoogleString& path = config->file_cache_path();
  std::pair<PathCacheMap::iterator, bool> result = path_cache_map_.insert(
      PathCacheMap::value_type(path, static_cast<ApacheCache*>(NULL)));
  PathCacheMap::iterator iter = result.first;
  if (result.second) {
    iter->second = new ApacheCache(path, *config, this);
  }
  return iter->second;
}

FileSystem* ApacheRewriteDriverFactory::DefaultFileSystem() {
  // Pass in NULL for the pool.  We do not want the file-system to
  // be auto-destructed based on the factory's pool: we want to follow
  // C++ destruction semantics.
  return new AprFileSystem(NULL, thread_system());
}

Hasher* ApacheRewriteDriverFactory::NewHasher() {
  return new MD5Hasher();
}

Timer* ApacheRewriteDriverFactory::DefaultTimer() {
  return new AprTimer();
}

MessageHandler* ApacheRewriteDriverFactory::DefaultHtmlParseMessageHandler() {
  return apache_html_parse_message_handler_;
}

MessageHandler* ApacheRewriteDriverFactory::DefaultMessageHandler() {
  return apache_message_handler_;
}

// Note: DefaultCacheInterface should return a thread-safe cache object.
CacheInterface* ApacheRewriteDriverFactory::DefaultCacheInterface() {
  LOG(DFATAL) << "In Apache the cache is owned by ApacheCache, not the factory";
  return NULL;
}

NamedLockManager* ApacheRewriteDriverFactory::DefaultLockManager() {
  LOG(DFATAL) << "In Apache locks are owned by ApacheCache, not the factory";
  return NULL;
}

UrlFetcher* ApacheRewriteDriverFactory::DefaultUrlFetcher() {
  LOG(DFATAL) << "In Apache the fetchers are not global, but kept in a map.";
  return NULL;
}

UrlAsyncFetcher* ApacheRewriteDriverFactory::DefaultAsyncUrlFetcher() {
  LOG(DFATAL) << "In Apache the fetchers are not global, but kept in a map.";
  return NULL;
}

UrlPollableAsyncFetcher* ApacheRewriteDriverFactory::GetFetcher(
    ApacheConfig* config) {
  const GoogleString& proxy = config->fetcher_proxy();

  // Fetcher-key format: "[(R|W)slurp_directory][\nproxy]"
  GoogleString key;
  if (config->slurping_enabled()) {
    if (config->slurp_read_only()) {
      key = StrCat("R", config->slurp_directory());
    } else {
      key = StrCat("W", config->slurp_directory());
    }
  }
  if (!proxy.empty()) {
    StrAppend(&key, "\n", proxy);
  }

  std::pair<FetcherMap::iterator, bool> result = fetcher_map_.insert(
      FetcherMap::value_type(key, static_cast<UrlPollableAsyncFetcher*>(NULL)));
  FetcherMap::iterator iter = result.first;
  if (result.second) {
    UrlPollableAsyncFetcher* fetcher = NULL;
    if (config->slurping_enabled()) {
      if (config->slurp_read_only()) {
        HttpDumpUrlFetcher* dump_fetcher = new HttpDumpUrlFetcher(
            config->slurp_directory(), file_system(), timer());
        defer_delete(new Deleter<HttpDumpUrlFetcher>(dump_fetcher));
        fetcher = new FakeUrlAsyncFetcher(dump_fetcher);
      } else {
        // Make a copy of the passed-in config with the slurp directory
        // erased, and use that to construct the base fetcher.
        ApacheConfig no_slurp_config("");
        no_slurp_config.Merge(*config, *config);
        no_slurp_config.set_slurp_directory("");
        UrlPollableAsyncFetcher* base_fetcher = GetFetcher(&no_slurp_config);

        UrlFetcher* sync_fetcher = new SyncFetcherAdapter(
            timer(), config->fetcher_time_out_ms(), base_fetcher,
            thread_system());
        defer_delete(new Deleter<UrlFetcher>(sync_fetcher));
        HttpDumpUrlWriter* dump_writer = new HttpDumpUrlWriter(
            config->slurp_directory(), sync_fetcher, file_system(), timer());
        defer_delete(new Deleter<HttpDumpUrlWriter>(dump_writer));
        fetcher = new FakeUrlAsyncFetcher(dump_writer);
      }
    } else {
      fetcher = new SerfUrlAsyncFetcher(
          proxy.c_str(),
          NULL,  // Do not use the Factory pool so we can control deletion.
          thread_system(), statistics(), timer(),
          config->fetcher_time_out_ms());
    }
    iter->second = fetcher;
  }
  return iter->second;
}

// TODO(jmarantz): make this per-vhost.
void ApacheRewriteDriverFactory::SharedCircularBufferInit(bool is_root) {
  // Set buffer size to 0 means turning it off
  if (shared_mem_runtime() != NULL && (message_buffer_size_ != 0)) {
    shared_circular_buffer_.reset(new SharedCircularBuffer(
        shared_mem_runtime(),
        message_buffer_size_,
        filename_prefix().as_string(),
        hostname_identifier()));
    if (shared_circular_buffer_->InitSegment(is_root, message_handler())) {
      apache_message_handler_->set_buffer(shared_circular_buffer_.get());
      apache_html_parse_message_handler_->set_buffer(
          shared_circular_buffer_.get());
     }
  }
}

// Temporarily disable shared-mem-referrers stuff until we get the rest the
// one-factory-per-process change in.
#define ENABLE_REFERER_STATS 0

void ApacheRewriteDriverFactory::SharedMemRefererStatisticsInit(bool is_root) {
#if ENABLE_REFERER_STATS
  if (shared_mem_runtime_ != NULL && config_->collect_referer_statistics()) {
    if (config_->hash_referer_statistics()) {
      // By making the hashes equal roughly to half the expected url length,
      // entries corresponding to referrals in the
      // shared_mem_referer_statistics_ map will be roughly the expected size
      //   The size of the hash might be capped, so we check for this and cap
      // expected average url length if necessary.
      //
      // hostname_identifier() is passed in as a suffix so that the shared
      // memory segments for different v-hosts have unique identifiers, keeping
      // the statistics separate.
      Hasher* hasher =
          new MD5Hasher(kRefererStatisticsAverageUrlLength / 2);
      size_t referer_statistics_average_expected_url_length_ =
          2 * hasher->HashSizeInChars();
      shared_mem_referer_statistics_.reset(new HashedRefererStatistics(
          kRefererStatisticsNumberOfPages,
          referer_statistics_average_expected_url_length_,
          shared_mem_runtime(),
          filename_prefix().as_string(),
          hostname_identifier(),
          hasher));
    } else {
      shared_mem_referer_statistics_.reset(new SharedMemRefererStatistics(
          kRefererStatisticsNumberOfPages,
          kRefererStatisticsAverageUrlLength,
          shared_mem_runtime(),
          filename_prefix().as_string(),
          hostname_identifier()));
    }
    if (!shared_mem_referer_statistics_->InitSegment(is_root,
                                                     message_handler())) {
      shared_mem_referer_statistics_.reset(NULL);
    }
  }
#endif
}

void ApacheRewriteDriverFactory::ParentOrChildInit() {
  SharedCircularBufferInit(is_root_process_);
  SharedMemRefererStatisticsInit(is_root_process_);
}

void ApacheRewriteDriverFactory::RootInit() {
  ParentOrChildInit();
  for (ApacheResourceManagerSet::iterator p = uninitialized_managers_.begin(),
           e = uninitialized_managers_.end(); p != e; ++p) {
    ApacheResourceManager* resource_manager = *p;

    // Determine the set of caches needed based on the unique
    // file_cache_path()s in the manager configurations.  We ignore
    // the GetCache return value because our goal is just to populate
    // the map which we'll iterate on below.
    GetCache(resource_manager->config());
  }
  for (PathCacheMap::iterator p = path_cache_map_.begin(),
           e = path_cache_map_.end(); p != e; ++p) {
    ApacheCache* cache = p->second;
    cache->RootInit();
  }
}

void ApacheRewriteDriverFactory::ChildInit() {
  is_root_process_ = false;
  ParentOrChildInit();
  // Reinitialize pid for child process.
  apache_message_handler_->SetPidString(static_cast<int64>(getpid()));
  apache_html_parse_message_handler_->SetPidString(
      static_cast<int64>(getpid()));
  slow_worker_.reset(new SlowWorker(thread_system()));
  if (shared_mem_statistics_.get() != NULL) {
    shared_mem_statistics_->Init(false, message_handler());
  }

  for (PathCacheMap::iterator p = path_cache_map_.begin(),
           e = path_cache_map_.end(); p != e; ++p) {
    ApacheCache* cache = p->second;
    cache->ChildInit();
  }
  for (ApacheResourceManagerSet::iterator p = uninitialized_managers_.begin(),
           e = uninitialized_managers_.end(); p != e; ++p) {
    ApacheResourceManager* resource_manager = *p;
    resource_manager->ChildInit();
  }
  uninitialized_managers_.clear();
}

void ApacheRewriteDriverFactory::DumpRefererStatistics(Writer* writer) {
#if ENABLE_REFERER_STATS
  // Note: Referer statistics are only displayed for within the same v-host
  MessageHandler* handler = message_handler();
  if (shared_mem_referer_statistics_ == NULL) {
    writer->Write("mod_pagespeed referer statistics either had an error or "
                  "are not enabled.", handler);
  } else {
    switch (config_->referer_statistics_output_level()) {
      case ApacheConfig::kFast:
        shared_mem_referer_statistics_->DumpFast(writer, handler);
        break;
      case ApacheConfig::kSimple:
        shared_mem_referer_statistics_->DumpSimple(writer, handler);
        break;
      case ApacheConfig::kOrganized:
        shared_mem_referer_statistics_->DumpOrganized(writer, handler);
        break;
    }
  }
#endif
}

void ApacheRewriteDriverFactory::ShutDown() {
  StopCacheWrites();

  // Next, we shutdown the fetchers before killing the workers in
  // RewriteDriverFactory::ShutDown; this is so any rewrite jobs in progress
  // can quickly wrap up.
  for (FetcherMap::iterator p = fetcher_map_.begin(), e = fetcher_map_.end();
       p != e; ++p) {
    UrlAsyncFetcher* fetcher = p->second;
    fetcher->ShutDown();
    defer_delete(new Deleter<UrlAsyncFetcher>(fetcher));
  }
  fetcher_map_.clear();

  if (is_root_process_) {
    // Cleanup statistics.
    // TODO(morlovich): This looks dangerous with async.
    if (shared_mem_statistics_.get() != NULL) {
      shared_mem_statistics_->GlobalCleanup(message_handler());
    }
    // Cleanup SharedCircularBuffer.
    // Use GoogleMessageHandler instead of ApacheMessageHandler.
    // As we are cleaning SharedCircularBuffer, we do not want to write to its
    // buffer and passing ApacheMessageHandler here may cause infinite loop.
    GoogleMessageHandler handler;
    if (shared_circular_buffer_ != NULL) {
      shared_circular_buffer_->GlobalCleanup(&handler);
    }
  }

  // Reset SharedCircularBuffer to NULL, so that any shutdown warnings
  // (e.g. in ResourceManager::ShutDownDrivers) don't reference
  // deleted objects as the base-class is deleted.
  apache_message_handler_->set_buffer(NULL);
  apache_html_parse_message_handler_->set_buffer(NULL);
  RewriteDriverFactory::ShutDown();
}

// Initializes global statistics object if needed, using factory to
// help with the settings if needed.
// Note: does not call set_statistics() on the factory.
Statistics* ApacheRewriteDriverFactory::MakeSharedMemStatistics() {
  if (shared_mem_statistics_.get() == NULL) {
    // Note that we create the statistics object in the parent process, and
    // it stays around in the kids but gets reinitialized for them
    // with a call to InitVariables(false) inside pagespeed_child_init.
    shared_mem_statistics_.reset(new SharedMemStatistics(
        shared_mem_runtime(), filename_prefix().as_string()));
    Initialize(shared_mem_statistics_.get());
    shared_mem_statistics_->AddHistogram(kHtmlRewriteTimeHistogram);
    shared_mem_statistics_->Init(true, message_handler());
    html_rewrite_time_us_histogram_ = shared_mem_statistics_->GetHistogram(
        kHtmlRewriteTimeHistogram);
    html_rewrite_time_us_histogram_->SetMaxValue(200 * Timer::kMsUs);
  }
  DCHECK(!statistics_frozen_);
  statistics_frozen_ = true;
  SetStatistics(shared_mem_statistics_.get());
  return shared_mem_statistics_.get();
}

void ApacheRewriteDriverFactory::Initialize(Statistics* statistics) {
  RewriteDriverFactory::Initialize(statistics);
  SerfUrlAsyncFetcher::Initialize(statistics);
}

void ApacheRewriteDriverFactory::AddHtmlRewriteTimeUs(int64 rewrite_time_us) {
  if (html_rewrite_time_us_histogram_ != NULL) {
    html_rewrite_time_us_histogram_->Add(rewrite_time_us);
  }
}

ApacheResourceManager* ApacheRewriteDriverFactory::MakeApacheResourceManager(
    server_rec* server) {
  ApacheResourceManager* rm = new ApacheResourceManager(this, server, version_);
  uninitialized_managers_.insert(rm);
  return rm;
}

bool ApacheRewriteDriverFactory::PoolDestroyed(ApacheResourceManager* rm) {
  if (uninitialized_managers_.erase(rm) == 1) {
    delete rm;
  }
  return uninitialized_managers_.empty();
}

RewriteOptions* ApacheRewriteDriverFactory::NewRewriteOptions() {
  return new ApacheConfig(hostname_identifier_);
}

}  // namespace net_instaweb
