/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */



#include "ngx_rewrite_driver_factory.h"

#include <cstdio>

#include "log_message_handler.h"
#include "ngx_message_handler.h"
#include "ngx_rewrite_options.h"
#include "ngx_server_context.h"
#include "ngx_url_async_fetcher.h"

#include "net/instaweb/http/public/rate_controller.h"
#include "net/instaweb/http/public/rate_controlling_url_async_fetcher.h"
#include "net/instaweb/http/public/wget_url_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/null_shared_mem.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/sharedmem/shared_circular_buffer.h"
#include "pagespeed/kernel/sharedmem/shared_mem_statistics.h"
#include "pagespeed/kernel/thread/pthread_shared_mem.h"
#include "pagespeed/kernel/thread/scheduler_thread.h"
#include "pagespeed/kernel/thread/slow_worker.h"
#include "pagespeed/system/in_place_resource_recorder.h"
#include "pagespeed/system/serf_url_async_fetcher.h"
#include "pagespeed/system/system_caches.h"
#include "pagespeed/system/system_rewrite_options.h"

namespace net_instaweb {

class FileSystem;
class Hasher;
class MessageHandler;
class Statistics;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class Writer;

class SharedCircularBuffer;

NgxRewriteDriverFactory::NgxRewriteDriverFactory(
    const ProcessContext& process_context,
    SystemThreadSystem* system_thread_system, StringPiece hostname, int port)
    : SystemRewriteDriverFactory(process_context, system_thread_system,
        NULL /* default shared memory runtime */, hostname, port),
      threads_started_(false),
      ngx_message_handler_(
          new NgxMessageHandler(timer(), thread_system()->NewMutex())),
      ngx_html_parse_message_handler_(
          new NgxMessageHandler(timer(), thread_system()->NewMutex())),
      log_(NULL),
      resolver_timeout_(NGX_CONF_UNSET_MSEC),
      use_native_fetcher_(false),
      // 100 Aligns to nginx's server-side default.
      native_fetcher_max_keepalive_requests_(100),
      ngx_shared_circular_buffer_(NULL),
      hostname_(hostname.as_string()),
      port_(port),
      process_script_variables_mode_(ProcessScriptVariablesMode::kOff),
      process_script_variables_set_(false),
      shut_down_(false) {
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

UrlAsyncFetcher* NgxRewriteDriverFactory::AllocateFetcher(
    SystemRewriteOptions* config) {
  if (use_native_fetcher_) {
    NgxUrlAsyncFetcher* fetcher = new NgxUrlAsyncFetcher(
        config->fetcher_proxy().c_str(),
        log_,
        resolver_timeout_,
        config->blocking_fetch_timeout_ms(),
        resolver_,
        native_fetcher_max_keepalive_requests_,
        thread_system(),
        message_handler());
    ngx_url_async_fetchers_.push_back(fetcher);
    return fetcher;
  } else {
    return SystemRewriteDriverFactory::AllocateFetcher(config);
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

RewriteOptions* NgxRewriteDriverFactory::NewRewriteOptions() {
  NgxRewriteOptions* options = new NgxRewriteOptions(thread_system());
  // TODO(jefftk): figure out why using SetDefaultRewriteLevel like
  // mod_pagespeed does in mod_instaweb.cc:create_dir_config() isn't enough here
  // -- if you use that instead then ngx_pagespeed doesn't actually end up
  // defaulting CoreFilters.
  // See: https://github.com/apache/incubator-pagespeed-ngx/issues/1190
  options->SetRewriteLevel(RewriteOptions::kCoreFilters);
  return options;
}

RewriteOptions* NgxRewriteDriverFactory::NewRewriteOptionsForQuery() {
  return new NgxRewriteOptions(thread_system());
}

bool NgxRewriteDriverFactory::CheckResolver() {
  if (use_native_fetcher_ && resolver_ == NULL) {
    return false;
  }
  return true;
}

NgxServerContext* NgxRewriteDriverFactory::MakeNgxServerContext(
    StringPiece hostname, int port) {
  NgxServerContext* server_context = new NgxServerContext(this, hostname, port);
  uninitialized_server_contexts_.insert(server_context);
  return server_context;
}

ServerContext* NgxRewriteDriverFactory::NewDecodingServerContext() {
  ServerContext* sc = new NgxServerContext(this, hostname_, port_);
  InitStubDecodingServerContext(sc);
  return sc;
}

ServerContext* NgxRewriteDriverFactory::NewServerContext() {
  LOG(DFATAL) << "MakeNgxServerContext should be used instead";
  return NULL;
}

void NgxRewriteDriverFactory::ShutDown() {
  if (!shut_down_) {
    shut_down_ = true;
    SystemRewriteDriverFactory::ShutDown();
  }
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
  // TODO(jefftk): use a native nginx timer instead of running our own thread.
  // See issue #111.
  SchedulerThread* thread = new SchedulerThread(thread_system(), scheduler());
  bool ok = thread->Start();
  CHECK(ok) << "Unable to start scheduler thread";
  defer_cleanup(thread->MakeDeleter());
  threads_started_ = true;
}

void NgxRewriteDriverFactory::SetMainConf(NgxRewriteOptions* main_options) {
  // Propagate process-scope options from the copy we had during nginx option
  // parsing to our own.
  if (main_options != NULL) {
    default_options()->MergeOnlyProcessScopeOptions(*main_options);
  }
}

void NgxRewriteDriverFactory::LoggingInit(
    ngx_log_t* log, bool may_install_crash_handler) {
  log_ = log;
  net_instaweb::log_message_handler::Install(log);
  if (may_install_crash_handler && install_crash_handler()) {
    NgxMessageHandler::InstallCrashHandler(log);
  }
  ngx_message_handler_->set_log(log);
  ngx_html_parse_message_handler_->set_log(log);
}

void NgxRewriteDriverFactory::SetCircularBuffer(
    SharedCircularBuffer* buffer) {
  ngx_shared_circular_buffer_ = buffer;
  ngx_message_handler_->set_buffer(buffer);
  ngx_html_parse_message_handler_->set_buffer(buffer);
}

void NgxRewriteDriverFactory::SetServerContextMessageHandler(
    ServerContext* server_context, ngx_log_t* log) {
  NgxMessageHandler* handler = new NgxMessageHandler(
      timer(), thread_system()->NewMutex());
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

void NgxRewriteDriverFactory::PrepareForkedProcess(const char* name) {
  ngx_pid = ngx_getpid();  // Needed for logging to have the right PIDs.
  SystemRewriteDriverFactory::PrepareForkedProcess(name);
}

void NgxRewriteDriverFactory::NameProcess(const char* name) {
  SystemRewriteDriverFactory::NameProcess(name);

  // Superclass set status with prctl.  Nginx has a helper function for setting
  // argv[0] as well, so let's use that.  We'll show up as:
  //
  //    nginx: pagespeed $name

  char name_for_setproctitle[32];
  snprintf(name_for_setproctitle, sizeof(name_for_setproctitle),
           "pagespeed %s", name);
  ngx_setproctitle(name_for_setproctitle);
}

}  // namespace net_instaweb
