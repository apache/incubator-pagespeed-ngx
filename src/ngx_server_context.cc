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

#include "ngx_cache.h"
#include "ngx_rewrite_options.h"
#include "ngx_rewrite_driver_factory.h"
#include "net/instaweb/util/public/file_system_lock_manager.h"

namespace net_instaweb {

NgxServerContext::NgxServerContext(NgxRewriteDriverFactory* factory) :
    ServerContext(factory),
    ngx_factory_(factory),
    initialized_(false){
}

NgxServerContext::~NgxServerContext() {
  delete lock_manager();
}

NgxRewriteOptions* NgxServerContext::config() {
  return NgxRewriteOptions::DynamicCast(global_options());
}

void NgxServerContext::ChildInit() {
  DCHECK(!initialized_);
  if (!initialized_) {
    initialized_ = true;
    NgxCache* cache = ngx_factory_->GetCache(config());

    set_lock_manager(cache->lock_manager());
    /*
    UrlAsyncFetcher* fetcher = apache_factory_->GetFetcher(config());
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
          split_statistics_.get(), apache_factory_->thread_system(),
          apache_factory_->timer()));
      set_rewrite_stats(local_rewrite_stats_.get());

      // In case of gzip fetching, we will have the UrlAsyncFetcherStats take
      // care of it rather than the original fetcher, so we get correct
      // numbers for bytes fetched.
      if (apache_factory_->fetch_with_gzip()) {
        fetcher->set_fetch_with_gzip(false);
      }
      stats_fetcher_.reset(new UrlAsyncFetcherStats(
          kLocalFetcherStatsPrefix, fetcher,
          apache_factory_->timer(), split_statistics_.get()));
      if (apache_factory_->fetch_with_gzip()) {
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
    */
    ngx_factory_->InitServerContext(this);
    /*
    html_rewrite_time_us_histogram_ = statistics()->GetHistogram(
        kHtmlRewriteTimeUsHistogram);
    html_rewrite_time_us_histogram_->SetMaxValue(2 * Timer::kSecondUs);
    */
  }
}

}  // namespace net_instaweb
