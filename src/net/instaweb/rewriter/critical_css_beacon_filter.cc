/*
 * Copyright 2013 Google Inc.
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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/rewriter/public/critical_css_beacon_filter.h"

#include <set>
#include <vector>

#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "webutil/css/media.h"
#include "webutil/css/parser.h"
#include "webutil/css/selector.h"

using Css::Parser;
using Css::Selector;
using Css::Selectors;
using Css::Stylesheet;
using Css::Ruleset;
using Css::Rulesets;

namespace net_instaweb {

// Counters.
const char CriticalCssBeaconFilter::kCriticalCssBeaconAddedCount[] =
    "critical_css_beacon_filter_script_added_count";
const char CriticalCssBeaconFilter::kCriticalCssNoBeaconDueToMissingData[] =
    "critical_css_no_beacon_due_to_missing_data";
const char CriticalCssBeaconFilter::kCriticalCssSkippedDueToCharset[] =
    "critical_css_skipped_due_to_charset";

CriticalCssBeaconFilter::CriticalCssBeaconFilter(RewriteDriver* driver)
    : CssSummarizerBase(driver) {
  Statistics* stats = driver->server_context()->statistics();
  critical_css_beacon_added_count_ = stats->GetVariable(
      kCriticalCssBeaconAddedCount);
  critical_css_no_beacon_due_to_missing_data_ = stats->GetVariable(
      kCriticalCssNoBeaconDueToMissingData);
  critical_css_skipped_due_to_charset_ = stats->GetVariable(
      kCriticalCssSkippedDueToCharset);
}

CriticalCssBeaconFilter::~CriticalCssBeaconFilter() {}

void CriticalCssBeaconFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCriticalCssBeaconAddedCount);
  statistics->AddVariable(kCriticalCssNoBeaconDueToMissingData);
  statistics->AddVariable(kCriticalCssSkippedDueToCharset);
}

void CriticalCssBeaconFilter::Summarize(const Css::Stylesheet& stylesheet,
                                        GoogleString* out) const {
  StringSet selectors;
  FindSelectorsFromStylesheet(stylesheet, &selectors);
  // Serialize set into out.
  AppendJoinCollection(out, selectors, ",");
}

void CriticalCssBeaconFilter::SummariesDone() {
  // We construct a transient set of StringPiece objects backed by the summary
  // information.  We parse each summary back into component selectors from its
  // comma-separated string, use the set to remove duplicates (they'll be
  // sorted, too, which makes this easier to test).  We re-serialize the set.
  set<StringPiece> selectors;
  for (int i = 0; i < NumStyles(); ++i) {
    const GoogleString* one_block = GetSummary(i);
    if (one_block == NULL) {
      // Skip entries that weren't fetched.
      continue;
    }
    StringPieceVector temp;
    SplitStringPieceToVector(*one_block, ",", &temp,
                             false /* empty shouldn't happen */);
    selectors.insert(temp.begin(), temp.end());
  }
  GoogleString comment(" ");
  AppendJoinCollection(&comment, selectors, ", ");
  comment.append(" ");
  InjectSummaryData(driver()->NewCommentNode(NULL, comment));
}

void CriticalCssBeaconFilter::FindSelectorsFromRuleset(
    const Ruleset& ruleset, StringSet* selectors) {
  const Selectors& rule_selectors = ruleset.selectors();
  for (int i = 0, n = rule_selectors.size(); i < n; ++i) {
    GoogleString trimmed(css_util::JsDetectableSelector(*rule_selectors[i]));
    if (!trimmed.empty()) {
      // Non-empty trimmed selector.  An empty trimmed selector (eg :hover,
      // which gets stripped away as it's not JS detectable) is *automatically*
      // critical, and we could also ignore the selector * (:hover is implicitly
      // *:hover).
      selectors->insert(trimmed);
    }
  }
}

// Returns false on parse failure, else records css selectors (in normalized
// string form) in selectors.  The selectors will be sorted and unique.  Logging
// of failures etc. should be done in the caller.
void CriticalCssBeaconFilter::FindSelectorsFromStylesheet(
    const Stylesheet& css, StringSet* selectors) {
  const Rulesets& rulesets = css.rulesets();
  for (int i = 0, n = rulesets.size(); i < n; ++i) {
    Ruleset* ruleset = rulesets[i];
    if (ruleset->type() == Ruleset::UNPARSED_REGION) {
      // Couldn't parse this as a rule.
      continue;
    }
    // Skip rules that can't apply to the screen.
    if (!css_util::CanMediaAffectScreen(ruleset->media_queries().ToString())) {
      continue;
    }
    // Record the selectors associated with this ruleset.
    FindSelectorsFromRuleset(*ruleset, selectors);
  }
}

}  // namespace net_instaweb
