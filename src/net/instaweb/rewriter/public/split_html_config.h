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
class Panel;
class RewriteDriver;

struct XpathUnit {
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

  const CriticalLineInfo* critical_line_info() {
    return critical_line_info_;
  }

  XpathMap* xpath_map() { return &xpath_map_; }
  PanelIdToSpecMap* panel_id_to_spec() { return &panel_id_to_spec_; }

 private:
  const CriticalLineInfo* critical_line_info_;  // Owned by rewrite_driver_.

  // Maps the string representation of the xpath to its parsed representation.
  XpathMap xpath_map_;

  // Maps the panel's id to its Panel specification protobuf.
  // TODO(bharathbhushan): Can we get rid of this?
  PanelIdToSpecMap panel_id_to_spec_;

  DISALLOW_COPY_AND_ASSIGN(SplitHtmlConfig);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_SPLIT_HTML_CONFIG_H_
