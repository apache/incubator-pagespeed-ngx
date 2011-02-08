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
#include "net/instaweb/apache/apr_statistics.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/http/public/sync_fetcher_adapter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/file_cache.h"
#include "net/instaweb/util/public/gflags.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/write_through_cache.h"

namespace net_instaweb {

ApacheRewriteDriverFactory::ApacheRewriteDriverFactory(
    server_rec* server, const StringPiece& version)
    : server_rec_(server),
      serf_url_fetcher_(NULL),
      serf_url_async_fetcher_(NULL),
      statistics_(NULL),
      lru_cache_kb_per_process_(0),
      lru_cache_byte_limit_(0),
      file_cache_clean_interval_ms_(Timer::kHourMs),
      file_cache_clean_size_kb_(100 * 1024),  // 100 megabytes
      fetcher_time_out_ms_(5 * Timer::kSecondMs),
      slurp_flush_limit_(0),
      version_(version.data(), version.size()),
      statistics_enabled_(true) {
  apr_pool_create(&pool_, NULL);
  cache_mutex_.reset(NewMutex());
  rewrite_drivers_mutex_.reset(NewMutex());

  // In Apache, we default to using the "core filters".
  options()->SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);
}

ApacheRewriteDriverFactory::~ApacheRewriteDriverFactory() {
  // We free all the resources before destroying the pool, because some of the
  // resource uses the sub-pool and will need that pool to be around to
  // clean up properly.
  ShutDown();

  apr_pool_destroy(pool_);
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

CacheInterface* ApacheRewriteDriverFactory::DefaultCacheInterface() {
  FileCache::CachePolicy* policy = new FileCache::CachePolicy(
      timer(), file_cache_clean_interval_ms_, file_cache_clean_size_kb_);
  CacheInterface* cache = new FileCache(
      file_cache_path_, file_system(), filename_encoder(), policy,
      message_handler());
  if (lru_cache_kb_per_process_ != 0) {
    LRUCache* lru_cache = new LRUCache(lru_cache_kb_per_process_ * 1024);

    // We only add the threadsafe-wrapper to the LRUCache.  The FileCache
    // is naturally thread-safe because it's got no writable member variables.
    // And surrounding that slower-running class with a mutex would likely
    // cause contention.
    ThreadsafeCache* ts_cache = new ThreadsafeCache(lru_cache, cache_mutex());
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
        fetcher_proxy_.c_str(), pool_, statistics_, timer(),
        fetcher_time_out_ms_);
  }
  return serf_url_async_fetcher_;
}


HtmlParse* ApacheRewriteDriverFactory::DefaultHtmlParse() {
  return new HtmlParse(html_parse_message_handler());
}

AbstractMutex* ApacheRewriteDriverFactory::NewMutex() {
  return new AprMutex(pool_);
}

ResourceManager* ApacheRewriteDriverFactory::ComputeResourceManager() {
  ResourceManager* resource_manager =
      RewriteDriverFactory::ComputeResourceManager();
  resource_manager->set_statistics(statistics_);
  http_cache()->SetStatistics(statistics_);
  return resource_manager;
}

void ApacheRewriteDriverFactory::ShutDown() {
  if (serf_url_async_fetcher_ != NULL) {
    serf_url_async_fetcher_->WaitForActiveFetches(
        fetcher_time_out_ms_, message_handler(),
        SerfUrlAsyncFetcher::kThreadedAndMainline);
  }
  cache_mutex_.reset(NULL);
  rewrite_drivers_mutex_.reset(NULL);
  RewriteDriverFactory::ShutDown();
}

void ApacheRewriteDriverFactory::Terminate() {
  google::ShutDownCommandLineFlags();
}

}  // namespace net_instaweb
