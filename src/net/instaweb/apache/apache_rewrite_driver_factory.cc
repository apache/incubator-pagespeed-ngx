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

#include "net/instaweb/apache/apache_rewrite_driver_factory.h"

#include "apr_pools.h"
#include "httpd.h"

#include "net/instaweb/apache/apache_message_handler.h"
#include "net/instaweb/apache/apache_thread_system.h"
#include "net/instaweb/apache/apr_file_system.h"
#include "net/instaweb/apache/apr_mutex.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
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
#include "net/instaweb/util/public/pthread_shared_mem.h"
#include "net/instaweb/util/public/shared_mem_lock_manager.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/write_through_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const size_t kRefererStatisticsNumberOfPages = 1024;
const size_t kRefererStatisticsAverageUrlLength = 64;

}  // namespace

SharedMemOwnerMap* ApacheRewriteDriverFactory::lock_manager_owners_ = NULL;
RefCountedOwner<SlowWorker>::Family
    ApacheRewriteDriverFactory::slow_worker_family_;

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
      serf_url_fetcher_(NULL),
      serf_url_async_fetcher_(NULL),
      shared_mem_statistics_(NULL),
      shared_mem_runtime_(new PthreadSharedMem()),
      shared_circular_buffer_(NULL),
      slow_worker_(&slow_worker_family_),
      version_(version.data(), version.size()),
      statistics_frozen_(false),
      owns_statistics_(false),
      is_root_process_(true),
      shared_mem_referer_statistics_(NULL),
      hostname_identifier_(StrCat(server->server_hostname,
                                  ":",
                                  IntegerToString(server->port))),
      apache_message_handler_(new ApacheMessageHandler(
          server_rec_, version_, timer())),
      apache_html_parse_message_handler_(new ApacheMessageHandler(
          server_rec_, version_, timer())),
      shared_mem_lock_manager_lifecycler_(
          this,
          &ApacheRewriteDriverFactory::CreateSharedMemLockManager,
          "lock manager",
          &lock_manager_owners_),
      config_(new ApacheConfig(hostname_identifier_)) {
  apr_pool_create(&pool_, NULL);

  // In Apache, we default to using the "core filters". Note that this is not
  // the only place the default is applied --- for directories with .htaccess
  // files it is given in create_dir_config in mod_instaweb.cc
  config_->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
  // Make sure the ownership of apache_message_handler_ and
  // apache_html_parse_message_handler_ is given to scoped pointer.
  // Otherwise may result in leak error in test.
  message_handler();
  html_parse_message_handler();
}

ApacheRewriteDriverFactory::~ApacheRewriteDriverFactory() {
  // Finish up any background tasks and stop accepting new ones. This ensures
  // that as soon as the first ApacheRewriteDriverFactory is shutdown we
  // no longer have to worry about outstanding jobs in the slow_worker_ trying
  // to access FileCache and similar objects we're about to blow away.
  if (!is_root_process_) {
    slow_worker_.Get()->ShutDown();
  }

  // We free all the resources before destroying the pool, because some of the
  // resource uses the sub-pool and will need that pool to be around to
  // clean up properly.
  ShutDown();

  apr_pool_destroy(pool_);
}

SharedMemLockManager* ApacheRewriteDriverFactory::CreateSharedMemLockManager() {
  return new SharedMemLockManager(shared_mem_runtime_.get(),
                                  StrCat(config_->file_cache_path(),
                                         "/named_locks"),
                                  scheduler(), hasher(), message_handler());
}

