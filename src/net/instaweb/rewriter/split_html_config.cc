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
//
// Code for managing configuration related to the split html filter.
// Mainly the code here parses the xpath configuration and digests it into a
// form usable during response processing.

#include "net/instaweb/rewriter/public/split_html_config.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/critical_line_info_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/re2.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

// Regular expressions which are used to validate and parse the xpaths for the
// below-the-fold panels.
RE2* xpath_with_id_pattern_ = NULL;
RE2* xpath_with_child_pattern_ = NULL;

namespace {

void ComputePanels(
    const CriticalLineInfo& critical_line_info,
    PanelIdToSpecMap* panel_id_to_spec) {
  for (int i = 0; i < critical_line_info.panels_size(); ++i) {
    const Panel& panel = critical_line_info.panels(i);
    const GoogleString panel_id =
        StrCat(BlinkUtil::kPanelId, ".", IntegerToString(i));
    (*panel_id_to_spec)[panel_id] = &panel;
  }
}

bool ParseXpath(const GoogleString& xpath,
                std::vector<XpathUnit>* xpath_units) {
  StringPieceVector list;
  net_instaweb::SplitStringUsingSubstr(xpath, "/", &list);
  for (int j = 0, n = list.size(); j < n; ++j) {
    XpathUnit unit;
    GoogleString str;
    StringPiece match = list[j];
    if (!RE2::FullMatch(StringPieceToRe2(match), *xpath_with_child_pattern_,
                        &unit.tag_name, &str, &unit.child_number)) {
      GoogleString str1;
      RE2::FullMatch(StringPieceToRe2(match), *xpath_with_id_pattern_,
                     &unit.tag_name, &str, &str1, &unit.attribute_value);
    }
    xpath_units->push_back(unit);
  }
  return true;
}

void PopulateXpathMap(const GoogleString& xpath,
                      XpathMap* xpath_map) {
  if (xpath_map->find(xpath) == xpath_map->end()) {
    XpathUnits* xpath_units = new XpathUnits();
    ParseXpath(xpath, xpath_units);
    (*xpath_map)[xpath] = xpath_units;
  }
}

void PopulateXpathMap(
    const CriticalLineInfo& critical_line_info, XpathMap* xpath_map) {
  for (int i = 0; i < critical_line_info.panels_size(); ++i) {
    const Panel& panel = critical_line_info.panels(i);
    PopulateXpathMap(panel.start_xpath(), xpath_map);
    if (panel.has_end_marker_xpath()) {
      PopulateXpathMap(panel.end_marker_xpath(), xpath_map);
    }
  }
}

}  // namespace

SplitHtmlConfig::SplitHtmlConfig(RewriteDriver* driver) : driver_(driver) {
  critical_line_info_ = driver->server_context()->
      critical_line_info_finder()->GetCriticalLine(driver);
  if (critical_line_info_ != NULL) {
    ComputePanels(*critical_line_info_, &panel_id_to_spec_);
    PopulateXpathMap(*critical_line_info_, &xpath_map_);
  }
}

SplitHtmlConfig::~SplitHtmlConfig() {
  STLDeleteContainerPairSecondPointers(xpath_map_.begin(), xpath_map_.end());
}

void SplitHtmlConfig::Initialize() {
  RE2::Options re2_options;
  xpath_with_id_pattern_ =
      new RE2("(\\w+)(\\[@(\\w+)\\s*=\\s*\"(.*)\"\\])", re2_options);
  xpath_with_child_pattern_ = new RE2("(\\w+)(\\[(\\d+)\\])", re2_options);
}

void SplitHtmlConfig::Terminate() {
  delete xpath_with_id_pattern_;
  xpath_with_id_pattern_ = NULL;
  delete xpath_with_child_pattern_;
  xpath_with_child_pattern_ = NULL;
}

SplitHtmlState::SplitHtmlState(const SplitHtmlConfig* config) :
    config_(config),
    current_panel_parent_element_(NULL) {
}

SplitHtmlState::~SplitHtmlState() {
}

bool SplitHtmlState::IsElementSiblingOfCurrentPanel(
    HtmlElement* element) const {
  return current_panel_parent_element_ != NULL &&
      current_panel_parent_element_ == element->parent();
}

bool SplitHtmlState::IsElementParentOfCurrentPanel(
    HtmlElement* element) const {
  return current_panel_parent_element_ != NULL &&
      current_panel_parent_element_ == element;
}

bool SplitHtmlState::ElementMatchesXpath(
    const HtmlElement* element,
    const std::vector<XpathUnit>& xpath_units) const {
  int j = xpath_units.size() - 1, k = num_children_stack_.size() - 2;
  for (; j >= 0 && k >= 0; j--, k--, element = element->parent()) {
    if (element->name_str() !=  xpath_units[j].tag_name) {
      return false;
    }
    if (!xpath_units[j].attribute_value.empty()) {
      return (element->AttributeValue(HtmlName::kId) != NULL &&
          element->AttributeValue(HtmlName::kId) ==
              xpath_units[j].attribute_value);
    } else if (xpath_units[j].child_number == num_children_stack_[k]) {
      continue;
    } else {
      return false;
    }
  }

  if (j < 0 && k < 0) {
    return true;
  }
  return false;
}

GoogleString SplitHtmlState::MatchPanelIdForElement(
    HtmlElement* element) const {
  if (config_->critical_line_info() == NULL) {
    return "";
  }
  for (int i = 0; i < config_->critical_line_info()->panels_size(); i++) {
    const Panel& panel = config_->critical_line_info()->panels(i);
    XpathMap::const_iterator xpaths =
        config_->xpath_map()->find(panel.start_xpath());
    if (xpaths == config_->xpath_map()->end()) {
      continue;
    }
    if (ElementMatchesXpath(element, *xpaths->second)) {
      return StrCat(BlinkUtil::kPanelId, ".", IntegerToString(i));
    }
  }
  return "";
}

bool SplitHtmlState::IsEndMarkerForCurrentPanel(HtmlElement* element) const {
  if (current_panel_parent_element() == NULL) {
    return false;
  }
  PanelIdToSpecMap::const_iterator panel_it =
      config_->panel_id_to_spec()->find(current_panel_id_);
  if (panel_it == config_->panel_id_to_spec()->end()) {
    LOG(DFATAL) << "Invalid Panelid: " << current_panel_id_ << " for url "
                << config_->driver()->google_url().Spec();
    return false;
  }
  const Panel& panel = *panel_it->second;
  if (!panel.has_end_marker_xpath()) {
    return false;
  }
  XpathMap::const_iterator xpaths = config_->xpath_map()->find(
      panel.end_marker_xpath());
  if (xpaths == config_->xpath_map()->end()) {
    return false;
  }
  return ElementMatchesXpath(element, *xpaths->second);
}

void SplitHtmlState::UpdateNumChildrenStack(const HtmlElement* element) {
  if (!num_children_stack()->empty()) {
    // Ignore some of the non-rendered tags for numbering the children. This
    // helps avoid mismatches due to combine_javascript combining differently
    // and creating different numbers of script nodes in different rewrites.
    // This also helps when combine_css combines link tags or styles differently
    // in different rewrites.
    if (element->keyword() != HtmlName::kScript &&
        element->keyword() != HtmlName::kNoscript &&
        element->keyword() != HtmlName::kStyle &&
        element->keyword() != HtmlName::kLink) {
      num_children_stack()->back()++;;
    }
    num_children_stack()->push_back(0);
  } else if (element->keyword() == HtmlName::kBody) {
    // Start the stack only once body is encountered.
    num_children_stack()->push_back(0);
  }
}

}  // namespace net_instaweb
