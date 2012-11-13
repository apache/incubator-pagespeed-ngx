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
#include "net/instaweb/util/public/file_system_lock_manager.h"
#include "net/instaweb/util/public/write_through_cache.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/write_through_http_cache.h"

//XXX: discuss the proper way to to this:
#include "net/instaweb/apache/apr_thread_compatible_pool.cc"
#include "net/instaweb/apache/serf_url_async_fetcher.cc"

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
//class SerfUrlAsyncFetcher;

NgxRewriteDriverFactory::NgxRewriteDriverFactory() {
  RewriteDriverFactory::InitStats(&simple_stats_);
  SerfUrlAsyncFetcher::InitStats(&simple_stats_); 
  SetStatistics(&simple_stats_);
  timer_ = DefaultTimer();
  apr_initialize();
  apr_pool_create(&pool_,NULL);
}

NgxRewriteDriverFactory::~NgxRewriteDriverFactory() {
  delete timer_;
  slow_worker_->ShutDown();
}

Hasher* NgxRewriteDriverFactory::NewHasher() {
  return new MD5Hasher;
}

UrlFetcher* NgxRewriteDriverFactory::DefaultUrlFetcher() {
  return new WgetUrlFetcher;
}

UrlAsyncFetcher* NgxRewriteDriverFactory::DefaultAsyncUrlFetcher() {
  net_instaweb::UrlAsyncFetcher* fetcher = new net_instaweb::SerfUrlAsyncFetcher(
      // "127.0.0.1:80",
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
  return new FileSystemLockManager(
      file_system(), filename_prefix().as_string(),
      scheduler(), message_handler());
}

void NgxRewriteDriverFactory::SetupCaches(ServerContext* server_context) {
  // TODO(jefftk): make LRUCache size configurable.
  LRUCache* lru_cache = new LRUCache(10 * 1000 * 1000);
  CacheInterface* cache = new ThreadsafeCache(
      lru_cache, thread_system()->NewMutex());

  // TODO(oschaaf): more configuration
  FileCache::CachePolicy* policy = new FileCache::CachePolicy(
      timer(),
      hasher(),
      6000, //clean interval ms
      10 * 1024 * 1014, //clean size kb
      10000); //clean inode limit

  FileCache* file_cache = new FileCache(filename_prefix().as_string(),
                                        file_system(), NULL,
                                        filename_encoder(), policy,
                                        message_handler());

  slow_worker_.reset(new SlowWorker(thread_system()));

  WriteThroughHTTPCache* write_through_http_cache = new WriteThroughHTTPCache(
      cache, file_cache, timer(), hasher(), statistics());
  write_through_http_cache->set_cache1_limit(1024*128);
  server_context->set_http_cache(write_through_http_cache);

  WriteThroughCache* write_through_cache = new WriteThroughCache(
      cache, file_cache);
  write_through_cache->set_cache1_limit(1024*128);
  server_context->set_metadata_cache(write_through_cache);
  server_context->MakePropertyCaches(file_cache);
  server_context->set_enable_property_cache(true);
}

Statistics* NgxRewriteDriverFactory::statistics() {
  return &simple_stats_;
}

}  // namespace net_instaweb
