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
#include "ngx_cache.h"

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

NgxRewriteDriverFactory::NgxRewriteDriverFactory() {
  RewriteDriverFactory::InitStats(&simple_stats_);
  SerfUrlAsyncFetcher::InitStats(&simple_stats_);
  SetStatistics(&simple_stats_);
  timer_ = DefaultTimer();
  apr_initialize();
  apr_pool_create(&pool_,NULL);

  InitializeDefaultOptions();
  //shared_mem_runtime_(new NullSharedMem());
}

NgxRewriteDriverFactory::~NgxRewriteDriverFactory() {
  delete timer_;
  slow_worker_->ShutDown();
}

const char NgxRewriteDriverFactory::kStaticJavaScriptPrefix[] =
    "/ngx_pagespeed_static/";

Hasher* NgxRewriteDriverFactory::NewHasher() {
  return new MD5Hasher;
}

UrlFetcher* NgxRewriteDriverFactory::DefaultUrlFetcher() {
  return new WgetUrlFetcher;
}

UrlAsyncFetcher* NgxRewriteDriverFactory::DefaultAsyncUrlFetcher() {
  net_instaweb::UrlAsyncFetcher* fetcher =
      new net_instaweb::SerfUrlAsyncFetcher(
          "",
          pool_,
          thread_system(),
          statistics(),
          timer(),
          2500,
          message_handler());
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
  (void)ngx_cache;
  LRUCache* lru_cache = new LRUCache(
      options->lru_cache_kb_per_process() * 1024);
  CacheInterface* cache = new ThreadsafeCache(
      lru_cache, thread_system()->NewMutex());

  FileCache::CachePolicy* policy = new FileCache::CachePolicy(
      timer(),
      hasher(),
      options->file_cache_clean_interval_ms(),
      options->file_cache_clean_size_kb() * 1024,
      options->file_cache_clean_inode_limit());

  FileCache* file_cache = new FileCache(options->file_cache_path(),
                                        file_system(), NULL,
                                        filename_encoder(), policy,
                                        message_handler());

  slow_worker_.reset(new SlowWorker(thread_system()));

  WriteThroughHTTPCache* write_through_http_cache = new WriteThroughHTTPCache(
      cache, file_cache, timer(), hasher(), statistics());
  write_through_http_cache->set_cache1_limit(options->lru_cache_byte_limit());
  server_context->set_http_cache(write_through_http_cache);

  WriteThroughCache* write_through_cache = new WriteThroughCache(
      cache, file_cache);
  write_through_cache->set_cache1_limit(options->lru_cache_byte_limit());
  server_context->set_metadata_cache(write_through_cache);
  server_context->MakePropertyCaches(file_cache);
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
  int thread_limit=1;
  //ap_mpm_query(AP_MPMQ_HARD_LIMIT_THREADS, &thread_limit);
  //thread_limit += num_rewrite_threads() + num_expensive_rewrite_threads();
  return new AprMemCache(spec, thread_limit, hasher(), statistics(),
                         timer(), message_handler());
}


}  // namespace net_instaweb
