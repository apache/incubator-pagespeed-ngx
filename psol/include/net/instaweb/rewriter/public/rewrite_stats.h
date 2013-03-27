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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_STATS_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_STATS_H_

#include <vector>

#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class Histogram;
class Statistics;
class ThreadSystem;
class TimedVariable;
class Timer;
class Variable;
class Waveform;

// Collects a few specific statistics variables related to Rewriting.
class RewriteStats {
 public:
  static const char kNumResourceFetchSuccesses[];
  static const char kNumResourceFetchFailures[];

  RewriteStats(Statistics* stats, ThreadSystem* thread_system, Timer* timer);
  ~RewriteStats();

  static void InitStats(Statistics* statistics);

  Variable* cached_output_hits() { return cached_output_hits_; }
  Variable* cached_output_missed_deadline() {
    return cached_output_missed_deadline_; }
  Variable* cached_output_misses() { return cached_output_misses_;
  }
  Variable* cached_resource_fetches() { return cached_resource_fetches_; }
  Variable* failed_filter_resource_fetches() {
    return failed_filter_resource_fetches_;
  }
  Variable* num_flushes() { return num_flushes_; }
  Variable* resource_404_count() { return resource_404_count_; }
  Variable* resource_url_domain_rejections() {
    return resource_url_domain_rejections_;
  }
  Variable* slurp_404_count() { return slurp_404_count_; }
  Variable* succeeded_filter_resource_fetches() {
    return succeeded_filter_resource_fetches_;
  }
  Variable* total_page_load_ms() { return total_page_load_ms_; }
  // Note: page_load_count is a misnomer, it is really beacon count.
  // TODO(sligocki): Rename to something more clear.
  Variable* page_load_count() { return page_load_count_; }
  Variable* fallback_responses_served() {
    return fallback_responses_served_;
  }
  Variable* num_conditional_refreshes() { return num_conditional_refreshes_; }

  Variable* ipro_served() { return ipro_served_; }
  Variable* ipro_not_in_cache() { return ipro_not_in_cache_; }
  Variable* ipro_not_rewritable() { return ipro_not_rewritable_; }

  Histogram* beacon_timings_ms_histogram() {
    return beacon_timings_ms_histogram_;
  }
  // .pagespeed. resource latency in ms.
  Histogram* fetch_latency_histogram() { return fetch_latency_histogram_; }
  // HTML rewrite latency in ms.
  Histogram* rewrite_latency_histogram() { return rewrite_latency_histogram_; }
  Histogram* backend_latency_histogram() { return backend_latency_histogram_; }

  // Number of .pagespeed. resources fetched.
  TimedVariable* total_fetch_count() { return total_fetch_count_; }
  // Number of HTML pages rewritten.
  TimedVariable* total_rewrite_count() { return total_rewrite_count_; }

  Waveform* thread_queue_depth(RewriteDriverFactory::WorkerPoolCategory pool) {
    return thread_queue_depths_[pool];
  }

  TimedVariable* num_rewrites_executed() { return num_rewrites_executed_; }
  TimedVariable* num_rewrites_dropped() { return num_rewrites_dropped_; }

 private:
  Variable* cached_output_hits_;
  Variable* cached_output_missed_deadline_;
  Variable* cached_output_misses_;
  Variable* cached_resource_fetches_;
  Variable* failed_filter_resource_fetches_;
  Variable* num_flushes_;
  Variable* page_load_count_;
  Variable* resource_404_count_;
  Variable* resource_url_domain_rejections_;
  Variable* slurp_404_count_;
  Variable* succeeded_filter_resource_fetches_;
  Variable* total_page_load_ms_;
  Variable* fallback_responses_served_;
  Variable* num_conditional_refreshes_;
  Variable* ipro_served_;
  Variable* ipro_not_in_cache_;
  Variable* ipro_not_rewritable_;

  Histogram* beacon_timings_ms_histogram_;
  Histogram* fetch_latency_histogram_;
  Histogram* rewrite_latency_histogram_;
  Histogram* backend_latency_histogram_;

  TimedVariable* total_fetch_count_;
  TimedVariable* total_rewrite_count_;
  TimedVariable* num_rewrites_executed_;
  TimedVariable* num_rewrites_dropped_;

  std::vector<Waveform*> thread_queue_depths_;

  DISALLOW_COPY_AND_ASSIGN(RewriteStats);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_STATS_H_
