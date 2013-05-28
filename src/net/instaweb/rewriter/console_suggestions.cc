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

#include "net/instaweb/rewriter/public/console_suggestions.h"

#include <algorithm>                    // for sort

#include "base/logging.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/rewriter/public/css_combine_filter.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
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
  // Domains are not authorized.
  AddConsoleSuggestion(
      StatSumRatio(RewriteStats::kResourceUrlDomainRejections,
                   RewriteStats::kResourceUrlDomainAcceptances),
      "Resources not rewritten because domain wasn't "
      "authorized: %.2f%%",
      // TODO(sligocki): Add doc links.
      "");

  // Resources are not cacheable.
  AddConsoleSuggestion(
      StatSumRatio(RewriteStats::kNumCacheControlNotRewritableResources,
                   RewriteStats::kNumCacheControlRewritableResources),
      "Resources not rewritten because of restrictive Cache-Control "
      "headers: %.2f%%",
      "");


  // Cache too small (High backend cache miss rate).
  AddConsoleSuggestion(StatSumRatio(HTTPCache::kCacheBackendMisses,
                                    HTTPCache::kCacheBackendHits),
                       "Cache misses: %.f%%",
                       "");

  // Resources accessed too infrequently (High cache expirations).
  {
    int64 bad = StatValue(HTTPCache::kCacheExpirations);
    // Total number of Find() calls.
    int64 total = StatValue(HTTPCache::kCacheHits) +
        StatValue(HTTPCache::kCacheMisses);
    AddConsoleSuggestion(Ratio(bad, total),
                         "Cache lookups were expired: %.2f%%",
                         "");
  }

  // Cannot parse CSS.
  // TODO(sligocki): This counts per rewrite, it seems like it should count
  // per time CSS URL is seen in HTML.
  AddConsoleSuggestion(StatSumRatio(CssFilter::kParseFailures,
                                    CssFilter::kBlocksRewritten),
                       "CSS files not rewritten because of parse errors: "
                       "%.2f%%",
                       "");

  // Cannot parse JavaScript.
  AddConsoleSuggestion(
      StatSumRatio(JavascriptRewriteConfig::kMinificationFailures,
                   JavascriptRewriteConfig::kBlocksMinified),
      "JavaScript minification failures: %.2f%%",
      "");

  // Image reading failure.
  {
    double good =
        StatValue(ImageRewriteFilter::kImageRewrites) +
        // These are considered good because they were read and we could have
        // optimized them, the only reason we didn't was because they were
        // already optimal.
        StatValue(ImageRewriteFilter::kImageRewritesDroppedNoSavingResize) +
        StatValue(ImageRewriteFilter::kImageRewritesDroppedNoSavingNoResize);
    double bad =
        StatValue(ImageRewriteFilter::kImageNoRewritesHighResolution) +
        StatValue(ImageRewriteFilter::kImageRewritesDroppedDecodeFailure) +
        StatValue(ImageRewriteFilter::kImageRewritesDroppedServerWriteFail) +
        StatValue(ImageRewriteFilter::kImageRewritesDroppedMIMETypeUnknown) +
        StatValue(ImageRewriteFilter::kImageNoRewritesHighResolution);
    // TODO(sligocki): We don't seem to be tracking TimedVariables as
    // normal Variables in mod_pagespeed. Fix this.
    // + StatValue(ImageRewriteFilter::kImageRewritesDroppedDueToLoad);
    AddConsoleSuggestion(SumRatio(bad, good),
                         "Image rewrite failures: %.2f%%",
                         "");
  }

  // CSS not combinable.
  {
    double good = StatValue(CssCombineFilter::kCssFileCountReduction);
    double total = StatValue(CssCombineFilter::kCssCombineOpportunities);
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
