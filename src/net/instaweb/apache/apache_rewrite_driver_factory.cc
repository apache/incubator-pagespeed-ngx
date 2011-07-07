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

#include "net/instaweb/apache/apache_message_handler.h"
#include "net/instaweb/apache/apr_file_system.h"
#include "net/instaweb/apache/apr_mutex.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/http/public/sync_fetcher_adapter.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/file_cache.h"
#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/pthread_shared_mem.h"
#include "net/instaweb/util/public/pthread_thread_system.h"
#include "net/instaweb/util/public/shared_mem_lock_manager.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/write_through_cache.h"

namespace net_instaweb {

SharedMemOwnerMap* ApacheRewriteDriverFactory::lock_manager_owners_ = NULL;
RefCountedOwner<SlowWorker>::Family
    ApacheRewriteDriverFactory::slow_worker_family_;

ApacheRewriteDriverFactory::ApacheRewriteDriverFactory(
    server_rec* server, const StringPiece& version)
    : server_rec_(server),
      serf_url_fetcher_(NULL),
      serf_url_async_fetcher_(NULL),
      shared_mem_statistics_(NULL),
      shared_mem_runtime_(new PthreadSharedMem()),
      slow_worker_(&slow_worker_family_),
      lru_cache_kb_per_process_(0),
      lru_cache_byte_limit_(0),
      file_cache_clean_interval_ms_(Timer::kHourMs),
      file_cache_clean_size_kb_(100 * 1024),  // 100 megabytes
      fetcher_time_out_ms_(5 * Timer::kSecondMs),
      slurp_flush_limit_(0),
      version_(version.data(), version.size()),
      statistics_enabled_(true),
      statistics_frozen_(false),
      owns_statistics_(false),
      test_proxy_(false),
      is_root_process_(true),
      use_shared_mem_locking_(false),
      shared_mem_lock_manager_lifecycler_(
          this,
          &ApacheRewriteDriverFactory::CreateSharedMemLockManager,
          "lock manager",
          &lock_manager_owners_) {
  apr_pool_create(&pool_, NULL);

  // In Apache, we default to using the "core filters".
  options()->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
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
                                  StrCat(file_cache_path(), "/named_locks"),
                                  timer(), hasher(), message_handler());
}

FileSystem* ApacheRewriteDriverFactory::DefaultFileSystem() {
  return new AprFileSystem(pool_);
}

Hasher* ApacheRewriteDriverFactory::NewHasher() {
  return new MD5Hasher();
}

Timer* ApacheRewriteDriverFactory::DefaultTimer() {
  return new AprTimer();
}

MessageHandler* ApacheRewriteDriverFactory::DefaultHtmlParseMessageHandler() {
  return new ApacheMessageHandler(server_rec_, version_);
}

MessageHandler* ApacheRewriteDriverFactory::DefaultMessageHandler() {
  return new ApacheMessageHandler(server_rec_, version_);
}

bool ApacheRewriteDriverFactory::set_file_cache_path(const StringPiece& p) {
  p.CopyToString(&file_cache_path_);
  if (file_system()->IsDir(file_cache_path_.c_str(),
                           message_handler()).is_true()) {
    return true;
  }
  bool ok =
    file_system()->RecursivelyMakeDir(file_cache_path_, message_handler());
  if (ok) {
    AddCreatedDirectory(file_cache_path_);
  }
  return ok;
}

