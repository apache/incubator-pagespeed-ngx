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
#include <vector>

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/re2.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

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
  // TODO(bharathbhushan): Can we compile and reuse the REs?
  static const char* kXpathWithChildNumber = "(\\w+)(\\[(\\d+)\\])";
  static const char* kXpathWithId = "(\\w+)(\\[@(\\w+)\\s*=\\s*\"(.*)\"\\])";
  StringPieceVector list;
  net_instaweb::SplitStringUsingSubstr(xpath, "/", &list);
  for (int j = 0, n = list.size(); j < n; ++j) {
    XpathUnit unit;
    GoogleString str;
    StringPiece match = list[j];
    if (!RE2::FullMatch(StringPieceToRe2(match), kXpathWithChildNumber,
                        &unit.tag_name, &str, &unit.child_number)) {
      GoogleString str1;
      RE2::FullMatch(StringPieceToRe2(match), kXpathWithId, &unit.tag_name,
                     &str, &str1, &unit.attribute_value);
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

SplitHtmlConfig::SplitHtmlConfig(RewriteDriver* driver) {
  GoogleString critical_line_config;
  const RequestHeaders* request_headers = driver->request_headers();
  if (request_headers != NULL) {
    const char* header = request_headers->Lookup1(
        HttpAttributes::kXPsaSplitConfig);
    if (header != NULL) {
      critical_line_config = header;
    }
  }
  if (critical_line_config.empty()) {
    critical_line_config = driver->options()->critical_line_config();
  }
  if (!critical_line_config.empty()) {
    CriticalLineInfo* critical_line_info = new CriticalLineInfo;
    StringPieceVector xpaths;
    SplitStringPieceToVector(critical_line_config, ",",
                             &xpaths, true);
    for (int i = 0, n = xpaths.size(); i < n; i++) {
      StringPieceVector xpath_pair;
      SplitStringPieceToVector(xpaths[i], ":", &xpath_pair, true);
      Panel* panel = critical_line_info->add_panels();
      panel->set_start_xpath(xpath_pair[0].data(), xpath_pair[0].length());
      if (xpath_pair.size() == 2) {
        panel->set_end_marker_xpath(
            xpath_pair[1].data(), xpath_pair[1].length());
      }
    }
    driver->set_critical_line_info(critical_line_info);
  }
  critical_line_info_ = driver->critical_line_info();
  if (critical_line_info_ != NULL) {
    ComputePanels(*critical_line_info_, &panel_id_to_spec_);
    PopulateXpathMap(*critical_line_info_, &xpath_map_);
  }
}

SplitHtmlConfig::~SplitHtmlConfig() {
  STLDeleteContainerPairSecondPointers(xpath_map_.begin(), xpath_map_.end());
}

}  // namespace net_instaweb
