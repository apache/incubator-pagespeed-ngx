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

#include "ngx_request_context.h"
#include "ngx_rewrite_options.h"
#include "ngx_rewrite_driver_factory.h"
#include "net/instaweb/apache/add_headers_fetcher.h"
#include "net/instaweb/apache/loopback_route_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/split_statistics.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

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
  DCHECK(!initialized_);
  if (!initialized_) {
    initialized_ = true;
    set_lock_manager(ngx_factory_->caches()->GetLockManager(config()));

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
    }

    ngx_factory_->InitServerContext(this);
    // TODO(oschaaf): in mod_pagespeed, the ServerContext owns
    // the fetchers, and sets up the UrlAsyncFetcherStats here
  }
}

void NgxServerContext::CreateLocalStatistics(
    Statistics* global_statistics) {
  local_statistics_ =
      ngx_factory_->AllocateAndInitSharedMemStatistics(
          hostname_identifier(),
          config()->statistics_logging_enabled(),
          config()->statistics_logging_interval_ms(),
          config()->statistics_logging_file());
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
  // TODO(oschaaf): Once the ServerContext owns the fetchers,
  // initialise UrlAsyncFetcherStats here
}

void NgxServerContext::ApplySessionFetchers(
    const RequestContextPtr& request, RewriteDriver* driver) {
  const NgxRewriteOptions* conf = NgxRewriteOptions::DynamicCast(
      driver->options());
  CHECK(conf != NULL);
  NgxRequestContext* ngx_request = NgxRequestContext::DynamicCast(
      request.get());
  if (ngx_request == NULL) {
    return;  // decoding_driver has a null RequestContext.
  }

  // Note that these fetchers are applied in the opposite order of how they are
  // added
  // TODO(oschaaf): in mod_pagespeed, LoopbackRouteFetcher is not added when
  // one of these is set:  disable_loopback_routing, slurping_enabled, or
  // test_proxy.

  // Note the port here is our port, not from the request, since
  // LoopbackRouteFetcher may decide we should be talking to ourselves.
  driver->SetSessionFetcher(new LoopbackRouteFetcher(
      driver->options(), ngx_request->local_ip(),
      ngx_request->local_port(), driver->async_fetcher()));

  if (driver->options()->num_custom_fetch_headers() > 0) {
    driver->SetSessionFetcher(new AddHeadersFetcher(driver->options(),
                                                    driver->async_fetcher()));
  }
}

}  // namespace net_instaweb