CacheInterface* ApacheRewriteDriverFactory::DefaultCacheInterface() {
  FileCache::CachePolicy* policy = new FileCache::CachePolicy(
      timer(), file_cache_clean_interval_ms_, file_cache_clean_size_kb_);
  CacheInterface* cache = new FileCache(
      file_cache_path_, file_system(), slow_worker_.Get(), filename_encoder(),
      policy, message_handler());
  if (lru_cache_kb_per_process_ != 0) {
    LRUCache* lru_cache = new LRUCache(lru_cache_kb_per_process_ * 1024);

    // We only add the threadsafe-wrapper to the LRUCache.  The FileCache
    // is naturally thread-safe because it's got no writable member variables.
    // And surrounding that slower-running class with a mutex would likely
    // cause contention.
    ThreadsafeCache* ts_cache = new ThreadsafeCache(lru_cache, NewMutex());
    WriteThroughCache* write_through_cache =
        new WriteThroughCache(ts_cache, cache);
    // By default, WriteThroughCache does not limit the size of entries going
    // into its front cache.
    if (lru_cache_byte_limit_ != 0) {
      write_through_cache->set_cache1_limit(lru_cache_byte_limit_);
    }
    cache = write_through_cache;
  }
  return cache;
}

NamedLockManager* ApacheRewriteDriverFactory::DefaultLockManager() {
  if (use_shared_mem_locking_ &&
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
        timer(), fetcher_time_out_ms_, serf_url_async_fetcher_);
  }
  return serf_url_fetcher_;
}

UrlAsyncFetcher* ApacheRewriteDriverFactory::DefaultAsyncUrlFetcher() {
  if (serf_url_async_fetcher_ == NULL) {
    serf_url_async_fetcher_ = new SerfUrlAsyncFetcher(
        fetcher_proxy_.c_str(), pool_, statistics(), timer(),
        fetcher_time_out_ms_);
  }
  return serf_url_async_fetcher_;
}


HtmlParse* ApacheRewriteDriverFactory::DefaultHtmlParse() {
  return new HtmlParse(html_parse_message_handler());
}

ThreadSystem* ApacheRewriteDriverFactory::DefaultThreadSystem() {
  // TODO(morlovich): We need an ApacheThreadSystem, but it will likely
  // not use the apr_ library for threads, which is difficult to use
  // because it uses apr_pools, which are not thread-safe.  Instead
  // we will subclass PthreadThreadSystem and add any additional signal
  // masking needed to enable clean shutdowns.
  return new PthreadThreadSystem;
}

AbstractMutex* ApacheRewriteDriverFactory::NewMutex() {
  return new AprMutex(pool_);
}

void ApacheRewriteDriverFactory::SetStatistics(SharedMemStatistics* x) {
  DCHECK(!statistics_frozen_);
  shared_mem_statistics_ = x;
}

Statistics* ApacheRewriteDriverFactory::statistics() {
  statistics_frozen_ = true;
  if (shared_mem_statistics_ == NULL) {
    return RewriteDriverFactory::statistics();  // null implementation
  }
  return shared_mem_statistics_;
}

void ApacheRewriteDriverFactory::RootInit() {
  if (use_shared_mem_locking_) {
    shared_mem_lock_manager_lifecycler_.RootInit();
  }
}

void ApacheRewriteDriverFactory::ChildInit() {
  is_root_process_ = false;
  if (!slow_worker_.Attach()) {
    slow_worker_.Initialize(new SlowWorker(thread_system()));
    if (!slow_worker_.Get()->Start()) {
      message_handler()->Message(
          kError, "Unable to start background work thread.");
    }
  }
  if (shared_mem_statistics_ != NULL) {
    shared_mem_statistics_->InitVariables(false, message_handler());
  }
  if (use_shared_mem_locking_) {
    shared_mem_lock_manager_lifecycler_.ChildInit();
  }
}

void ApacheRewriteDriverFactory::ShutDown() {
  if (serf_url_async_fetcher_ != NULL) {
    serf_url_async_fetcher_->WaitForActiveFetches(
        fetcher_time_out_ms_, message_handler(),
        SerfUrlAsyncFetcher::kThreadedAndMainline);
  }
  if (is_root_process_) {
    if (owns_statistics_ && (shared_mem_statistics_ != NULL)) {
      shared_mem_statistics_->GlobalCleanup(message_handler());
    }
    shared_mem_lock_manager_lifecycler_.GlobalCleanup(message_handler());
  }
  RewriteDriverFactory::ShutDown();
}

}  // namespace net_instaweb
