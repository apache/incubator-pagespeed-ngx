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

#include "ngx_server_context.h"

extern "C" {
  #include <ngx_http.h>
}

#include "ngx_pagespeed.h"
#include "ngx_message_handler.h"
#include "ngx_rewrite_driver_factory.h"
#include "ngx_rewrite_options.h"
#include "net/instaweb/http/public/url_async_fetcher_stats.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/system/public/add_headers_fetcher.h"
#include "net/instaweb/system/public/loopback_route_fetcher.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/system/public/system_request_context.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/split_statistics.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

namespace {

const char kLocalFetcherStatsPrefix[] = "http";

}  // namespace

const char kCacheFlushCount[] = "cache_flush_count";
const char kCacheFlushTimestampMs[] = "cache_flush_timestamp_ms";

// Statistics histogram names.
const char kHtmlRewriteTimeUsHistogram[] = "Html Time us Histogram";


NgxServerContext::NgxServerContext(NgxRewriteDriverFactory* factory)
    : SystemServerContext(factory),
      ngx_factory_(factory),
      initialized_(false) {
}

NgxServerContext::~NgxServerContext() {
}

NgxRewriteOptions* NgxServerContext::config() {
  return NgxRewriteOptions::DynamicCast(global_options());
}

void NgxServerContext::ChildInit() {
  // TODO(jefftk): move this function into SystemServerContext

  DCHECK(!initialized_);
  if (!initialized_) {
    initialized_ = true;
    set_lock_manager(ngx_factory_->caches()->GetLockManager(config()));
    UrlAsyncFetcher* fetcher = ngx_factory_->GetFetcher(config());
    set_default_system_fetcher(fetcher);

    if (split_statistics_.get() != NULL) {
      // Readjust the SHM stuff for the new process
      local_statistics_->Init(false, message_handler());

      // Create local stats for the ServerContext, and fill in its
      // statistics() and rewrite_stats() using them; if we didn't do this here
      // they would get set to the factory's by the InitServerContext call
      // below.
      set_statistics(split_statistics_.get());
      local_rewrite_stats_.reset(new RewriteStats(
          split_statistics_.get(), ngx_factory_->thread_system(),
          ngx_factory_->timer()));
      set_rewrite_stats(local_rewrite_stats_.get());

      // In case of gzip fetching, we will have the UrlAsyncFetcherStats take
      // care of it rather than the original fetcher, so we get correct
      // numbers for bytes fetched.
      if (ngx_factory_->fetch_with_gzip()) {
        fetcher->set_fetch_with_gzip(false);
      }
      stats_fetcher_.reset(new UrlAsyncFetcherStats(
          kLocalFetcherStatsPrefix, fetcher,
          ngx_factory_->timer(), split_statistics_.get()));
      if (ngx_factory_->fetch_with_gzip()) {
        stats_fetcher_->set_fetch_with_gzip(true);
      }
      set_default_system_fetcher(stats_fetcher_.get());
    }

    // To allow Flush to come in while multiple threads might be
    // referencing the signature, we must be able to mutate the
    // timestamp and signature atomically.  RewriteOptions supports
    // an optional read/writer lock for this purpose.
    global_options()->set_cache_invalidation_timestamp_mutex(
        thread_system()->NewRWLock());
    ngx_factory_->InitServerContext(this);
  }
}

void NgxServerContext::CreateLocalStatistics(
    Statistics* global_statistics) {
  local_statistics_ =
      ngx_factory_->AllocateAndInitSharedMemStatistics(
          true /* local */, hostname_identifier(), *config());
  split_statistics_.reset(new SplitStatistics(
      ngx_factory_->thread_system(), local_statistics_, global_statistics));
  // local_statistics_ was ::InitStat'd by AllocateAndInitSharedMemStatistics,
  // but we need to take care of split_statistics_.
  NgxRewriteDriverFactory::InitStats(split_statistics_.get());
}

void NgxServerContext::InitStats(Statistics* statistics) {
  // TODO(oschaaf): we need to port the cache flush mechanism
  statistics->AddVariable(kCacheFlushCount);
  statistics->AddVariable(kCacheFlushTimestampMs);
  Histogram* html_rewrite_time_us_histogram =
      statistics->AddHistogram(kHtmlRewriteTimeUsHistogram);
  // We set the boundary at 2 seconds which is about 2 orders of magnitude
  // worse than anything we have reasonably seen, to make sure we don't
  // cut off actual samples.
  html_rewrite_time_us_histogram->SetMaxValue(2 * Timer::kSecondUs);
  UrlAsyncFetcherStats::InitStats(kLocalFetcherStatsPrefix, statistics);
}

SystemRequestContext* NgxServerContext::NewRequestContext(
    ngx_http_request_t* r) {
  // Based on ngx_http_variable_server_port.
  bool port_set = false;
  int local_port;
#if (NGX_HAVE_INET6)
  if (r->connection->local_sockaddr->sa_family == AF_INET6) {
    local_port = ntohs(reinterpret_cast<struct sockaddr_in6*>(
        r->connection->local_sockaddr)->sin6_port);
    port_set = true;
  }
#endif
  if (!port_set) {
    local_port = ntohs(reinterpret_cast<struct sockaddr_in*>(
        r->connection->local_sockaddr)->sin_port);
  }

  ngx_str_t local_ip;
  u_char addr[NGX_SOCKADDR_STRLEN];
  local_ip.len = NGX_SOCKADDR_STRLEN;
  local_ip.data = addr;
  ngx_int_t rc = ngx_connection_local_sockaddr(r->connection, &local_ip, 0);
  if (rc != NGX_OK) {
    local_ip.len = 0;
  }

  return new SystemRequestContext(thread_system()->NewMutex(),
                                  timer(),
                                  local_port,
                                  ngx_psol::str_to_string_piece(local_ip));
}

}  // namespace net_instaweb
