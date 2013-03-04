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

#include "ngx_rewrite_options.h"
#include "ngx_thread_system.h"
#include "ngx_server_context.h"

#include "net/instaweb/apache/serf_url_async_fetcher.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/fake_url_async_fetcher.h"
#include "net/instaweb/http/public/wget_url_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/system/public/apr_thread_compatible_pool.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/null_shared_mem.h"
#include "net/instaweb/util/public/pthread_shared_mem.h"
#include "net/instaweb/util/public/scheduler_thread.h"
#include "net/instaweb/util/public/simple_stats.h"
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

NgxRewriteDriverFactory::NgxRewriteDriverFactory(NgxRewriteOptions* main_conf)
    : RewriteDriverFactory(new NgxThreadSystem()),
      // TODO(oschaaf): mod_pagespeed ifdefs this:
      shared_mem_runtime_(new PthreadSharedMem()),
      main_conf_(main_conf),
      threads_started_(false),
      is_root_process_(true) {
  RewriteDriverFactory::InitStats(&simple_stats_);
  SerfUrlAsyncFetcher::InitStats(&simple_stats_);
  SystemCaches::InitStats(&simple_stats_);
  SetStatistics(&simple_stats_);
  timer_ = DefaultTimer();
  InitializeDefaultOptions();
  default_options()->set_beacon_url("/ngx_pagespeed_beacon");

  // TODO(oschaaf): determine a sensible limit
  int thread_limit = 2;
  caches_.reset(
      new SystemCaches(this, shared_mem_runtime_.get(), thread_limit));
}

NgxRewriteDriverFactory::~NgxRewriteDriverFactory() {
  delete timer_;
  timer_ = NULL;

  ShutDown();

  CHECK(uninitialized_server_contexts_.empty() || is_root_process_);
  STLDeleteElements(&uninitialized_server_contexts_);
}

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
  CHECK(false);
  return NULL;
}

void NgxRewriteDriverFactory::SetupCaches(ServerContext* server_context) {
  caches_->SetupCaches(server_context);

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

void NgxRewriteDriverFactory::InitStaticAssetManager(
    StaticAssetManager* static_asset_manager) {
  static_asset_manager->set_library_url_prefix(kStaticAssetPrefix);
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

void NgxRewriteDriverFactory::ShutDown() {
  StopCacheActivity();
  RewriteDriverFactory::ShutDown();
  caches_->ShutDown(message_handler());
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

void NgxRewriteDriverFactory::ParentOrChildInit() {
  // left in as a stub, we will need it later on
}

void NgxRewriteDriverFactory::RootInit() {
  ParentOrChildInit();

  // Let SystemCaches know about the various paths we have in configuration
  // first, as well as the memcached instances.
  for (NgxServerContextSet::iterator p = uninitialized_server_contexts_.begin(),
           e = uninitialized_server_contexts_.end(); p != e; ++p) {
    NgxServerContext* server_context = *p;
    caches_->RegisterConfig(server_context->config());
  }

  caches_->RootInit();
}

void NgxRewriteDriverFactory::ChildInit() {
  is_root_process_ = false;
  ParentOrChildInit();

  caches_->ChildInit();
  for (NgxServerContextSet::iterator p = uninitialized_server_contexts_.begin(),
           e = uninitialized_server_contexts_.end(); p != e; ++p) {
    NgxServerContext* server_context = *p;
    server_context->ChildInit();
  }

  uninitialized_server_contexts_.clear();
}

}  // namespace net_instaweb
