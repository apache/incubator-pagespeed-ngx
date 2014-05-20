/*
 * Copyright 2011 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/rewrite_stats.h"

#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/waveform.h"

namespace net_instaweb {

namespace {

const char kCachedOutputMissedDeadline[] =
    "rewrite_cached_output_missed_deadline";
const char kCachedOutputHits[] = "rewrite_cached_output_hits";
const char kCachedOutputMisses[] = "rewrite_cached_output_misses";
const char kInstawebResource404Count[] = "resource_404_count";
const char kInstawebSlurp404Count[] = "slurp_404_count";
const char kResourceFetchesCached[] = "resource_fetches_cached";
const char kResourceFetchConstructSuccesses[] =
    "resource_fetch_construct_successes";
const char kResourceFetchConstructFailures[] =
    "resource_fetch_construct_failures";
const char kNumFlushes[] = "num_flushes";
const char kFallbackResponsesServed[] = "num_fallback_responses_served";
const char kProactivelyFreshenUserFacingRequest[] =
    "num_proactively_freshen_user_facing_request";
const char kFallbackResponsesServedWhileRevalidate[] =
    "num_fallback_responses_served_while_revalidate";
const char kNumConditionalRefreshes[] = "num_conditional_refreshes";

const char kIproServed[] = "ipro_served";
const char kIproNotInCache[] = "ipro_not_in_cache";
const char kIproNotRewritable[] = "ipro_not_rewritable";

const char* kWaveFormCounters[RewriteDriverFactory::kNumWorkerPools] = {
  "html-worker-queue-depth",
  "rewrite-worker-queue-depth",
  "low-priority-worked-queue-depth"
};

// Variables for the beacon to increment.  These are currently handled in
// mod_pagespeed_handler on apache.  The average load time in milliseconds is
// total_page_load_ms / page_load_count.  Note that these are not updated
// together atomically, so you might get a slightly bogus value.
//
// We also keep a histogram, kBeaconTimingsMsHistogram of these.
const char kTotalPageLoadMs[] = "total_page_load_ms";
const char kPageLoadCount[] = "page_load_count";

const int kNumWaveformSamples = 200;

// Histogram names.
const char kBeaconTimingsMsHistogram[] = "Beacon Reported Load Time (ms)";
const char kFetchLatencyHistogram[] = "Pagespeed Resource Latency Histogram";
const char kRewriteLatencyHistogram[] = "Rewrite Latency Histogram";
const char kBackendLatencyHistogram[] =
    "Backend Fetch First Byte Latency Histogram";

// TimedVariable names.
const char kTotalFetchCount[] = "total_fetch_count";
const char kTotalRewriteCount[] = "total_rewrite_count";
const char kRewritesExecuted[] = "num_rewrites_executed";
const char kRewritesDropped[] = "num_rewrites_dropped";

}  // namespace

const char RewriteStats::kNumCacheControlRewritableResources[] =
    "num_cache_control_rewritable_resources";
const char RewriteStats::kNumCacheControlNotRewritableResources[] =
    "num_cache_control_not_rewritable_resources";
const char RewriteStats::kNumResourceFetchSuccesses[] =
    "num_resource_fetch_successes";
const char RewriteStats::kNumResourceFetchFailures[] =
    "num_resource_fetch_failures";
// Num of URLs we could have rewritten and were authorized to rewrite.
const char RewriteStats::kResourceUrlDomainAcceptances[] =
    "resource_url_domain_acceptances";
// Num of URLs we could have rewritten, but were not authorized to rewrite
// because of the domain they are on.
const char RewriteStats::kResourceUrlDomainRejections[] =
    "resource_url_domain_rejections";

const char RewriteStats::kDownstreamCachePurgeAttempts[] =
    "downstream_cache_purge_attempts";
const char RewriteStats::kSuccessfulDownstreamCachePurges[] =
    "successful_downstream_cache_purges";

// In Apache, this is called in the root process to establish shared memory
// boundaries prior to the primary initialization of RewriteDriverFactories.
//
// Note that there are other statistics owned by filters and subsystems,
// that must get the some treatment.
void RewriteStats::InitStats(Statistics* statistics) {
  statistics->AddVariable(kResourceUrlDomainAcceptances);
  statistics->AddVariable(kResourceUrlDomainRejections);
  statistics->AddVariable(kCachedOutputMissedDeadline);
  statistics->AddVariable(kCachedOutputHits);
  statistics->AddVariable(kCachedOutputMisses);
  statistics->AddVariable(kInstawebResource404Count);
  statistics->AddVariable(kInstawebSlurp404Count);
  statistics->AddVariable(kTotalPageLoadMs);
  statistics->AddVariable(kPageLoadCount);
  statistics->AddVariable(kResourceFetchesCached);
  statistics->AddVariable(kResourceFetchConstructSuccesses);
  statistics->AddVariable(kResourceFetchConstructFailures);
  statistics->AddVariable(kNumCacheControlRewritableResources);
  statistics->AddVariable(kNumCacheControlNotRewritableResources);
  statistics->AddVariable(kNumFlushes);
  statistics->AddHistogram(kBeaconTimingsMsHistogram);
  statistics->AddHistogram(kFetchLatencyHistogram);
  statistics->AddHistogram(kRewriteLatencyHistogram);
  statistics->AddHistogram(kBackendLatencyHistogram);
  statistics->AddVariable(kFallbackResponsesServed);
  statistics->AddVariable(kProactivelyFreshenUserFacingRequest);
  statistics->AddVariable(kFallbackResponsesServedWhileRevalidate);
  statistics->AddVariable(kNumConditionalRefreshes);
  statistics->AddVariable(kIproServed);
  statistics->AddVariable(kIproNotInCache);
  statistics->AddVariable(kIproNotRewritable);
  statistics->AddVariable(kDownstreamCachePurgeAttempts);
  statistics->AddVariable(kSuccessfulDownstreamCachePurges);
  statistics->AddTimedVariable(kTotalFetchCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kTotalRewriteCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kRewritesExecuted,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kRewritesDropped,
                               ServerContext::kStatisticsGroup);
  statistics->AddVariable(kNumResourceFetchSuccesses);
  statistics->AddVariable(kNumResourceFetchFailures);

  for (int i = 0; i < RewriteDriverFactory::kNumWorkerPools; ++i) {
    statistics->AddUpDownCounter(kWaveFormCounters[i]);
  }
}

// This is called when a RewriteDriverFactory is created, and adds
// common statistics to a public structure.
//
// Note that there are other statistics owned by filters and subsystems,
// that must get the some treatment.
RewriteStats::RewriteStats(Statistics* stats,
                           ThreadSystem* thread_system,
                           Timer* timer)
    : cached_output_hits_(
        stats->GetVariable(kCachedOutputHits)),
      cached_output_missed_deadline_(
          stats->GetVariable(kCachedOutputMissedDeadline)),
      cached_output_misses_(
          stats->GetVariable(kCachedOutputMisses)),
      cached_resource_fetches_(
          stats->GetVariable(kResourceFetchesCached)),
      failed_filter_resource_fetches_(
          stats->GetVariable(kResourceFetchConstructFailures)),
      num_cache_control_rewritable_resources_(
          stats->GetVariable(kNumCacheControlRewritableResources)),
      num_cache_control_not_rewritable_resources_(
          stats->GetVariable(kNumCacheControlNotRewritableResources)),
      num_flushes_(
          stats->GetVariable(kNumFlushes)),
      page_load_count_(
          stats->GetVariable(kPageLoadCount)),
      resource_404_count_(
          stats->GetVariable(kInstawebResource404Count)),
      resource_url_domain_acceptances_(
          stats->GetVariable(kResourceUrlDomainAcceptances)),
      resource_url_domain_rejections_(
          stats->GetVariable(kResourceUrlDomainRejections)),
      slurp_404_count_(
          stats->GetVariable(kInstawebSlurp404Count)),
      succeeded_filter_resource_fetches_(
          stats->GetVariable(kResourceFetchConstructSuccesses)),
      total_page_load_ms_(
          stats->GetVariable(kTotalPageLoadMs)),
      fallback_responses_served_(
          stats->GetVariable(kFallbackResponsesServed)),
      num_proactively_freshen_user_facing_request_(
          stats->GetVariable(kProactivelyFreshenUserFacingRequest)),
      fallback_responses_served_while_revalidate_(
          stats->GetVariable(kFallbackResponsesServedWhileRevalidate)),
      num_conditional_refreshes_(
          stats->GetVariable(kNumConditionalRefreshes)),
      ipro_served_(stats->GetVariable(kIproServed)),
      ipro_not_in_cache_(stats->GetVariable(kIproNotInCache)),
      ipro_not_rewritable_(stats->GetVariable(kIproNotRewritable)),
      downstream_cache_purge_attempts_(
          stats->GetVariable(kDownstreamCachePurgeAttempts)),
      successful_downstream_cache_purges_(
          stats->GetVariable(kSuccessfulDownstreamCachePurges)),
      beacon_timings_ms_histogram_(
          stats->GetHistogram(kBeaconTimingsMsHistogram)),
      fetch_latency_histogram_(
          stats->GetHistogram(kFetchLatencyHistogram)),
      rewrite_latency_histogram_(
          stats->GetHistogram(kRewriteLatencyHistogram)),
      backend_latency_histogram_(
          stats->GetHistogram(kBackendLatencyHistogram)),
      total_fetch_count_(stats->GetTimedVariable(kTotalFetchCount)),
      total_rewrite_count_(stats->GetTimedVariable(kTotalRewriteCount)),
      num_rewrites_executed_(stats->GetTimedVariable(kRewritesExecuted)),
      num_rewrites_dropped_(stats->GetTimedVariable(kRewritesDropped)) {
  // Timers are not guaranteed to go forward in time, however
  // Histograms will CHECK-fail given a negative value unless
  // EnableNegativeBuckets is called, allowing bars to be created with
  // negative x-axis labels in the histogram.
  // TODO(sligocki): Any reason not to set this by default for all Histograms?
  beacon_timings_ms_histogram_->EnableNegativeBuckets();
  fetch_latency_histogram_->EnableNegativeBuckets();
  rewrite_latency_histogram_->EnableNegativeBuckets();
  backend_latency_histogram_->EnableNegativeBuckets();

  for (int i = 0; i < RewriteDriverFactory::kNumWorkerPools; ++i) {
    thread_queue_depths_.push_back(
        new Waveform(thread_system, timer, kNumWaveformSamples,
                     stats->GetUpDownCounter(kWaveFormCounters[i])));
  }
}

RewriteStats::~RewriteStats() {
  STLDeleteElements(&thread_queue_depths_);
}

}  // namespace net_instaweb
