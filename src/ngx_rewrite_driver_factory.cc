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

#include "log_message_handler.h"
#include "ngx_message_handler.h"
#include "ngx_rewrite_options.h"
#include "ngx_server_context.h"
#include "ngx_thread_system.h"
#include "ngx_url_async_fetcher.h"
#include "pthread_shared_mem.h"

#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/wget_url_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/null_shared_mem.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scheduler_thread.h"
#include "net/instaweb/util/public/posix_timer.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class FileSystem;
class Hasher;
class MessageHandler;
class Statistics;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class Writer;

const char NgxRewriteDriverFactory::kStaticAssetPrefix[] =
    "/ngx_pagespeed_static/";

namespace {

const char kShutdownCount[] = "child_shutdown_count";

}  // namespace

NgxRewriteDriverFactory::NgxRewriteDriverFactory(
    NgxThreadSystem* ngx_thread_system)
    : SystemRewriteDriverFactory(ngx_thread_system),
      ngx_thread_system_(ngx_thread_system),
      // TODO(oschaaf): mod_pagespeed ifdefs this:
      shared_mem_runtime_(new ngx::PthreadSharedMem()),
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
      statistics_frozen_(false),
      ngx_url_async_fetcher_(NULL),
      log_(NULL),
      resolver_timeout_(NGX_CONF_UNSET_MSEC),
      use_native_fetcher_(false) {
  InitializeDefaultOptions();
  default_options()->set_beacon_url("/ngx_pagespeed_beacon");
  SystemRewriteOptions* system_options = dynamic_cast<SystemRewriteOptions*>(
      default_options());
  system_options->set_file_cache_clean_inode_limit(500000);
  system_options->set_avoid_renaming_introspective_javascript(true);
  set_message_handler(ngx_message_handler_);
  set_html_parse_message_handler(ngx_html_parse_message_handler_);

  // see https://code.google.com/p/modpagespeed/issues/detail?id=672
  int thread_limit = 1;
  caches_.reset(
      new SystemCaches(this, shared_mem_runtime_.get(), thread_limit));
}

NgxRewriteDriverFactory::~NgxRewriteDriverFactory() {
  ShutDown();

  CHECK(uninitialized_server_contexts_.empty() || is_root_process_);
  STLDeleteElements(&uninitialized_server_contexts_);
  shared_mem_statistics_.reset(NULL);
}

Hasher* NgxRewriteDriverFactory::NewHasher() {
  return new MD5Hasher;
}

UrlFetcher* NgxRewriteDriverFactory::DefaultUrlFetcher() {
  CHECK(false) << "Nothing should still be using DefaultUrlFetcher()";
  return NULL;
}

UrlAsyncFetcher* NgxRewriteDriverFactory::DefaultAsyncUrlFetcher() {
  const char* fetcher_proxy = "";
  if (main_conf_ != NULL) {
    fetcher_proxy = main_conf_->fetcher_proxy().c_str();
  }

  if (use_native_fetcher_) {
    net_instaweb::NgxUrlAsyncFetcher* fetcher =
        new net_instaweb::NgxUrlAsyncFetcher(
            fetcher_proxy,
            log_,
            resolver_timeout_,
            25000,
            resolver_,
            thread_system(),
            message_handler());
    ngx_url_async_fetcher_ = fetcher;
    return fetcher;
  } else {
    net_instaweb::SerfUrlAsyncFetcher* fetcher =
        new net_instaweb::SerfUrlAsyncFetcher(
            fetcher_proxy,
            NULL,
            thread_system(),
            statistics(),
            timer(),
            2500,
            message_handler());
    // Make sure we don't block the nginx event loop
    fetcher->set_force_threaded(true);
    return fetcher;
  }
}

MessageHandler* NgxRewriteDriverFactory::DefaultHtmlParseMessageHandler() {
  return ngx_html_parse_message_handler_;
}

MessageHandler* NgxRewriteDriverFactory::DefaultMessageHandler() {
  return ngx_message_handler_;
}

FileSystem* NgxRewriteDriverFactory::DefaultFileSystem() {
  return new StdioFileSystem();
}

Timer* NgxRewriteDriverFactory::DefaultTimer() {
  return new PosixTimer;
}

NamedLockManager* NgxRewriteDriverFactory::DefaultLockManager() {
  CHECK(false);
  return NULL;
}

