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
//
// Author: jmarantz@google.com (Joshua Marantz)
//         lsong@google.com (Libo Song)

#include "net/instaweb/apache/apache_rewrite_driver_factory.h"

#include <unistd.h>

#include <algorithm>

#include "apr_pools.h"
#include "httpd.h"
#include "ap_mpm.h"

#include "base/logging.h"
#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/apache/apache_message_handler.h"
#include "net/instaweb/apache/apache_server_context.h"
#include "net/instaweb/apache/apache_thread_system.h"
#include "net/instaweb/apache/apr_timer.h"
#include "net/instaweb/apache/mod_spdy_fetch_controller.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/null_shared_mem.h"
#include "net/instaweb/util/public/pthread_shared_mem.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/slow_worker.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class SharedCircularBuffer;

const char ApacheRewriteDriverFactory::kStaticAssetPrefix[] =
    "/mod_pagespeed_static/";

ApacheRewriteDriverFactory::ApacheRewriteDriverFactory(
    server_rec* server, const StringPiece& version)
    : SystemRewriteDriverFactory(
          new ApacheThreadSystem,
          NULL, /* default shared memory runtime */
          server->server_hostname,
          server->port),
      server_rec_(server),
      version_(version.data(), version.size()),
      apache_message_handler_(new ApacheMessageHandler(
          server_rec_, version_, timer(), thread_system()->NewMutex())),
      apache_html_parse_message_handler_(new ApacheMessageHandler(
          server_rec_, version_, timer(), thread_system()->NewMutex())),
      use_per_vhost_statistics_(false),
      enable_property_cache_(true),
      inherit_vhost_config_(false),
      install_crash_handler_(false),
      thread_counts_finalized_(false),
      num_rewrite_threads_(-1),
      num_expensive_rewrite_threads_(-1) {
  apr_pool_create(&pool_, NULL);

  // Make sure the ownership of apache_message_handler_ and
  // apache_html_parse_message_handler_ is given to scoped pointer.
  // Otherwise may result in leak error in test.
  message_handler();
  html_parse_message_handler();
  InitializeDefaultOptions();

  // Note: this must run after mod_pagespeed_register_hooks has completed.
  // See http://httpd.apache.org/docs/2.4/developer/new_api_2_4.html and
  // search for ap_mpm_query.
  AutoDetectThreadCounts();

  int thread_limit = 0;
  ap_mpm_query(AP_MPMQ_HARD_LIMIT_THREADS, &thread_limit);
  thread_limit += num_rewrite_threads() + num_expensive_rewrite_threads();
  caches()->set_thread_limit(thread_limit);
}

ApacheRewriteDriverFactory::~ApacheRewriteDriverFactory() {
  // We free all the resources before destroying the pool, because some of the
  // resource uses the sub-pool and will need that pool to be around to
  // clean up properly.
  ShutDown();

  apr_pool_destroy(pool_);

  // We still have registered a pool deleter here, right?  This seems risky...
  STLDeleteElements(&uninitialized_server_contexts_);
}

Timer* ApacheRewriteDriverFactory::DefaultTimer() {
  return new AprTimer();
}

MessageHandler* ApacheRewriteDriverFactory::DefaultHtmlParseMessageHandler() {
  return apache_html_parse_message_handler_;
}

MessageHandler* ApacheRewriteDriverFactory::DefaultMessageHandler() {
  return apache_message_handler_;
}

void ApacheRewriteDriverFactory::SetupCaches(ServerContext* server_context) {
  SystemRewriteDriverFactory::SetupCaches(server_context);

  // TODO(jmarantz): It would make more sense to have the base ServerContext
  // own the ProxyFetchFactory, but that would create a cyclic directory
  // dependency.  This can be resolved minimally by moving proxy_fetch.cc
  // from automatic/ to rewriter/.  I think we should also think harder about
  // separating out rewriting infrastructure from rewriters.
  ApacheServerContext* apache_server_context =
      dynamic_cast<ApacheServerContext*>(server_context);
  CHECK(apache_server_context != NULL);
  apache_server_context->InitProxyFetchFactory();
}

void ApacheRewriteDriverFactory::InitStaticAssetManager(
    StaticAssetManager* static_asset_manager) {
  static_asset_manager->set_library_url_prefix(kStaticAssetPrefix);
}

QueuedWorkerPool* ApacheRewriteDriverFactory::CreateWorkerPool(
    WorkerPoolCategory pool, StringPiece name) {
  switch (pool) {
    case kHtmlWorkers:
      // In practice this is 0, as we don't use HTML threads in Apache.
      return new QueuedWorkerPool(1, name, thread_system());
    case kRewriteWorkers:
      return new QueuedWorkerPool(num_rewrite_threads_, name, thread_system());
    case kLowPriorityRewriteWorkers:
      return new QueuedWorkerPool(num_expensive_rewrite_threads_,
                                  name,
                                  thread_system());
    default:
      return RewriteDriverFactory::CreateWorkerPool(pool, name);
  }
}

