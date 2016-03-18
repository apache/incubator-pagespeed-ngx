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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_CSS_BEACON_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_CSS_BEACON_FILTER_H_

#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/css_summarizer_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"
#include "pagespeed/kernel/http/semantic_type.h"

namespace Css {

class Ruleset;
class Stylesheet;

}  // namespace Css

namespace net_instaweb {

class Statistics;
class Variable;

// Figure out the set of CSS selectors referenced from a page, saving those
// selectors in an OutputResource for each CSS <style> or <link> on the page.
// Based on that set of candidate critical selectors, inject javascript for
// detecting critical above the fold css selectors after the page has loaded.
// Assumes CSS @imports have been flattened first.
class CriticalCssBeaconFilter : public CssSummarizerBase {
 public:
  static const char kInitializePageSpeedJs[];

  // Statistics:
  static const char kCriticalCssBeaconAddedCount[];
  static const char kCriticalCssNoBeaconDueToMissingData[];
  static const char kCriticalCssSkippedDueToCharset[];

  explicit CriticalCssBeaconFilter(RewriteDriver* driver);
  virtual ~CriticalCssBeaconFilter();

  static void InitStats(Statistics* statistics);

  virtual const char* Name() const { return "CriticalCssBeacon"; }
  virtual const char* id() const { return "cb"; }

  // This filter needs access to all critical selectors (even those from
  // unauthorized domains) in order to let the clients use them while
  // detecting critical selectors that can be subsequently beaconed back
  // to the server and eventually inlined into the HTML.
  virtual RewriteDriver::InlineAuthorizationPolicy AllowUnauthorizedDomain()
      const {
    return driver()->options()->HasInlineUnauthorizedResourceType(
               semantic_type::kStylesheet) ?
           RewriteDriver::kInlineUnauthorizedResources :
           RewriteDriver::kInlineOnlyAuthorizedResources;
  }

  // Selectors are inlined into javascript.
  virtual bool IntendedForInlining() const { return true; }
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 protected:
  virtual bool MustSummarize(HtmlElement* element) const;
  virtual void Summarize(Css::Stylesheet* stylesheet,
                         GoogleString* out) const;
  virtual void SummariesDone();

  virtual void DetermineEnabled(GoogleString* disabled_reason);

 private:
  static void FindSelectorsFromRuleset(const Css::Ruleset& ruleset,
                                       StringSet* selectors);
  // The following adds the selectors to the given StringSet.
  static void FindSelectorsFromStylesheet(const Css::Stylesheet& css,
                                          StringSet* selectors);

  // Append the selectors initialization JavaScript.
  void AppendSelectorsInitJs(GoogleString* script, const StringSet& selectors);

  // Append the beaconing initialization JavaScript.
  void AppendBeaconInitJs(const BeaconMetadata& metadata, GoogleString* script);

  // The total number of times the beacon is added to a page.
  Variable* critical_css_beacon_added_count_;
  // The number of times we abandon beacon insertion due to missing CSS data (it
  // was still being fetched / rewritten).
  Variable* critical_css_no_beacon_due_to_missing_data_;
  // The number of CSS files we ignore due to charset incompatibility.
  // Should these block critical CSS insertion?
  Variable* critical_css_skipped_due_to_charset_;

  DISALLOW_COPY_AND_ASSIGN(CriticalCssBeaconFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_CSS_BEACON_FILTER_H_
