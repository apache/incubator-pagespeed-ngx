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
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
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

void CriticalCssBeaconFilter::Summarize(Stylesheet* stylesheet,
                                        GoogleString* out) const {
  StringSet selectors;
  FindSelectorsFromStylesheet(*stylesheet, &selectors);
  // Serialize set into out.
  AppendJoinCollection(out, selectors, ",");
}

// Return the initial portion of the beaconing Javascript.
// Just requires array of css selector strings plus closing ");"
// Right now the result looks like:
// ...static script from critical_css_beacon.js...
// pagespeed.criticalCssBeaconInit('beacon_url','page_url','options_hash',
// To which the caller then appends:
// ['selector 1','selector 2','selector 3']);
GoogleString CriticalCssBeaconFilter::BeaconBoilerplate() {
  const RewriteOptions::BeaconUrl& beacons = driver()->options()->beacon_url();
  const GoogleString& beacon_url =
      driver()->IsHttps() ? beacons.https : beacons.http;
  StaticAssetManager* static_asset_manager =
      driver()->server_context()->static_asset_manager();
  GoogleString script;
  const GoogleString& script_url = static_asset_manager->GetAssetUrl(
      StaticAssetManager::kCriticalCssBeaconJs, driver()->options());
  HtmlElement* external_script = driver()->NewElement(NULL, HtmlName::kScript);
  InjectSummaryData(external_script);
  driver()->AddAttribute(external_script, HtmlName::kSrc, script_url);
  StrAppend(&script, "pagespeed.criticalCssBeaconInit('", beacon_url, "','");
  EscapeToJsStringLiteral(driver()->google_url().Spec(),
                          false /* no quote */, &script);
  GoogleString options_signature_hash =
      driver()->server_context()->hasher()->Hash(
          driver()->options()->signature());
  StrAppend(&script, "','", options_signature_hash, "',");
  return script;
}

void CriticalCssBeaconFilter::SummariesDone() {
  // First check the property cache to see if we need to inject a beacon at all.
  // TODO(jmaessen): Why do we do this so late in the process?  Because
  // eventually we want to store a signature of the selectors the browser
  // checked as part of the beacon result, and this is the point where we'll be
  // able to check it against the set of selectors we would *like* the browser
  // to check.  If they're different we have to insert the beacon because the
  // CSS has changed.
  // TODO(jmaessen): This is also where we decide whether we want >1 set of
  // beacon results, a la what's done with critical images.  For now we bail
  // out if there are beacon results available to us.
  if (driver()->CriticalSelectors() != NULL) {
    return;
  }
  // We construct a transient set of StringPiece objects backed by the summary
  // information.  We parse each summary back into component selectors from its
  // comma-separated string, use the set to remove duplicates (they'll be
  // sorted, too, which makes this easier to test).  We re-serialize the set.
  set<StringPiece> selectors;
  for (int i = 0; i < NumStyles(); ++i) {
    const SummaryInfo& block_info = GetSummaryForStyle(i);
    if (block_info.state != kSummaryOk) {
      // Don't beacon unless all CSS was correctly parsed and summarized.
      return;
    }
    // The critical_selector_filter doesn't include <noscript>-specific CSS
    // in the critical CSS it computes; so there is no need to figure out
    // critical selectors for such CSS.
    if (block_info.is_inside_noscript) {
      continue;
    }
    StringPieceVector temp;
    SplitStringPieceToVector(block_info.data, ",", &temp,
                             false /* empty shouldn't happen */);
    selectors.insert(temp.begin(), temp.end());
  }
  if (selectors.empty()) {
    // Don't insert beacon if no parseable CSS was found (usually meaning there
    // wasn't any CSS).
    // TODO(jmaessen): Mark this case in the property cache.  We need to be a
    // bit careful here; we want to attempt to re-instrument if there were JS
    // resources that arrived too late for us to parse them and include them in
    // the beaconing.  We are eventually going to need to compute a selector
    // signature of some sort and use that to control when we re-instrument (and
    // when we consider the beacon results to match the CSS that we actually see
    // on the page).
    driver()->InfoHere("No CSS selectors found.");
    return;
  }
  // The set is assembled.  Insert the beaconing code.
  GoogleString script = BeaconBoilerplate();
  StrAppend(&script, "[");
  AppendJoinCollection(&script, selectors, ",");
  StrAppend(&script, "]);");
  HtmlElement* script_element = driver()->NewElement(NULL, HtmlName::kScript);
  InjectSummaryData(script_element);
  StaticAssetManager* static_asset_manager =
      driver()->server_context()->static_asset_manager();
  static_asset_manager->AddJsToElement(script, script_element, driver_);
  if (critical_css_beacon_added_count_ != NULL) {
    critical_css_beacon_added_count_->Add(1);
  }
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
      // We're cautious here and escape each selector, as they're culled from a
      // parse of site css data.
      GoogleString quoted_and_escaped;
      EscapeToJsStringLiteral(trimmed, true /* quote */, &quoted_and_escaped);
      selectors->insert(quoted_and_escaped);
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