void ApacheRewriteDriverFactory::AutoDetectThreadCounts() {
  if (thread_counts_finalized_) {
    return;
  }

  // Detect whether we're using a threaded MPM.
  apr_status_t status;
  int result = 0, threads = 1;
  status = ap_mpm_query(AP_MPMQ_IS_THREADED, &result);
  if (status == APR_SUCCESS &&
      (result == AP_MPMQ_STATIC || result == AP_MPMQ_DYNAMIC)) {
    status = ap_mpm_query(AP_MPMQ_MAX_THREADS, &threads);
    if (status != APR_SUCCESS) {
      threads = 0;
    }
  }

  threads = std::max(1, threads);

  if (threads > 1) {
    // Apply defaults for threaded.
    max_mod_spdy_fetch_threads_ = 8;  // TODO(morlovich): Base on MPM's count?
    if (num_rewrite_threads_ <= 0) {
      num_rewrite_threads_ = 4;
    }
    if (num_expensive_rewrite_threads_ <= 0) {
      num_expensive_rewrite_threads_ = 4;
    }
    message_handler()->Message(
        kInfo, "Detected threaded MPM with up to %d threads."
        " Own threads: %d Rewrite, %d Expensive Rewrite.",
        threads, num_rewrite_threads_, num_expensive_rewrite_threads_);

  } else {
    // Apply defaults for non-threaded.

    // If using mod_spdy_fetcher we roughly want one thread for non-background,
    // fetches one for background ones.
    max_mod_spdy_fetch_threads_ = 2;
    if (num_rewrite_threads_ <= 0) {
      num_rewrite_threads_ = 1;
    }
    if (num_expensive_rewrite_threads_ <= 0) {
      num_expensive_rewrite_threads_ = 1;
    }
    message_handler()->Message(
        kInfo, "No threading detected in MPM."
        " Own threads: %d Rewrite, %d Expensive Rewrite.",
        num_rewrite_threads_, num_expensive_rewrite_threads_);
  }

  thread_counts_finalized_ = true;
}

void ApacheRewriteDriverFactory::ParentOrChildInit() {
  if (install_crash_handler_) {
    ApacheMessageHandler::InstallCrashHandler(server_rec_);
  }
  SystemRewriteDriverFactory::ParentOrChildInit();
}

void ApacheRewriteDriverFactory::ChildInit() {
  SystemRewriteDriverFactory::ChildInit();
  mod_spdy_fetch_controller_.reset(
      new ModSpdyFetchController(max_mod_spdy_fetch_threads_, thread_system(),
                                 timer(), statistics()));
}


void ApacheRewriteDriverFactory::ShutDownMessageHandlers() {
  // Reset SharedCircularBuffer to NULL, so that any shutdown warnings
  // (e.g. in ServerContext::ShutDownDrivers) don't reference
  // deleted objects as the base-class is deleted.
  //
  // TODO(jefftk): merge ApacheMessageHandler and NgxMessageHandler into
  // SystemMessageHandler and then move this into System.
  apache_message_handler_->set_buffer(NULL);
  apache_html_parse_message_handler_->set_buffer(NULL);
}

void ApacheRewriteDriverFactory::SetupMessageHandlers() {
  // TODO(jefftk): merge ApacheMessageHandler and NgxMessageHandler into
  // SystemMessageHandler and then move this into System.
  apache_message_handler_->SetPidString(static_cast<int64>(getpid()));
  apache_html_parse_message_handler_->SetPidString(
            static_cast<int64>(getpid()));
}

void ApacheRewriteDriverFactory::ShutDownFetchers() {
  if (mod_spdy_fetch_controller_.get() != NULL) {
    mod_spdy_fetch_controller_->ShutDown();
  }
}

void ApacheRewriteDriverFactory::SetCircularBuffer(
    SharedCircularBuffer* buffer) {
  // TODO(jefftk): merge ApacheMessageHandler and NgxMessageHandler into
  // SystemMessageHandler and then move this into System.
  apache_message_handler_->set_buffer(buffer);
  apache_html_parse_message_handler_->set_buffer(buffer);
}

void ApacheRewriteDriverFactory::Initialize() {
  ApacheConfig::Initialize();
  RewriteDriverFactory::Initialize();
}

void ApacheRewriteDriverFactory::InitStats(Statistics* statistics) {
  // Init standard system stats.
  SystemRewriteDriverFactory::InitStats(statistics);

  // Init Apache-specific stats.
  ApacheServerContext::InitStats(statistics);
}

void ApacheRewriteDriverFactory::Terminate() {
  RewriteDriverFactory::Terminate();
  ApacheConfig::Terminate();
  PthreadSharedMem::Terminate();
}

ApacheServerContext* ApacheRewriteDriverFactory::MakeApacheServerContext(
    server_rec* server) {
  ApacheServerContext* server_context =
      new ApacheServerContext(this, server, version_);
  uninitialized_server_contexts_.insert(server_context);
  return server_context;
}

bool ApacheRewriteDriverFactory::PoolDestroyed(
    ApacheServerContext* server_context) {
  if (uninitialized_server_contexts_.erase(server_context) == 1) {
    delete server_context;
  }

  // Returns true if all the ServerContexts known by the factory and its
  // superclass are finished.  Then it's time to destroy the factory.  Note
  // that ApacheRewriteDriverFactory keeps track of ServerContexts that
  // are partially constructed.  RewriteDriverFactory keeps track of
  // ServerContexts that are already serving requests.  We need to clean
  // all of them out before we can terminate the driver.
  bool no_active_server_contexts = TerminateServerContext(server_context);
  return (no_active_server_contexts && uninitialized_server_contexts_.empty());
}

ApacheConfig* ApacheRewriteDriverFactory::NewRewriteOptions() {
  return new ApacheConfig(hostname_identifier(), thread_system());
}

ApacheConfig* ApacheRewriteDriverFactory::NewRewriteOptionsForQuery() {
  return new ApacheConfig("query", thread_system());
}

int ApacheRewriteDriverFactory::requests_per_host() {
  CHECK(thread_counts_finalized_);
  return std::min(4, num_rewrite_threads_);
}

}  // namespace net_instaweb
