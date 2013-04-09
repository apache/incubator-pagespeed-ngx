// Copyright 2013 Google Inc.
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
// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/util/public/console_suggestions.h"

#include <algorithm>                    // for sort

#include "base/logging.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

ConsoleSuggestionsFactory::~ConsoleSuggestionsFactory() {
}

// Stat helper functions.
int64 ConsoleSuggestionsFactory::StatValue(StringPiece var_name) {
  Variable* var = stats_->GetVariable(var_name);
  if (var == NULL) {
    LOG(DFATAL) << "Invalid statistics name: " << var_name;
    return 0;
  } else {
    return var->Get();
  }
}

double ConsoleSuggestionsFactory::StatRatio(StringPiece numerator,
                                            StringPiece denominator) {
  int64 denom_value = StatValue(denominator);
  if (denom_value == 0) {
    return 0.0;
  } else {
    double num_value = StatValue(numerator);
    return num_value / denom_value;
  }
}

// Returns ratio of bad / (good + bad).
double ConsoleSuggestionsFactory::StatSumRatio(StringPiece bad,
                                               StringPiece good) {
  int64 bad_value = StatValue(bad);
  int64 good_value = StatValue(good);
  int64 total = bad_value + good_value;
  if (total == 0) {
    return 0.0;
  } else {
    return static_cast<double>(bad_value) / total;
  }
}

namespace {

bool CompareSuggestions(ConsoleSuggestion a, ConsoleSuggestion b) {
  // Note: largest importance first.
  return (a.importance > b.importance);
}

}  // namespace

void ConsoleSuggestionsFactory::GenerateSuggestions() {
  // Serf fetch failure rate.
  // TODO(sligocki): This should probably be in the Apache-specific code.
  double fetch_failure_rate = StatRatio("serf_fetch_failure_count",
                                        "serf_fetch_request_count");
  suggestions_.push_back(ConsoleSuggestion(
      fetch_failure_rate,
      StringPrintf("Fetch failure rate: %.2f%%", fetch_failure_rate * 100),
      // TODO(sligocki): Add doc links.
      ""));

  // Resource fetch failures.
  double resource_fetch_failure_rate =
      StatSumRatio("num_resource_fetch_failures",
                   "num_resource_fetch_successes");
  suggestions_.push_back(ConsoleSuggestion(
      resource_fetch_failure_rate,
      StringPrintf("Resource fetch failure rate: %.2f%%",
                   resource_fetch_failure_rate * 100),
      ""));

  // Cache miss rate.
  double cache_miss_rate = StatSumRatio("cache_misses", "cache_hits");
  suggestions_.push_back(ConsoleSuggestion(
      cache_miss_rate,
      StringPrintf("Cache miss rate: %.2f%%", cache_miss_rate * 100),
      ""));

  // CSS rewrite failure rate.
  double css_failure_rate = StatSumRatio("css_filter_parse_failures",
                                         "css_filter_blocks_rewritten");
  suggestions_.push_back(ConsoleSuggestion(
      css_failure_rate,
      StringPrintf("CSS parser failure rate: %.2f%%", css_failure_rate * 100),
      ""));

  // TODO(sligocki): Images

  // Javascript minification failure rate.
  double js_failure_rate = StatSumRatio("javascript_minification_failures",
                                        "javascript_blocks_minified");
  suggestions_.push_back(ConsoleSuggestion(
      js_failure_rate,
      StringPrintf("Javascript minification failure rate: %.2f%%",
                   js_failure_rate * 100),
      ""));

  // Most important suggestions first.
  std::sort(suggestions_.begin(), suggestions_.end(), CompareSuggestions);

  // TODO(sligocki): Strip suggestions down. For example, only display top
  // 10 suggestions. Or only display suggestions that are above some cutoff
  // of importance.
}

}  // namespace net_instaweb
