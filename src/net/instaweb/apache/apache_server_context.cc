// Copyright 2011 Google Inc.
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

#include "net/instaweb/apache/apache_server_context.h"

#include "httpd.h"
#include "base/logging.h"
#include "net/instaweb/apache/apache_config.h"
#include "net/instaweb/apache/apache_request_context.h"
#include "net/instaweb/apache/apache_rewrite_driver_factory.h"
#include "net/instaweb/apache/mod_spdy_fetcher.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/url_async_fetcher_stats.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_pool.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/system/public/add_headers_fetcher.h"
#include "net/instaweb/system/public/loopback_route_fetcher.h"
#include "net/instaweb/system/public/system_caches.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/shared_mem_statistics.h"
#include "net/instaweb/util/public/split_statistics.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Statistics histogram names.
const char kHtmlRewriteTimeUsHistogram[] = "Html Time us Histogram";

const char kLocalFetcherStatsPrefix[] = "http";

class SpdyOptionsRewriteDriverPool : public RewriteDriverPool {
 public:
  explicit SpdyOptionsRewriteDriverPool(ApacheServerContext* context)
      : apache_server_context_(context) {
  }

  virtual RewriteOptions* TargetOptions() const {
    DCHECK(apache_server_context_->SpdyConfig() != NULL);
    return apache_server_context_->SpdyConfig();
  }

 private:
  ApacheServerContext* apache_server_context_;
};

}  // namespace

ApacheServerContext::ApacheServerContext(
    ApacheRewriteDriverFactory* factory,
    server_rec* server,
    const StringPiece& version)
    : SystemServerContext(factory),
      apache_factory_(factory),
      server_rec_(server),
      version_(version.data(), version.size()),
      hostname_identifier_(StrCat(server->server_hostname, ":",
                                  IntegerToString(server->port))),
      initialized_(false),
      local_statistics_(NULL),
      spdy_driver_pool_(NULL),
      html_rewrite_time_us_histogram_(NULL) {
  config()->set_description(hostname_identifier_);
  // We may need the message handler for error messages very early, before
  // we get to InitServerContext in ChildInit().
  set_message_handler(apache_factory_->message_handler());

  // Currently, mod_pagespeed always runs upstream of mod_headers when used as
  // an origin server.  Note that in a proxy application, this might not be the
  // case.  I'm not sure how we can detect this on a per-request basis so
  // that might require a small refactor.
  //
  // TODO(jmarantz): We'd like to change this for various reasons but are unsure
  // of the impact.
  set_response_headers_finalized(false);
}

ApacheServerContext::~ApacheServerContext() {
}

void ApacheServerContext::InitStats(Statistics* statistics) {
  SystemServerContext::InitStats(statistics);
  Histogram* html_rewrite_time_us_histogram =
      statistics->AddHistogram(kHtmlRewriteTimeUsHistogram);
  // We set the boundary at 2 seconds which is about 2 orders of magnitude
  // worse than anything we have reasonably seen, to make sure we don't
  // cut off actual samples.
  html_rewrite_time_us_histogram->SetMaxValue(2 * Timer::kSecondUs);
  UrlAsyncFetcherStats::InitStats(kLocalFetcherStatsPrefix, statistics);
}

bool ApacheServerContext::InitPath(const GoogleString& path) {
  if (file_system()->IsDir(path.c_str(), message_handler()).is_true()) {
    return true;
  }
  bool ok = file_system()->RecursivelyMakeDir(path, message_handler());
  if (ok) {
    apache_factory_->AddCreatedDirectory(path);
  }
  return ok;
}

ApacheConfig* ApacheServerContext::config() {
  return ApacheConfig::DynamicCast(global_options());
}

ApacheConfig* ApacheServerContext::SpdyConfigOverlay() {
  if (spdy_config_overlay_.get() == NULL) {
    spdy_config_overlay_.reset(new ApacheConfig(
        "spdy_overlay", thread_system()));
    // We want to copy any implicit rewrite level from the parent,
    // so we don't end up overriding it with passthrough. It's also OK
    // to forward explicit one to an implicit one here, since an implicit
    // will never override an explicit one (even if its different).
    spdy_config_overlay_->SetDefaultRewriteLevel(config()->level());
  }
  return spdy_config_overlay_.get();
}

ApacheConfig* ApacheServerContext::NonSpdyConfigOverlay() {
  if (non_spdy_config_overlay_.get() == NULL) {
    non_spdy_config_overlay_.reset(new ApacheConfig(
        "non_spdy_overlay", thread_system()));
    // See ::SpdyConfigOverlay for explanation.
    non_spdy_config_overlay_->SetDefaultRewriteLevel(config()->level());
  }
  return non_spdy_config_overlay_.get();
}