void NgxRewriteDriverFactory::SetupCaches(ServerContext* server_context) {
  caches_->SetupCaches(server_context);

  server_context->set_enable_property_cache(true);
  PropertyCache* pcache = server_context->page_property_cache();
  if (pcache->GetCohort(RewriteDriver::kBeaconCohort) == NULL) {
    pcache->AddCohort(RewriteDriver::kBeaconCohort);
  }
  if (pcache->GetCohort(RewriteDriver::kDomCohort) == NULL) {
    pcache->AddCohort(RewriteDriver::kDomCohort);
  }
}

RewriteOptions* NgxRewriteDriverFactory::NewRewriteOptions() {
  NgxRewriteOptions* options = new NgxRewriteOptions();
  options->SetRewriteLevel(RewriteOptions::kCoreFilters);
  return options;
}


void NgxRewriteDriverFactory::InitStaticAssetManager(
    StaticAssetManager* static_asset_manager) {
  static_asset_manager->set_library_url_prefix(kStaticAssetPrefix);
}

void NgxRewriteDriverFactory::PrintMemCacheStats(GoogleString* out) {
  // TODO(morlovich): Port the client code to proper API, so it gets
  // shm stats, too.
  caches_->PrintCacheStats(SystemCaches::kIncludeMemcached, out);
}

bool NgxRewriteDriverFactory::InitNgxUrlAsyncFetcher() {
  if (ngx_url_async_fetcher_ == NULL) {
    return true;
  }
  log_ = ngx_cycle->log;
  return ngx_url_async_fetcher_->Init();
}

bool NgxRewriteDriverFactory::CheckResolver() {
  if (use_native_fetcher_ && resolver_ == NULL) {
    return false;
  }
  return true;
}

void NgxRewriteDriverFactory::StopCacheActivity() {
  RewriteDriverFactory::StopCacheActivity();
  caches_->StopCacheActivity();
}

NgxServerContext* NgxRewriteDriverFactory::MakeNgxServerContext() {
  NgxServerContext* server_context = new NgxServerContext(this);
  uninitialized_server_contexts_.insert(server_context);
  return server_context;
}

ServerContext* NgxRewriteDriverFactory::NewServerContext() {
  LOG(DFATAL) << "MakeNgxServerContext should be used instead";
  return NULL;
}

void NgxRewriteDriverFactory::ShutDown() {
  StopCacheActivity();
  if (!is_root_process_) {
    Variable* child_shutdown_count = statistics()->GetVariable(kShutdownCount);
    child_shutdown_count->Add(1);
  }

  RewriteDriverFactory::ShutDown();
  caches_->ShutDown(message_handler());

  ngx_message_handler_->set_buffer(NULL);
  ngx_html_parse_message_handler_->set_buffer(NULL);

  if (is_root_process_) {
    // Cleanup statistics.
    // TODO(morlovich): This looks dangerous with async.
    if (shared_mem_statistics_.get() != NULL) {
      shared_mem_statistics_->GlobalCleanup(message_handler());
    }
    if (shared_circular_buffer_ != NULL) {
      shared_circular_buffer_->GlobalCleanup(message_handler());
    }
  }
}

void NgxRewriteDriverFactory::StartThreads() {
  if (threads_started_) {
    return;
  }
  ngx_thread_system_->PermitThreadStarting();
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
  net_instaweb::log_message_handler::Install(log);

  ParentOrChildInit(log);

  // Let SystemCaches know about the various paths we have in configuration
  // first, as well as the memcached instances.
  for (NgxServerContextSet::iterator p = uninitialized_server_contexts_.begin(),
           e = uninitialized_server_contexts_.end(); p != e; ++p) {
    NgxServerContext* server_context = *p;
    caches_->RegisterConfig(server_context->config());
  }

  caches_->RootInit();
}

void NgxRewriteDriverFactory::ChildInit(ngx_log_t* log) {
  is_root_process_ = false;

  ParentOrChildInit(log);
  if (shared_mem_statistics_.get() != NULL) {
    shared_mem_statistics_->Init(false, message_handler());
  }

  caches_->ChildInit();
  for (NgxServerContextSet::iterator p = uninitialized_server_contexts_.begin(),
           e = uninitialized_server_contexts_.end(); p != e; ++p) {
    NgxServerContext* server_context = *p;
    server_context->ChildInit();
  }

  uninitialized_server_contexts_.clear();
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
  SystemCaches::InitStats(statistics);
  SerfUrlAsyncFetcher::InitStats(statistics);
  PropertyCache::InitCohortStats(RewriteDriver::kBeaconCohort, statistics);
  PropertyCache::InitCohortStats(RewriteDriver::kDomCohort, statistics);

  statistics->AddVariable(kShutdownCount);
}

}  // namespace net_instaweb
