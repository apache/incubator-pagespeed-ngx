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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "webutil/css/media.h"
#include "webutil/css/parser.h"
#include "webutil/css/selector.h"

using Css::Selectors;
using Css::Stylesheet;
using Css::Ruleset;
using Css::Rulesets;

namespace net_instaweb {

const char CriticalCssBeaconFilter::kInitializePageSpeedJs[] =
    "var pagespeed = pagespeed || {};";

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

bool CriticalCssBeaconFilter::MustSummarize(HtmlElement* element) const {
  // Don't summarize alternate stylesheets, they are clearly non-critical.
  if (element->keyword() == HtmlName::kLink &&
      CssTagScanner::IsAlternateStylesheet(
          element->AttributeValue(HtmlName::kRel))) {
    return false;
  }

  // Don't summarize non-screen-affecting or <noscript> CSS at all; the time we
  // spend doing that is better devoted to summarizing CSS selectors we will
  // actually consider critical.
  return (noscript_element() == NULL) &&
      css_util::CanMediaAffectScreen(element->AttributeValue(HtmlName::kMedia));
}

void CriticalCssBeaconFilter::Summarize(Stylesheet* stylesheet,
                                        GoogleString* out) const {
  StringSet selectors;
  FindSelectorsFromStylesheet(*stylesheet, &selectors);
  // Serialize set into out.
  AppendJoinCollection(out, selectors, ",");
}

// Append the selector list initialization JavaScript to |script|.
// Right now the result looks like:
//   pagespeed.selectors=["selector 1","selector 2","selector 3"];
void CriticalCssBeaconFilter::AppendSelectorsInitJs(
    GoogleString* script, const StringSet& selectors) {
  StrAppend(script, "pagespeed.selectors=[");
  for (StringSet::const_iterator i = selectors.begin();
       i != selectors.end(); ++i) {
    if (i != selectors.begin()) {
      StrAppend(script, ",");
    }
    EscapeToJsStringLiteral(*i, true /* quote */, script);
  }
  StrAppend(script, "];");
}

// Append the beacon initialization JavaScript to |script|.
// Right now the result looks like:
//   pagespeed.criticalCssBeaconInit('beacon_url','page_url','options_hash',
//        pagespeed.selectors);
void CriticalCssBeaconFilter::AppendBeaconInitJs(
    const BeaconMetadata& metadata, GoogleString* script) {
  GoogleString beacon_url = driver()->IsHttps() ?
      driver()->options()->beacon_url().https :
      driver()->options()->beacon_url().http;
  GoogleString page_url;
  EscapeToJsStringLiteral(driver()->google_url().Spec(), false /* add_quotes */,
                          &page_url);
  Hasher* hasher = driver()->server_context()->hasher();
  GoogleString options_hash = hasher->Hash(driver()->options()->signature());
  StrAppend(script,
            "pagespeed.criticalCssBeaconInit('",
            beacon_url, "','", page_url, "','",
            options_hash, "','", metadata.nonce, "',pagespeed.selectors);");
}

void CriticalCssBeaconFilter::SummariesDone() {
  // We parse each summary back into component selectors from its
  // comma-separated string, using a StringSet to remove duplicates (they'll be
  // sorted, too, which makes this easier to test).  We re-serialize the set.
  StringSet selectors;
  for (int i = 0; i < NumStyles(); ++i) {
    const SummaryInfo& summary_info = GetSummaryForStyle(i);
    // The critical_selector_filter doesn't include <noscript>-specific CSS
    // in the critical CSS it computes; so there is no need to figure out
    // critical selectors for such CSS.
    if (summary_info.is_inside_noscript) {
      continue;
    }
    switch (summary_info.state) {
      case kSummaryStillPending:
        // Don't beacon if we're still waiting for critical selector data.
        return;
      case kSummaryOk: {
        // Include the selectors in the beacon
        StringPieceVector temp;
        SplitStringPieceToVector(summary_info.data, ",", &temp,
                                 true /* omit_empty_strings */);
        for (StringPieceVector::const_iterator i = temp.begin(),
                 end = temp.end();
             i != end; ++i) {
          selectors.insert(i->as_string());
        }
        break;
      }
      case kSummarySlotRemoved:
        // Another filter (likely combine CSS) has eliminated this CSS.
        continue;
      case kSummaryCssParseError:
      case kSummaryResourceCreationFailed:
      case kSummaryInputUnavailable:
        // The CSS couldn't be fetched or parsed in some fashion.  This will
        // be left in place by the rewriter, so we don't need to consider it for
        // beaconing either.  NOTE: this requires the rewriter to inject
        // critical CSS in situ so that we don't disrupt the cascade order
        // around the unparseable data.
        // TODO(jmaessen): Consider handling unparseable data within the CSS
        // parse tree, which would let us extract critical CSS selectors from
        // CSS with a mix of parseable and unparseable rules.
        continue;
    }
  }
  BeaconMetadata metadata =
      driver()->server_context()->critical_selector_finder()->
          PrepareForBeaconInsertion(selectors, driver());
  if (metadata.status == kDoNotBeacon) {
    // No beaconing required according to current pcache state and computed
    // selector set.
    return;
  }

  // Insert the beaconing code and selectors.
  GoogleString script;
  StaticAssetManager* asset_manager =
      driver()->server_context()->static_asset_manager();
  if (driver()->server_context()->factory()->UseBeaconResultsInFilters()) {
    script = asset_manager->GetAsset(
        StaticAssetManager::kCriticalCssBeaconJs, driver()->options());
    AppendSelectorsInitJs(&script, selectors);
    AppendBeaconInitJs(metadata, &script);
  } else {
    script = kInitializePageSpeedJs;
    AppendSelectorsInitJs(&script, selectors);
  }
  HtmlElement* script_element = driver()->NewElement(NULL, HtmlName::kScript);
  driver_->AddAttribute(script_element, HtmlName::kPagespeedNoDefer, "");
  InsertNodeAtBodyEnd(script_element);
  asset_manager->AddJsToElement(script, script_element, driver_);

  if (critical_css_beacon_added_count_ != NULL) {
    critical_css_beacon_added_count_->Add(1);
  }
}

void CriticalCssBeaconFilter::DetermineEnabled() {
  // Currently CriticalSelectorFilter can't deal with IE conditional comments,
  // so we disable ourselves for IE.
  // Note: this should match the logic in CriticalSelectorFilter.
  set_is_enabled(!driver_->user_agent_matcher()->IsIe(driver_->user_agent()));
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
