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
  return Ratio(StatValue(numerator), StatValue(denominator));
}

double ConsoleSuggestionsFactory::Ratio(int64 num_value, int64 denom_value) {
  if (denom_value == 0) {
    return 0.0;
  } else {
    return static_cast<double>(num_value) / denom_value;
  }
}

double ConsoleSuggestionsFactory::StatSumRatio(StringPiece bad,
                                               StringPiece good) {
  return SumRatio(StatValue(bad), StatValue(good));
};

double ConsoleSuggestionsFactory::SumRatio(int64 bad_value, int64 good_value) {
  return Ratio(bad_value, good_value + bad_value);
}

namespace {

bool CompareSuggestions(ConsoleSuggestion a, ConsoleSuggestion b) {
  // Note: largest importance first.
  return (a.importance > b.importance);
}

}  // namespace

void ConsoleSuggestionsFactory::AddConsoleSuggestion(
    double stat_failure_ratio, const char* message_format,
    const GoogleString& doc_link) {
  suggestions_.push_back(ConsoleSuggestion(
      stat_failure_ratio,
      StringPrintf(message_format, stat_failure_ratio * 100),
      doc_link));
}

void ConsoleSuggestionsFactory::GenerateSuggestions() {
  // Cannot fetch resources.
  // TODO(sligocki): This should probably be in the Apache-specific code.
  AddConsoleSuggestion(StatRatio("serf_fetch_failure_count",
                                 "serf_fetch_request_count"),
                       "Resources not loaded because of fetch failure: %.2f%%",
                       // TODO(sligocki): Add doc links.
                       "");

  // Domains are not authorized.
  // TODO(sligocki): Use constants (rather than string literals) for these
  // stat names.
  AddConsoleSuggestion(StatSumRatio("resource_url_domain_rejections",
                                    "resource_url_domain_acceptances"),
                       "Resources not rewritten because domain wasn't "
                       "authorized: %.2f%%",
                       "");

  // Resources are not cacheable.
  AddConsoleSuggestion(
      StatSumRatio("num_cache_control_not_rewritable_resources",
                   "num_cache_control_rewritable_resources"),
      "Resources not rewritten because of restrictive Cache-Control "
      "headers: %.2f%%",
      "");

  // Cache too small (High backend cache miss rate).
  AddConsoleSuggestion(StatSumRatio("cache_backend_misses",
                                    "cache_backend_hits"),
                       "Cache evictions: %.2f%%",
                       "");

  // Resources accessed too infrequently (High cache expirations).
  {
    int64 bad = StatValue("cache_expirations");
    // Total number of Find() calls.
    int64 total = StatValue("cache_hits") + StatValue("cache_misses");
    AddConsoleSuggestion(Ratio(bad, total),
                         "Cache expirations: %.2f%%",
                         "");
  }

  // Cannot parse CSS.
  // TODO(sligocki): This counts per rewrite, it seems like it should count
  // per time CSS URL is seen in HTML.
  AddConsoleSuggestion(StatSumRatio("css_filter_parse_failures",
                                    "css_filter_blocks_rewritten"),
                       "CSS files not rewritten because of parse errors: "
                       "%.2f%%",
                       "");

  // Cannot parse JavaScript.
  AddConsoleSuggestion(StatSumRatio("javascript_minification_failures",
                                    "javascript_blocks_minified"),
                       "JavaScript minification failures: %.2f%%",
                       "");

  // Image reading failure.
  {
    double good = StatValue("image_rewrites") +
        // These are considered good because they were read and we could have
        // optimized them, the only reason we didn't was because they were
        // already optimal.
        StatValue("image_rewrites_dropped_nosaving_resize") +
        StatValue("image_rewrites_dropped_nosaving_noresize");
    double bad = StatValue("image_norewrites_high_resolution") +
        StatValue("image_rewrites_dropped_decode_failure") +
        StatValue("image_rewrites_dropped_server_write_fail") +
        StatValue("image_rewrites_dropped_mime_type_unknown") +
        StatValue("image_norewrites_high_resolution");
    // TODO(sligocki): We don't seem to be tracking TimedVariables as
    // normal Variables in mod_pagespeed. Fix this.
    // + StatValue("image_rewrites_dropped_due_to_load");
    AddConsoleSuggestion(SumRatio(bad, good),
                         "Image rewrite failures: %.2f%%",
                         "");
  }

  // CSS not combinable.
  {
    double good = StatValue("css_file_count_reduction");
    double total = StatValue("css_combine_opportunities");
    AddConsoleSuggestion(Ratio(total - good, total),
                         "CSS combine opportunities missed: %.2f%%",
                         "");
  }

  // Most important suggestions first.
  std::sort(suggestions_.begin(), suggestions_.end(), CompareSuggestions);

  // TODO(sligocki): Strip suggestions down. For example, only display top
  // 10 suggestions. Or only display suggestions that are above some cutoff
  // of importance.
}

}  // namespace net_instaweb
