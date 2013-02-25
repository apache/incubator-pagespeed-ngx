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

#include <cstdio>

#include "ngx_cache.h"
#include "ngx_message_handler.h"
#include "ngx_rewrite_options.h"
#include "ngx_thread_system.h"
#include "ngx_server_context.h"

#include "net/instaweb/apache/apr_mem_cache.h"
#include "net/instaweb/apache/apr_thread_compatible_pool.h"
#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/wget_url_fetcher.h"
#include "net/instaweb/http/public/write_through_http_cache.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/async_cache.h"
#include "net/instaweb/util/public/cache_batcher.h"
#include "net/instaweb/util/public/cache_copy.h"
#include "net/instaweb/util/public/cache_stats.h"
#include "net/instaweb/util/public/fallback_cache.h"
#include "net/instaweb/util/public/file_cache.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/null_shared_mem.h"
#include "pthread_shared_mem.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/shared_mem_referer_statistics.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/scheduler_thread.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/threadsafe_cache.h"
#include "net/instaweb/util/public/write_through_cache.h"

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

namespace {

const size_t kRefererStatisticsNumberOfPages = 1024;
const size_t kRefererStatisticsAverageUrlLength = 64;
const char kShutdownCount[] = "child_shutdown_count";

}  // namespace

const char NgxRewriteDriverFactory::kMemcached[] = "memcached";

NgxRewriteDriverFactory::NgxRewriteDriverFactory()
    : RewriteDriverFactory(new NgxThreadSystem()),
      // TODO(oschaaf): mod_pagespeed ifdefs this:
      shared_mem_runtime_(new PthreadSharedMem()),
      cache_hasher_(20),
      main_conf_(NULL),
      threads_started_(false),
      use_per_vhost_statistics_(false),
      is_root_process_(true),
      ngx_message_handler_(new NgxMessageHandler(thread_system()->NewMutex())),
      ngx_html_parse_message_handler_(
          new NgxMessageHandler(thread_system()->NewMutex())),
      install_crash_handler_(false),
      message_buffer_size_(0),
      shared_circular_buffer_(NULL),
      statistics_frozen_(false) {
  timer_ = DefaultTimer();
  InitializeDefaultOptions();
  default_options()->set_beacon_url("/ngx_pagespeed_beacon");
  set_message_handler(ngx_message_handler_);
  set_html_parse_message_handler(ngx_html_parse_message_handler_);
}

NgxRewriteDriverFactory::~NgxRewriteDriverFactory() {
  delete timer_;
  timer_ = NULL;
  if (!is_root_process_ && slow_worker_ != NULL) {
    slow_worker_->ShutDown();
  }

  ShutDown();

  CHECK(uninitialized_server_contexts_.empty() || is_root_process_);
  STLDeleteElements(&uninitialized_server_contexts_);

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

  shared_mem_statistics_.reset(NULL);
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
  const char* fetcher_proxy = "";
  if (main_conf_ != NULL) {
    fetcher_proxy = main_conf_->fetcher_proxy().c_str();
  }

  net_instaweb::UrlAsyncFetcher* fetcher =
      new net_instaweb::SerfUrlAsyncFetcher(
          fetcher_proxy,
          NULL,
          thread_system(),
          statistics(),
          timer(),
          2500,
          message_handler());
  return fetcher;
}

MessageHandler* NgxRewriteDriverFactory::DefaultHtmlParseMessageHandler() {
  return ngx_html_parse_message_handler_;
}

MessageHandler* NgxRewriteDriverFactory::DefaultMessageHandler() {
  return ngx_message_handler_;
}

FileSystem* NgxRewriteDriverFactory::DefaultFileSystem() {
  return new StdioFileSystem(timer_);
}

Timer* NgxRewriteDriverFactory::DefaultTimer() {
  return new GoogleTimer;
}

NamedLockManager* NgxRewriteDriverFactory::DefaultLockManager() {
  CHECK(false);
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
    l2_cache = memcached;
    server_context->set_owned_cache(memcached);
    server_context->set_filesystem_metadata_cache(
        new CacheCopy(GetFilesystemMetadataCache(options)));
  }
  Statistics* stats = server_context->statistics();

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

