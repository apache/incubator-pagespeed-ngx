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

#include "net/instaweb/rewriter/public/css_summarizer_base.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace Css {

class Ruleset;
class Stylesheet;

}  // namespace Css

namespace net_instaweb {

class RewriteDriver;
class Statistics;
class Variable;

// Figure out the set of CSS selectors referenced from a page, saving those
// selectors in an OutputResource for each CSS <style> or <link> on the page.
// Based on that set of candidate critical selectors, inject javascript for
// detecting critical above the fold css selectors after the page has loaded.
// Assumes CSS @imports have been flattened first.
class CriticalCssBeaconFilter : public CssSummarizerBase {
 public:
  // Statistics:
  static const char kCriticalCssBeaconAddedCount[];
  static const char kCriticalCssNoBeaconDueToMissingData[];
  static const char kCriticalCssSkippedDueToCharset[];

  explicit CriticalCssBeaconFilter(RewriteDriver* driver);
  virtual ~CriticalCssBeaconFilter();

  static void InitStats(Statistics* statistics);

  virtual const char* Name() const { return "CriticalCssBeacon"; }
  virtual const char* id() const { return "cb"; }

 protected:
  virtual void Summarize(Css::Stylesheet* stylesheet,
                         GoogleString* out) const;
  virtual void SummariesDone();

 private:
  static void FindSelectorsFromRuleset(const Css::Ruleset& ruleset,
                                       StringSet* selectors);
  // The following adds the selectors to the given StringSet.
  static void FindSelectorsFromStylesheet(const Css::Stylesheet& css,
                                          StringSet* selectors);

  // Return the initial portion of the beaconing Javascript.  Just requires
  // appending the array of css selector strings plus a closing ");".
  GoogleString BeaconBoilerplate();

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