void ApacheServerContext::CollapseConfigOverlaysAndComputeSignatures() {
  if (spdy_config_overlay_.get() != NULL ||
      non_spdy_config_overlay_.get() != NULL) {
    // We need separate SPDY/non-SPDY configs if we have any
    // <IfModpagespeed spdy> or <IfModpagespeed !spdy> blocks.
    // We compute the SPDY one first since we need config() to be
    // the common config and not common + !spdy.
    spdy_specific_config_.reset(config()->Clone());
    spdy_specific_config_->set_cache_invalidation_timestamp_mutex(
        thread_system()->NewRWLock());
    if (spdy_config_overlay_.get() != NULL) {
      spdy_specific_config_->Merge(*spdy_config_overlay_);
    }
    ComputeSignature(spdy_specific_config_.get());
  }

  if (non_spdy_config_overlay_.get() != NULL) {
    config()->Merge(*non_spdy_config_overlay_);
  }

  ComputeSignature(config());

  if (spdy_specific_config_.get() != NULL) {
    spdy_driver_pool_ = new SpdyOptionsRewriteDriverPool(this);
    ManageRewriteDriverPool(spdy_driver_pool_);
  }
}

void ApacheServerContext::CreateLocalStatistics(
    Statistics* global_statistics) {
  local_statistics_ =
      apache_factory_->AllocateAndInitSharedMemStatistics(
          hostname_identifier(), config());
  split_statistics_.reset(new SplitStatistics(
      apache_factory_->thread_system(), local_statistics_, global_statistics));
  // local_statistics_ was ::InitStat'd by AllocateAndInitSharedMemStatistics,
  // but we need to take care of split_statistics_.
  ApacheRewriteDriverFactory::InitStats(split_statistics_.get());
}

void ApacheServerContext::ChildInit() {
  DCHECK(!initialized_);
  if (!initialized_) {
    initialized_ = true;
    set_lock_manager(apache_factory_->caches()->GetLockManager(config()));
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
    apache_factory_->InitServerContext(this);

    html_rewrite_time_us_histogram_ = statistics()->GetHistogram(
        kHtmlRewriteTimeUsHistogram);
    html_rewrite_time_us_histogram_->SetMaxValue(2 * Timer::kSecondUs);
  }
}

bool ApacheServerContext::PoolDestroyed() {
  ShutDownDrivers();
  return apache_factory_->PoolDestroyed(this);
}

bool ApacheServerContext::UpdateCacheFlushTimestampMs(int64 timestamp_ms) {
  bool flushed = SystemServerContext::UpdateCacheFlushTimestampMs(timestamp_ms);
  if (SpdyConfig() != NULL) {
    // We need to make sure to update the invalidation timestamp in the
    // SPDY configuration as well, so it also gets any cache flushes.
    flushed |= SpdyConfig()->UpdateCacheInvalidationTimestampMs(timestamp_ms);
  }
  return flushed;
}

void ApacheServerContext::AddHtmlRewriteTimeUs(int64 rewrite_time_us) {
  if (html_rewrite_time_us_histogram_ != NULL) {
    html_rewrite_time_us_histogram_->Add(rewrite_time_us);
  }
}

RewriteDriverPool* ApacheServerContext::SelectDriverPool(bool using_spdy) {
  if (using_spdy && (SpdyConfig() != NULL)) {
    return spdy_driver_pool();
  }
  return standard_rewrite_driver_pool();
}

void ApacheServerContext::ApplySessionFetchers(
    const RequestContextPtr& request, RewriteDriver* driver) {
  const ApacheConfig* conf = ApacheConfig::DynamicCast(driver->options());
  CHECK(conf != NULL);
  ApacheRequestContext* apache_request = ApacheRequestContext::DynamicCast(
      request.get());
  if (apache_request == NULL) {
    return;  // decoding_driver has a null RequestContext.
  }

  // Note that these fetchers are applied in the opposite order of how they are
  // added: the last one added here is the first one applied and vice versa.
  //
  // Currently, we want AddHeadersFetcher running first, then perhaps
  // ModSpdyFetcher and then LoopbackRouteFetcher (and then Serf).
  //
  // We want AddHeadersFetcher to run before the ModSpdyFetcher since we
  // want any headers it adds to be visible.
  //
  // We want ModSpdyFetcher to run before LoopbackRouteFetcher as it needs
  // to know the request hostname, which LoopbackRouteFetcher could potentially
  // rewrite to 127.0.0.1; and it's OK without the rewriting since it will
  // always talk to the local machine anyway.
  if (!apache_factory_->disable_loopback_routing() &&
      !config()->slurping_enabled() &&
      !config()->test_proxy()) {
    // Note the port here is our port, not from the request, since
    // LoopbackRouteFetcher may decide we should be talking to ourselves.
    driver->SetSessionFetcher(new LoopbackRouteFetcher(
        driver->options(), apache_request->local_ip(),
        apache_request->local_port(), driver->async_fetcher()));
  }

  if (conf->experimental_fetch_from_mod_spdy() &&
      apache_request->use_spdy_fetcher()) {
    driver->SetSessionFetcher(new ModSpdyFetcher(
        apache_factory_->mod_spdy_fetch_controller(), apache_request->url(),
        driver, apache_request->spdy_connection_factory()));
  }

  if (driver->options()->num_custom_fetch_headers() > 0) {
    driver->SetSessionFetcher(new AddHeadersFetcher(driver->options(),
                                                    driver->async_fetcher()));
  }
}

void ApacheServerContext::InitProxyFetchFactory() {
  proxy_fetch_factory_.reset(new ProxyFetchFactory(this));
}

}  // namespace net_instaweb