RewriteOptions* NgxRewriteDriverFactory::NewRewriteOptions() {
  return new NgxRewriteOptions();
}

void NgxRewriteDriverFactory::PrintMemCacheStats(GoogleString* out) {
  for (int i = 0, n = memcache_servers_.size(); i < n; ++i) {
    AprMemCache* mem_cache = memcache_servers_[i];
    if (!mem_cache->GetStatus(out)) {
      StrAppend(out, "\nError getting memcached server status for ",
                mem_cache->server_spec());
    }
  }
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

void NgxRewriteDriverFactory::StopCacheActivity() {
  RewriteDriverFactory::StopCacheActivity();

  // Iterate through the map of CacheInterface* objects constructed for
  // the memcached.  Note that these are not typically AprMemCache* objects,
  // but instead are a hierarchy of CacheStats*, CacheBatcher*, AsyncCache*,
  // and AprMemCache*, all of which must be stopped.
  for (MemcachedMap::iterator p = memcached_map_.begin(),
           e = memcached_map_.end(); p != e; ++p) {
    CacheInterface* cache = p->second;
    cache->ShutDown();
  }
}

NgxServerContext* NgxRewriteDriverFactory::MakeNgxServerContext() {
  NgxServerContext* server_context = new NgxServerContext(this);
  uninitialized_server_contexts_.insert(server_context);
  return server_context;
}

void NgxRewriteDriverFactory::ShutDown() {
  if (!is_root_process_) {
    Variable* child_shutdown_count = statistics()->GetVariable(kShutdownCount);
    child_shutdown_count->Add(1);
  } else {
    message_handler()->Message(kInfo, "Shutting down ngx_pagespeed root");
  }
  RewriteDriverFactory::ShutDown();

  // Take down any memcached threads.
  // TODO(oschaaf): should be refactored with the Apache shutdown code
  memcached_pool_.reset(NULL);
  ngx_message_handler_->set_buffer(NULL);
  ngx_html_parse_message_handler_->set_buffer(NULL);

  // TODO(oschaaf): enable this once the shared memory cleanup code
  // supports our ordering of events during a configuration reload
  if (is_root_process_) {
    // Cleanup statistics.
    // TODO(morlovich): This looks dangerous with async.
    if (shared_mem_statistics_.get() != NULL) {
      shared_mem_statistics_->GlobalCleanup(message_handler());
    }
    if (shared_circular_buffer_ != NULL) {
      shared_circular_buffer_->GlobalCleanup(message_handler());
    }
    // TODO(oschaaf): Should the shared memory lock manager be cleaning up here
    // as well? In mod_pagespeed, it doesn't.
  }
}

void NgxRewriteDriverFactory::StartThreads() {
  if (threads_started_) {
    return;
  }
  static_cast<NgxThreadSystem*>(thread_system())->PermitThreadStarting();
  // TODO(jefftk): use a native nginx timer instead of running our own thread.
  // See issue #111.
  SchedulerThread* thread = new SchedulerThread(thread_system(), scheduler());
  bool ok = thread->Start();
  CHECK(ok) << "Unable to start scheduler thread";
  defer_cleanup(thread->MakeDeleter());
  threads_started_ = true;
}

void NgxRewriteDriverFactory::ParentOrChildInit(ngx_log_t* log) {
  if (install_crash_handler_) {
    NgxMessageHandler::InstallCrashHandler(log);
  }
  ngx_message_handler_->set_log(log);
  ngx_html_parse_message_handler_->set_log(log);
  SharedCircularBufferInit(is_root_process_);
}

// TODO(jmarantz): make this per-vhost.
void NgxRewriteDriverFactory::SharedCircularBufferInit(bool is_root) {
  // Set buffer size to 0 means turning it off
  if (shared_mem_runtime() != NULL && (message_buffer_size_ != 0)) {
    // TODO(jmarantz): it appears that filename_prefix() is not actually
    // established at the time of this construction, calling into question
    // whether we are naming our shared-memory segments correctly.
    shared_circular_buffer_.reset(new SharedCircularBuffer(
        shared_mem_runtime(),
        message_buffer_size_,
        filename_prefix().as_string(),
        "foo.com" /*hostname_identifier()*/));
    if (shared_circular_buffer_->InitSegment(is_root, message_handler())) {
      ngx_message_handler_->set_buffer(shared_circular_buffer_.get());
      ngx_html_parse_message_handler_->set_buffer(
          shared_circular_buffer_.get());
    }
  }
}

void NgxRewriteDriverFactory::RootInit(ngx_log_t* log) {
  ParentOrChildInit(log);

  for (NgxServerContextSet::iterator p = uninitialized_server_contexts_.begin(),
           e = uninitialized_server_contexts_.end(); p != e; ++p) {
    NgxServerContext* server_context = *p;

    // Determine the set of caches needed based on the unique
    // file_cache_path()s in the manager configurations.  We ignore
    // the GetCache return value because our goal is just to populate
    // the map which we'll iterate on below.
    GetCache(server_context->config());
  }

  for (PathCacheMap::iterator p = path_cache_map_.begin(),
           e = path_cache_map_.end(); p != e; ++p) {
    NgxCache* cache = p->second;
    cache->RootInit();
  }
}

void NgxRewriteDriverFactory::ChildInit(ngx_log_t* log) {
  is_root_process_ = false;

  ParentOrChildInit(log);
  slow_worker_.reset(new SlowWorker(thread_system()));
  if (shared_mem_statistics_.get() != NULL) {
    shared_mem_statistics_->Init(false, message_handler());
  }

  for (PathCacheMap::iterator p = path_cache_map_.begin(),
           e = path_cache_map_.end(); p != e; ++p) {
    NgxCache* cache = p->second;
    cache->ChildInit();
  }
  for (NgxServerContextSet::iterator p = uninitialized_server_contexts_.begin(),
           e = uninitialized_server_contexts_.end(); p != e; ++p) {
    NgxServerContext* server_context = *p;
    server_context->ChildInit();
  }

  uninitialized_server_contexts_.clear();

  for (int i = 0, n = memcache_servers_.size(); i < n; ++i) {
    AprMemCache* mem_cache = memcache_servers_[i];
    bool connected = mem_cache->Connect();
    if (!connected) {
      message_handler()->Message(kError, "Failed to attach memcached, abort");
      abort();
    }
  }
}

// Initializes global statistics object if needed, using factory to
// help with the settings if needed.
// Note: does not call set_statistics() on the factory.
Statistics* NgxRewriteDriverFactory::MakeGlobalSharedMemStatistics(
    bool logging, int64 logging_interval_ms,
    const GoogleString& logging_file_base) {
  if (shared_mem_statistics_.get() == NULL) {
    shared_mem_statistics_.reset(AllocateAndInitSharedMemStatistics(
        "global", logging, logging_interval_ms, logging_file_base));
  }
  DCHECK(!statistics_frozen_);
  statistics_frozen_ = true;
  SetStatistics(shared_mem_statistics_.get());
  return shared_mem_statistics_.get();
}

SharedMemStatistics* NgxRewriteDriverFactory::
AllocateAndInitSharedMemStatistics(
    const StringPiece& name, const bool logging,
    const int64 logging_interval_ms,
    const GoogleString& logging_file_base) {
  // Note that we create the statistics object in the parent process, and
  // it stays around in the kids but gets reinitialized for them
  // inside ChildInit(), called from pagespeed_child_init.
  SharedMemStatistics* stats = new SharedMemStatistics(
      logging_interval_ms, StrCat(logging_file_base, name), logging,
      StrCat(filename_prefix(), name), shared_mem_runtime(), message_handler(),
      file_system(), timer());
  InitStats(stats);
  stats->Init(true, message_handler());
  return stats;
}

void NgxRewriteDriverFactory::InitStats(Statistics* statistics) {
  // Init standard PSOL stats.
  RewriteDriverFactory::InitStats(statistics);

  // Init Ngx-specific stats.
  NgxServerContext::InitStats(statistics);
  AprMemCache::InitStats(statistics);
  SerfUrlAsyncFetcher::InitStats(statistics);

  CacheStats::InitStats(NgxCache::kFileCache, statistics);
  CacheStats::InitStats(NgxCache::kLruCache, statistics);
  CacheStats::InitStats(kMemcached, statistics);

  statistics->AddVariable(kShutdownCount);
}

}  // namespace net_instaweb
