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

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/rate_controller.h"
#include "net/instaweb/http/public/rate_controlling_url_async_fetcher.h"
#include "net/instaweb/http/public/wget_url_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/system/public/in_place_resource_recorder.h"
#include "net/instaweb/system/public/serf_url_async_fetcher.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/system/public/system_rewrite_options.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/null_shared_mem.h"
#include "net/instaweb/util/public/posix_timer.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scheduler_thread.h"
#include "net/instaweb/util/public/shared_circular_buffer.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/stdio_file_system.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/thread/pthread_shared_mem.h"

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

class SharedCircularBuffer;

NgxRewriteDriverFactory::NgxRewriteDriverFactory(
    NgxThreadSystem* ngx_thread_system, StringPiece hostname, int port)
    : SystemRewriteDriverFactory(ngx_thread_system, hostname, port),
      ngx_thread_system_(ngx_thread_system),
      main_conf_(NULL),
      threads_started_(false),
      use_per_vhost_statistics_(false),
      ngx_message_handler_(new NgxMessageHandler(thread_system()->NewMutex())),
      ngx_html_parse_message_handler_(
          new NgxMessageHandler(thread_system()->NewMutex())),
      install_crash_handler_(false),
      ngx_url_async_fetcher_(NULL),
      log_(NULL),
      resolver_timeout_(NGX_CONF_UNSET_MSEC),
      use_native_fetcher_(false),
      ngx_shared_circular_buffer_(NULL) {
  InitializeDefaultOptions();
  default_options()->set_beacon_url("/ngx_pagespeed_beacon");
  SystemRewriteOptions* system_options = dynamic_cast<SystemRewriteOptions*>(
      default_options());
  system_options->set_file_cache_clean_inode_limit(500000);
  system_options->set_avoid_renaming_introspective_javascript(true);
  set_message_handler(ngx_message_handler_);
  set_html_parse_message_handler(ngx_html_parse_message_handler_);
}

NgxRewriteDriverFactory::~NgxRewriteDriverFactory() {
  ShutDown();
  ngx_shared_circular_buffer_ = NULL;
  STLDeleteElements(&uninitialized_server_contexts_);
}

Hasher* NgxRewriteDriverFactory::NewHasher() {
  return new MD5Hasher;
}

UrlAsyncFetcher* NgxRewriteDriverFactory::DefaultAsyncUrlFetcher() {
  const char* fetcher_proxy = "";
  if (main_conf_ != NULL) {
    fetcher_proxy = main_conf_->fetcher_proxy().c_str();
  }

  UrlAsyncFetcher* fetcher = NULL;
  SerfUrlAsyncFetcher* serf_fetcher = NULL;

  if (use_native_fetcher_) {
    ngx_url_async_fetcher_ = new NgxUrlAsyncFetcher(
            fetcher_proxy,
            log_,
            resolver_timeout_,
            25000,
            resolver_,
            thread_system(),
            message_handler());
    fetcher = ngx_url_async_fetcher_;
  } else {
    serf_fetcher = new SerfUrlAsyncFetcher(
            fetcher_proxy,
            NULL,
            thread_system(),
            statistics(),
            timer(),
            2500,
            message_handler());
    fetcher = serf_fetcher;
  }

  SystemRewriteOptions* system_options = dynamic_cast<SystemRewriteOptions*>(
      default_options());
  if (rate_limit_background_fetches_) {
    // Unfortunately, we need stats for load-shedding.
    if (system_options->statistics_enabled()) {
      // TODO(oschaaf): mps bases this multiplier on the configured
      // num_rewrite_threads_ which we don't have (yet).
      int multiplier = 4;
      fetcher = new RateControllingUrlAsyncFetcher(
          fetcher,
          500 * multiplier /* max queue size */,
          multiplier /* requests/host */,
          500 * multiplier /* queued per host */,
          thread_system(),
          statistics());
      if (serf_fetcher != NULL) {
        defer_cleanup(new Deleter<SerfUrlAsyncFetcher>(serf_fetcher));
      } else if (ngx_url_async_fetcher_ != NULL) {
        defer_cleanup(new Deleter<NgxUrlAsyncFetcher>(ngx_url_async_fetcher_));
      }
    } else {
      message_handler()->Message(
          kError, "Can't enable fetch rate-limiting without statistics");
    }
  }
  return fetcher;
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

RewriteOptions* NgxRewriteDriverFactory::NewRewriteOptions() {
  NgxRewriteOptions* options = new NgxRewriteOptions(thread_system());
  options->SetRewriteLevel(RewriteOptions::kCoreFilters);
  return options;
}


void NgxRewriteDriverFactory::InitStaticAssetManager(
    StaticAssetManager* static_asset_manager) {
  static_asset_manager->set_library_url_prefix(kStaticAssetPrefix);
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

NgxServerContext* NgxRewriteDriverFactory::MakeNgxServerContext() {
  NgxServerContext* server_context = new NgxServerContext(this);
  uninitialized_server_contexts_.insert(server_context);
  return server_context;
}

ServerContext* NgxRewriteDriverFactory::NewServerContext() {
  LOG(DFATAL) << "MakeNgxServerContext should be used instead";
  return NULL;
}

void NgxRewriteDriverFactory::ShutDownMessageHandlers() {
  ngx_message_handler_->set_buffer(NULL);
  ngx_html_parse_message_handler_->set_buffer(NULL);
  for (NgxMessageHandlerSet::iterator p =
           server_context_message_handlers_.begin();
       p != server_context_message_handlers_.end(); ++p) {
    (*p)->set_buffer(NULL);
  }
  server_context_message_handlers_.clear();
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

void NgxRewriteDriverFactory::LoggingInit(ngx_log_t* log) {
  if (install_crash_handler_) {
    NgxMessageHandler::InstallCrashHandler(log);
  }
  ngx_message_handler_->set_log(log);
  ngx_html_parse_message_handler_->set_log(log);
}

void NgxRewriteDriverFactory::SetCircularBuffer(
    SharedCircularBuffer* buffer) {
  ngx_shared_circular_buffer_ = buffer;
}

void NgxRewriteDriverFactory::SetServerContextMessageHandler(
    ServerContext* server_context, ngx_log_t* log) {
  NgxMessageHandler* handler = new NgxMessageHandler(
      thread_system()->NewMutex());
  handler->set_log(log);
  // The ngx_shared_circular_buffer_ will be NULL if MessageBufferSize hasn't
  // been raised from its default of 0.
  handler->set_buffer(ngx_shared_circular_buffer_);
  server_context_message_handlers_.insert(handler);
  defer_cleanup(new Deleter<NgxMessageHandler>(handler));
  server_context->set_message_handler(handler);
}

void NgxRewriteDriverFactory::InitStats(Statistics* statistics) {
  // Init standard PSOL stats.
  SystemRewriteDriverFactory::InitStats(statistics);
  RewriteDriverFactory::InitStats(statistics);
  RateController::InitStats(statistics);

  // Init Ngx-specific stats.
  NgxServerContext::InitStats(statistics);
  InPlaceResourceRecorder::InitStats(statistics);
}

}  // namespace net_instaweb
