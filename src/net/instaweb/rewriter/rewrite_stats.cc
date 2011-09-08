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
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/waveform.h"

namespace {

// resource_url_domain_rejections counts the number of urls on a page that we
// could have rewritten, except that they lay in a domain that did not
// permit resource rewriting relative to the current page.
const char kResourceUrlDomainRejections[] = "resource_url_domain_rejections";
static const char kCachedOutputMissedDeadline[] =
    "rewrite_cached_output_missed_deadline";
static const char kCachedOutputHits[] = "rewrite_cached_output_hits";
static const char kCachedOutputMisses[] = "rewrite_cached_output_misses";
const char kInstawebResource404Count[] = "resource_404_count";
const char kInstawebSlurp404Count[] = "slurp_404_count";
const char kResourceFetchesCached[] = "resource_fetches_cached";
const char kResourceFetchConstructSuccesses[] =
    "resource_fetch_construct_successes";
const char kResourceFetchConstructFailures[] =
    "resource_fetch_construct_failures";
const char kNumFlushes[] = "num_flushes";

// Variables for the beacon to increment.  These are currently handled in
// mod_pagespeed_handler on apache.  The average load time in milliseconds is
// total_page_load_ms / page_load_count.  Note that these are not updated
// together atomically, so you might get a slightly bogus value.
const char kTotalPageLoadMs[] = "total_page_load_ms";
const char kPageLoadCount[] = "page_load_count";

const int kNumWaveformSamples = 200;

}  // namespace

namespace net_instaweb {

// In Apache, this is called in the root process to establish shared memory
// boundaries prior to the primary initialization of RewriteDriverFactories.
//
// Note that there are other statistics owned by filters and subsystems,
// that must get the some treatment.
void RewriteStats::Initialize(Statistics* statistics) {
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
  statistics->AddVariable(kNumFlushes);
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
      num_flushes_(
          stats->GetVariable(kNumFlushes)),
      page_load_count_(
          stats->GetVariable(kPageLoadCount)),
      resource_404_count_(
          stats->GetVariable(kInstawebResource404Count)),
      resource_url_domain_rejections_(
          stats->GetVariable(kResourceUrlDomainRejections)),
      slurp_404_count_(
          stats->GetVariable(kInstawebSlurp404Count)),
      succeeded_filter_resource_fetches_(
          stats->GetVariable(kResourceFetchConstructSuccesses)),
      total_page_load_ms_(
          stats->GetVariable(kTotalPageLoadMs)),
      rewrite_thread_queue_depth_(
          new Waveform(thread_system, timer, kNumWaveformSamples)) {
}

RewriteStats::~RewriteStats() {
}

}  // namespace net_instaweb
