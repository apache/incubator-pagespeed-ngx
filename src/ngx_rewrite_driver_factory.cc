/*
 * Copyright 2012 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)

#include "ngx_rewrite_driver_factory.h"
#include "ngx_rewrite_options.h"
#include "ngx_url_async_fetcher.h"

#include <cstdio>

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/wget_url_fetcher.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/file_cache.h"
#include "net/instaweb/util/public/write_through_cache.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/write_through_http_cache.h"
#include "net/instaweb/apache/apr_thread_compatible_pool.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/apache/apr_mem_cache.h"
#include "net/instaweb/util/public/null_shared_mem.h"
#include "net/instaweb/util/public/cache_copy.h"
#include "net/instaweb/util/public/async_cache.h"
#include "net/instaweb/util/public/cache_stats.h"
#include "net/instaweb/util/public/cache_batcher.h"
#include "net/instaweb/util/public/fallback_cache.h"
#include "ngx_cache.h"
#include "net/instaweb/apache/apr_thread_compatible_pool.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/apache/apr_mem_cache.h"

namespace net_instaweb {

class CacheInterface;
class FileSystem;
class Hasher;
class MessageHandler;
class Statistics;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class Writer;

const char NgxRewriteDriverFactory::kMemcached[] = "memcached";

NgxRewriteDriverFactory::NgxRewriteDriverFactory(ngx_log_t* log,
    ngx_msec_t resolver_timeout, ngx_resolver_t* resolver) :
  shared_mem_runtime_(new NullSharedMem()),
  cache_hasher_(20) {
  RewriteDriverFactory::InitStats(&simple_stats_);
  SerfUrlAsyncFetcher::InitStats(&simple_stats_);
  AprMemCache::InitStats(&simple_stats_);
  CacheStats::InitStats(NgxCache::kFileCache, &simple_stats_);
  CacheStats::InitStats(NgxCache::kLruCache, &simple_stats_);
  CacheStats::InitStats(kMemcached, &simple_stats_);
  SetStatistics(&simple_stats_);
  timer_ = DefaultTimer();
  apr_initialize();
  apr_pool_create(&pool_,NULL);
  log_ = log;
  resolver_timeout_ = resolver_timeout_;
  resolver_ = resolver;
  ngx_url_async_fetcher_ = NULL;
  InitializeDefaultOptions();
}

NgxRewriteDriverFactory::~NgxRewriteDriverFactory() {
  delete timer_;
  timer_ = NULL;
  slow_worker_->ShutDown();
  apr_pool_destroy(pool_);
  pool_ = NULL;

  for (PathCacheMap::iterator p = path_cache_map_.begin(),
           e = path_cache_map_.end(); p != e; ++p) {
    NgxCache* cache = p->second;
    defer_cleanup(new Deleter<NgxCache>(cache));
  }

  for (MemcachedMap::iterator p = memcached_map_.begin(),
           e = memcached_map_.end(); p != e; ++p) {
    CacheInterface* memcached = p->second;
    defer_cleanup(new Deleter<CacheInterface>(memcached));
  }
}

const char NgxRewriteDriverFactory::kStaticJavaScriptPrefix[] =
    "/ngx_pagespeed_static/";

Hasher* NgxRewriteDriverFactory::NewHasher() {
  return new MD5Hasher;
}

UrlFetcher* NgxRewriteDriverFactory::DefaultUrlFetcher() {
  return NULL;
}

UrlAsyncFetcher* NgxRewriteDriverFactory::DefaultAsyncUrlFetcher() {
  net_instaweb::NgxUrlAsyncFetcher* fetcher =
    new net_instaweb::NgxUrlAsyncFetcher(
        "",
        log_,
        resolver_timeout_,
        60000,
        resolver_,
        thread_system(),
        message_handler());
  ngx_url_async_fetcher_ = fetcher;
  return fetcher;
}

MessageHandler* NgxRewriteDriverFactory::DefaultHtmlParseMessageHandler() {
  return new GoogleMessageHandler;
}

MessageHandler* NgxRewriteDriverFactory::DefaultMessageHandler() {
  return DefaultHtmlParseMessageHandler();
}

FileSystem* NgxRewriteDriverFactory::DefaultFileSystem() {
  return new StdioFileSystem(timer_);
}

Timer* NgxRewriteDriverFactory::DefaultTimer() {
  return new GoogleTimer;
}

NamedLockManager* NgxRewriteDriverFactory::DefaultLockManager() {
  return NULL;
}

void NgxRewriteDriverFactory::SetupCaches(ServerContext* server_context) {
  // TODO(jefftk): see the ngx_rewrite_options.h note on OriginRewriteOptions;
  // this would move to OriginRewriteOptions.

  NgxRewriteOptions* options = NgxRewriteOptions::DynamicCast(
      server_context->global_options());

  NgxCache* ngx_cache = GetCache(options);
  CacheInterface* l1_cache = ngx_cache->l1_cache();
  CacheInterface* l2_cache = ngx_cache->l2_cache();
  CacheInterface* memcached = GetMemcached(options, l2_cache);
  if (memcached != NULL) {
    // XXX(oschaaf): remove when done
    l1_cache = NULL;
    l2_cache = memcached;
    server_context->set_owned_cache(memcached);
    server_context->set_filesystem_metadata_cache(
        new CacheCopy(GetFilesystemMetadataCache(options)));
  }
  Statistics* stats = server_context->statistics();

  // TODO(jmarantz): consider moving ownership of the L1 cache into the
  // factory, rather than having one per vhost.
  //
  // Note that a user can disable the L1 cache by setting its byte-count
  // to 0, in which case we don't build the write-through mechanisms.
  if (l1_cache == NULL) {
    HTTPCache* http_cache = new HTTPCache(l2_cache, timer(), hasher(), stats);
    server_context->set_http_cache(http_cache);
    server_context->set_metadata_cache(new CacheCopy(l2_cache));
    server_context->MakePropertyCaches(l2_cache);
  } else {
    WriteThroughHTTPCache* write_through_http_cache = new WriteThroughHTTPCache(
        l1_cache, l2_cache, timer(), hasher(), stats);
    write_through_http_cache->set_cache1_limit(options->lru_cache_byte_limit());
    server_context->set_http_cache(write_through_http_cache);

    WriteThroughCache* write_through_cache = new WriteThroughCache(
        l1_cache, l2_cache);
    write_through_cache->set_cache1_limit(options->lru_cache_byte_limit());
    server_context->set_metadata_cache(write_through_cache);
    server_context->MakePropertyCaches(l2_cache);
  }

  // TODO(oschaaf): see the property cache setup in the apache rewrite
  // driver factory
  server_context->set_enable_property_cache(true);
}

Statistics* NgxRewriteDriverFactory::statistics() {
  return &simple_stats_;
}

RewriteOptions* NgxRewriteDriverFactory::NewRewriteOptions() {
  return new NgxRewriteOptions();
}

void NgxRewriteDriverFactory::InitStaticJavascriptManager(
    StaticJavascriptManager* static_js_manager) {
  static_js_manager->set_library_url_prefix(kStaticJavaScriptPrefix);
}

NgxCache* NgxRewriteDriverFactory::GetCache(NgxRewriteOptions* options) {
  const GoogleString& path = options->file_cache_path();
  std::pair<PathCacheMap::iterator, bool> result = path_cache_map_.insert(
      PathCacheMap::value_type(path, static_cast<NgxCache*>(NULL)));
  PathCacheMap::iterator iter = result.first;
  if (result.second) {
    iter->second = new NgxCache(path, *options, this);
  }
  return iter->second;
}

AprMemCache* NgxRewriteDriverFactory::NewAprMemCache(
    const GoogleString& spec) {
  // TODO(oschaaf): determine a sensible limit
  int thread_limit = 2;
  return new AprMemCache(spec, thread_limit, &cache_hasher_, statistics(),
                         timer(), message_handler());
}

CacheInterface* NgxRewriteDriverFactory::GetMemcached(
    NgxRewriteOptions* options, CacheInterface* l2_cache) {
  CacheInterface* memcached = NULL;

  // Find a memcache that matches the current spec, or create a new one
  // if needed. Note that this means that two different VirtualHost's will
  // share a memcached if their specs are the same but will create their own
  // if the specs are different.
  if (!options->memcached_servers().empty()) {
    const GoogleString& server_spec = options->memcached_servers();
    std::pair<MemcachedMap::iterator, bool> result = memcached_map_.insert(
        MemcachedMap::value_type(server_spec, memcached));
    if (result.second) {
      fprintf(stderr,"setting up memcached server [%s]\n",server_spec.c_str());
      AprMemCache* mem_cache = NewAprMemCache(server_spec);

      memcache_servers_.push_back(mem_cache);

      int num_threads = options->memcached_threads();
      if (num_threads != 0) {
        if (memcached_pool_.get() == NULL) {
          // Note -- we will use the first value of ModPagespeedMemCacheThreads
          // that we see in a VirtualHost, ignoring later ones.
          memcached_pool_.reset(new QueuedWorkerPool(num_threads,
                                                     thread_system()));
        }
        AsyncCache* async_cache = new AsyncCache(mem_cache,
                                                 memcached_pool_.get());
        async_caches_.push_back(async_cache);
        memcached = async_cache;
      } else {
        message_handler()->Message(kWarning,
          "Running memcached synchronously, this may hurt performance");
        memcached = mem_cache;
      }

      // Put the batcher above the stats so that the stats sees the MultiGets
      // and can show us the histogram of how they are sized.
#if CACHE_STATISTICS
      memcached = new CacheStats(kMemcached, memcached, timer(), statistics());
#endif
      CacheBatcher* batcher = new CacheBatcher(
          memcached, thread_system()->NewMutex(), statistics());
      if (num_threads != 0) {
        batcher->set_max_parallel_lookups(num_threads);
      }
      memcached = batcher;
      result.first->second = memcached;

      // TODO(oschaaf): should not connect to memcached here
      bool connected = mem_cache->Connect();
      fprintf(stderr,
              "connected to memcached backend: %s\n", connected ? "true": "false");

    } else {
      memcached = result.first->second;
    }

    // Note that a distinct FallbackCache gets created for every VirtualHost
    // that employs memcached, even if the memcached and file-cache
    // specifications are identical.  This does no harm, because there
    // is no data in the cache object itself; just configuration.  Sharing
    // FallbackCache* objects would require making a map using the
    // memcache & file-cache specs as a key, so it's simpler to make a new
    // small FallbackCache object for each VirtualHost.
    memcached = new FallbackCache(memcached, l2_cache,
                                  AprMemCache::kValueSizeThreshold,
                                  message_handler());
  }
  return memcached;
}

CacheInterface* NgxRewriteDriverFactory::GetFilesystemMetadataCache(
    NgxRewriteOptions* options) {
  // Reuse the memcached server(s) for the filesystem metadata cache. We need
  // to search for our config's entry in the vector of servers (not the more
  // obvious map) because the map's entries are wrapped in an AsyncCache, and
  // the filesystem metadata cache requires a blocking cache (like memcached).
  // Note that if we have a server spec we *know* it's in the searched vector.
  // We perform a linear scan assuming that the searched set will be small
  DCHECK_EQ(options->memcached_servers().empty(), memcache_servers_.empty());
  const GoogleString& server_spec = options->memcached_servers();
  for (int i = 0, n = memcache_servers_.size(); i < n; ++i) {
    if (server_spec == memcache_servers_[i]->server_spec()) {
      return memcache_servers_[i];
    }
  }

  return NULL;
}

bool NgxRewriteDriverFactory::InitNgxUrlAsyncFecther() {
  if (ngx_url_async_fetcher_ == NULL) {
    return true;
  }
  return ngx_url_async_fetcher_->Init();
}

}  // namespace net_instaweb
