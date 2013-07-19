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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_CONFIG_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_CONFIG_H_

#include <map>
#include <vector>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class CriticalLineInfo;
class HtmlElement;
class Panel;
class RewriteDriver;

struct XpathUnit {
  XpathUnit() : child_number(0) {}

  GoogleString tag_name;
  GoogleString attribute_value;
  int child_number;
};

typedef std::map<GoogleString, const Panel*> PanelIdToSpecMap;
typedef std::vector<XpathUnit> XpathUnits;
// Map of xpath to XpathUnits.
typedef std::map<GoogleString, XpathUnits*> XpathMap;

// Deals with configuration related to the split html rewriter. Handles parsing
// and lookup of the configuration.
class SplitHtmlConfig {
 public:
  // Process the critical line info in the options and populate it in the
  // driver. Populate the panelid-to-specification map based on the critical
  // line information in the driver.
  explicit SplitHtmlConfig(RewriteDriver* driver);
  ~SplitHtmlConfig();

  // Initialize & Terminate must be paired.
  static void Initialize();
  static void Terminate();

  const CriticalLineInfo* critical_line_info() const {
    return critical_line_info_;
  }

  const XpathMap* xpath_map() const {
    return &xpath_map_;
  }

  const PanelIdToSpecMap* panel_id_to_spec() const {
    return &panel_id_to_spec_;
  }

  const RewriteDriver* driver() const { return driver_; }

 private:
  // Not owned by this class.
  RewriteDriver* driver_;

  const CriticalLineInfo* critical_line_info_;  // Owned by rewrite_driver_.

  // Maps the string representation of the xpath to its parsed representation.
  XpathMap xpath_map_;

  // Maps the panel's id to its Panel specification protobuf.
  PanelIdToSpecMap panel_id_to_spec_;

  DISALLOW_COPY_AND_ASSIGN(SplitHtmlConfig);
};

// Represents the filter state necessary to perform the split.
class SplitHtmlState {
 public:
  explicit SplitHtmlState(const SplitHtmlConfig* config);
  ~SplitHtmlState();

  std::vector<int>* num_children_stack() { return &num_children_stack_; }

  bool ElementMatchesXpath(
      const HtmlElement* element,
      const std::vector<XpathUnit>& xpath_units) const;

  // Returns the panel id of the panel whose xpath matched with element.
  GoogleString MatchPanelIdForElement(HtmlElement* element) const;

  // Returns true if element is sibling of the current start element on top of
  // stack.
  bool IsElementSiblingOfCurrentPanel(HtmlElement* element) const;

  // Returns true if element is the parent of current panel
  bool IsElementParentOfCurrentPanel(HtmlElement* element) const;

  // Returns true if element matches with the end_marker for panel corresponding
  // to panel_id
  bool IsEndMarkerForCurrentPanel(HtmlElement* element) const;

  const HtmlElement* current_panel_parent_element() const {
    return current_panel_parent_element_;
  }

  void set_current_panel_parent_element(HtmlElement* element) {
    current_panel_parent_element_ = element;
  }

  const GoogleString& current_panel_id() const {
    return current_panel_id_;
  }

  void set_current_panel_id(const GoogleString& panel_id) {
    current_panel_id_ = panel_id;
  }

  void UpdateNumChildrenStack(const HtmlElement* element);

 private:
  // Not owned by this class.
  const SplitHtmlConfig* config_;

  // Number of children for each element on the element stack.
  std::vector<int> num_children_stack_;

  // Not owned by this class.
  HtmlElement* current_panel_parent_element_;

  GoogleString current_panel_id_;

  DISALLOW_COPY_AND_ASSIGN(SplitHtmlState);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_CONFIG_H_
