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

// Author: bharathbhushan@google.com (Bharath Bhushan Kowshik Raghupathi)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_HELPER_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_HELPER_FILTER_H_

#include <vector>

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/json.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "pagespeed/kernel/base/json_writer.h"

namespace net_instaweb {

class HtmlElement;
class RewriteDriver;
class SplitHtmlConfig;
class XpathUnit;

// Filter which helps in the presence of split html filter. Based on the xpath
// configuration it will decide the above-the-fold panels and below-the-fold
// panels and makes sure that downstream filters like inline preview images and
// lazyload images can work well in the absence of critical image information.
//
// When the above-the-fold html fragment is requested it allows the images in
// those panels to be inline previewed. When the below-the-fold html fragment is
// requested it allows the images in those panels to be lazyloaded.
// TODO(bharathbhushan): No need to have a json value.
// TODO(bharathbhushan): Share the xpath matching code with SplitHtmlFilter.
class SplitHtmlHelperFilter : public CommonFilter {
 public:
  explicit SplitHtmlHelperFilter(RewriteDriver* rewrite_driver);
  virtual ~SplitHtmlHelperFilter();

  virtual void StartDocumentImpl();
  virtual void EndDocument();

  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);

  virtual const char* Name() const { return "SplitHtmlHelperFilter"; }

 private:
  // Returns the panel id of the panel whose xpath matched with element.
  GoogleString MatchPanelIdForElement(HtmlElement* element);

  // Returns true if element is the parent of current panel
  bool IsElementParentOfCurrentPanel(HtmlElement* element);

  // Pops the json from top of the stack and merges with parent panel which is
  // one below it.
  void EndPanelInstance();

  // Pushes new Json to the top of the stack corresponding to element.
  void StartPanelInstance(HtmlElement* element, const GoogleString& panelid);

  // Returns true if element matches with the end_marker for panel corresponding
  // to panel_id
  bool IsEndMarkerForCurrentPanel(HtmlElement* element);

  // Appends dict to the dictionary array
  void AppendJsonData(Json::Value* dictionary, const Json::Value& dict);

  bool ElementMatchesXpath(const HtmlElement* element,
                           const std::vector<XpathUnit>& xpath_units);

  scoped_ptr<SplitHtmlConfig> config_;
  std::vector<ElementJsonPair> element_json_stack_;
  std::vector<int> num_children_stack_;
  GoogleString current_panel_id_;
  bool disable_filter_;
  bool inside_pagespeed_no_defer_script_;
  HtmlElement* current_panel_parent_element_;
  ScriptTagScanner script_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(SplitHtmlHelperFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_HELPER_FILTER_H_
