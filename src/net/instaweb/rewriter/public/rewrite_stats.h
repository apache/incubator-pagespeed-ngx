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

class Statistics;
class ThreadSystem;
class Timer;
class Variable;
class Waveform;

// Collects a few specific statistics variables related to Rewriting.
class RewriteStats {
 public:
  RewriteStats(Statistics* stats, ThreadSystem* thread_system, Timer* timer);
  ~RewriteStats();

  static void Initialize(Statistics* statistics);

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
  Variable* page_load_count() { return page_load_count_; }
  Waveform* thread_queue_depth(RewriteDriverFactory::WorkerPoolName name) {
    return thread_queue_depths_[name];
  }

 private:
  Variable* cached_output_hits_;
  Variable* cached_output_missed_deadline_;
  Variable* cached_output_misses_;
  Variable* cached_resource_fetches_;
  Variable* failed_filter_resource_fetches_;
  Variable* failed_filter_resource_fetches__;
  Variable* num_flushes_;
  Variable* page_load_count_;
  Variable* resource_404_count_;
  Variable* resource_url_domain_rejections_;
  Variable* slurp_404_count_;
  Variable* succeeded_filter_resource_fetches_;
  Variable* total_page_load_ms_;
  std::vector<Waveform*> thread_queue_depths_;

  DISALLOW_COPY_AND_ASSIGN(RewriteStats);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_STATS_H_