FileSystem* ApacheRewriteDriverFactory::DefaultFileSystem() {
  // Pass in NULL for the pool.  We do not want the file-system to
  // be auto-destructed based on the factory's pool: we want to follow
  // C++ destruction semantics.
  return new AprFileSystem(NULL);
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

bool ApacheRewriteDriverFactory::InitFileCachePath() {
  if (file_system()->IsDir(config_->file_cache_path().c_str(),
                           message_handler()).is_true()) {
    return true;
  }
  bool ok = file_system()->RecursivelyMakeDir(config_->file_cache_path(),
                                              message_handler());
  if (ok) {
    AddCreatedDirectory(config_->file_cache_path());
  }
  return ok;
}

// Note: DefaultCacheInterface should return a thread-safe cache object.
CacheInterface* ApacheRewriteDriverFactory::DefaultCacheInterface() {
  FileCache::CachePolicy* policy = new FileCache::CachePolicy(
      timer(),
      config_->file_cache_clean_interval_ms(),
      config_->file_cache_clean_size_kb());
  CacheInterface* cache = new FileCache(
      config_->file_cache_path(), file_system(), slow_worker_.Get(),
      filename_encoder(), policy, message_handler());
  if (config_->lru_cache_kb_per_process() != 0) {
    LRUCache* lru_cache = new LRUCache(
        config_->lru_cache_kb_per_process() * 1024);

    // We only add the threadsafe-wrapper to the LRUCache.  The FileCache
    // is naturally thread-safe because it's got no writable member variables.
    // And surrounding that slower-running class with a mutex would likely
    // cause contention.
    ThreadsafeCache* ts_cache =
        new ThreadsafeCache(lru_cache, thread_system()->NewMutex());
    WriteThroughCache* write_through_cache =
        new WriteThroughCache(ts_cache, cache);
    // By default, WriteThroughCache does not limit the size of entries going
    // into its front cache.
    if (config_->lru_cache_byte_limit() != 0) {
      write_through_cache->set_cache1_limit(config_->lru_cache_byte_limit());
    }
    cache = write_through_cache;
  }
  return cache;
}

NamedLockManager* ApacheRewriteDriverFactory::DefaultLockManager() {
  if (config_->use_shared_mem_locking() &&
      (shared_mem_lock_manager_lifecycler_.Get() != NULL)) {
    return shared_mem_lock_manager_lifecycler_.Release();
  }
  return RewriteDriverFactory::DefaultLockManager();
}

UrlPollableAsyncFetcher* ApacheRewriteDriverFactory::SubResourceFetcher() {
  assert(FetchersComputed());
  return serf_url_async_fetcher_;  // may be null in a readonly slurping mode
}

UrlFetcher* ApacheRewriteDriverFactory::DefaultUrlFetcher() {
  if (serf_url_fetcher_ == NULL) {
    DefaultAsyncUrlFetcher();  // Create async fetcher if necessary.
    serf_url_fetcher_ = new SyncFetcherAdapter(
        timer(), config_->fetcher_time_out_ms(), serf_url_async_fetcher_,
        thread_system());
  }
  return serf_url_fetcher_;
}

UrlAsyncFetcher* ApacheRewriteDriverFactory::DefaultAsyncUrlFetcher() {
  if (serf_url_async_fetcher_ == NULL) {
    serf_url_async_fetcher_ = new SerfUrlAsyncFetcher(
        config_->fetcher_proxy().c_str(),
        NULL,  // Do not use the Factory pool so we can control deletion.
        thread_system(), statistics(), timer(),
        config_->fetcher_time_out_ms());
  }
  return serf_url_async_fetcher_;
}


void ApacheRewriteDriverFactory::SetStatistics(SharedMemStatistics* x) {
  if (x != shared_mem_statistics_) {
    DCHECK(!statistics_frozen_);
    statistics_frozen_ = true;
    shared_mem_statistics_ = x;
    RewriteDriverFactory::SetStatistics(x);
  }
}

void ApacheRewriteDriverFactory::SharedCircularBufferInit(bool is_root) {
  // Set buffer size to 0 means turning it off
  if (shared_mem_runtime() != NULL && config_->message_buffer_size() != 0) {
    shared_circular_buffer_.reset(new SharedCircularBuffer(
        shared_mem_runtime(),
        config_->message_buffer_size(),
        filename_prefix().as_string(),
        hostname_identifier()));
    shared_circular_buffer_->InitSegment(is_root, message_handler());
    apache_message_handler_->set_buffer(shared_circular_buffer_.get());
    apache_html_parse_message_handler_->set_buffer(
        shared_circular_buffer_.get());
  }
}

void ApacheRewriteDriverFactory::SharedMemRefererStatisticsInit(bool is_root) {
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
}

void ApacheRewriteDriverFactory::ParentOrChildInit() {
  SharedCircularBufferInit(is_root_process_);
  SharedMemRefererStatisticsInit(is_root_process_);
}

void ApacheRewriteDriverFactory::RootInit() {
  ParentOrChildInit();
  if (config_->use_shared_mem_locking()) {
    shared_mem_lock_manager_lifecycler_.RootInit();
  }
}

void ApacheRewriteDriverFactory::ChildInit() {
  is_root_process_ = false;
  ParentOrChildInit();
  // Reinitialize pid for child process.
  apache_message_handler_->SetPidString(static_cast<int64>(getpid()));
  apache_html_parse_message_handler_->SetPidString(
      static_cast<int64>(getpid()));
  if (!slow_worker_.Attach()) {
    slow_worker_.Initialize(new SlowWorker(thread_system()));
  }
  if (shared_mem_statistics_ != NULL) {
    shared_mem_statistics_->Init(false, message_handler());
  }
  if (config_->use_shared_mem_locking()) {
    shared_mem_lock_manager_lifecycler_.ChildInit();
  }
}

void ApacheRewriteDriverFactory::DumpRefererStatistics(Writer* writer) {
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
}

void ApacheRewriteDriverFactory::ShutDown() {
  StopCacheWrites();

  // Next, we shutdown the fetcher before killing the workers in
  // RewriteDriverFactory::ShutDown; this is so any rewrite jobs in progress
  // can quickly wrap up.
  if (serf_url_async_fetcher_ != NULL) {
    serf_url_async_fetcher_->ShutDown();
  }

  if (is_root_process_) {
    // Cleanup statistics.
    // TODO(morlovich): This looks dangerous with async.
    if (owns_statistics_ && (shared_mem_statistics_ != NULL)) {
      shared_mem_statistics_->GlobalCleanup(message_handler());
    }
    shared_mem_lock_manager_lifecycler_.GlobalCleanup(message_handler());
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

}  // namespace net_instaweb
