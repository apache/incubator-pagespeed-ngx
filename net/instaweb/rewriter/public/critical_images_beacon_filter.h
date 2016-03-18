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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_BEACON_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_BEACON_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"

namespace net_instaweb {

// Inject javascript for detecting above the fold images after the page has
// loaded. Also adds data-pagespeed-url-hash attributes that the beacon sends
// back to the server. This allows the beacon to work despite image URL
// rewriting or inlining.
class CriticalImagesBeaconFilter : public CommonFilter {
 public:
  static const char* kImageOnloadCode;
  // Counters.
  static const char kCriticalImagesBeaconAddedCount[];

  explicit CriticalImagesBeaconFilter(RewriteDriver* driver);
  virtual ~CriticalImagesBeaconFilter();

  virtual void DetermineEnabled(GoogleString* disabled_reason);

  static void InitStats(Statistics* statistics);

  virtual void StartDocumentImpl() { }
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element) { }
  virtual void EndElementImpl(HtmlElement* element);
  virtual const char* Name() const { return "CriticalImagesBeacon"; }
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

  // Returns true if this filter is going to inject a beacon. Filters that need
  // to disabled when beaconing run, like the rendered_image_dimensions filter,
  // can check this function.
  static bool ShouldApply(RewriteDriver* rewrite_driver);

 private:
  // Clear all state associated with filter.
  void Clear();

  // Adds the beaconing javascript just before the current element if it has
  // not already been added and insert_beacon_js_ is true.
  void MaybeAddBeaconJavascript(HtmlElement* element);

  BeaconMetadata beacon_metadata_;
  StringSet image_url_hashes_;
  bool insert_beacon_js_;
  // The total number of times the beacon is added.
  Variable* critical_images_beacon_added_count_;

  bool added_beacon_js_;

  DISALLOW_COPY_AND_ASSIGN(CriticalImagesBeaconFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CRITICAL_IMAGES_BEACON_FILTER_H_
