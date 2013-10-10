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

// Author: jud@google.com (Jud Porter)
//
// This filter injects instrumentation JS into the page to determine the
// below-the-fold xpaths used for the split_html filter.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_BEACON_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_BEACON_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class Statistics;
class Variable;

// Inject JavaScript for detecting the below-the-fold HTML panels.
class SplitHtmlBeaconFilter : public CommonFilter {
 public:
  // Counters.
  static const char kSplitHtmlBeaconAddedCount[];

  explicit SplitHtmlBeaconFilter(RewriteDriver* driver);
  virtual ~SplitHtmlBeaconFilter() {}

  virtual void DetermineEnabled();

  static void InitStats(Statistics* statistics);

  virtual void StartDocumentImpl() {}
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {}
  virtual const char* Name() const { return "SplitHtmlBeacon"; }

  // Returns true if this filter is going to inject a beacon for this request.
  // Filters that need to be disabled when beaconing runs (such as SplitHtml)
  // should set_is_enabled(false) in their DetermineEnabled calls if this
  // returns true.
  static bool ShouldApply(RewriteDriver* driver);

 private:
  Variable* split_html_beacon_added_count_;

  DISALLOW_COPY_AND_ASSIGN(SplitHtmlBeaconFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_BEACON_FILTER_H_
