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
// Test code for SplitHtmlConfig.

#include "net/instaweb/rewriter/public/split_html_config.h"

#include <memory>
#include <utility>

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/critical_line_info_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

class SplitHtmlConfigTest : public RewriteTestBase {
 public:
  SplitHtmlConfigTest() {}

  const CriticalLineInfo* critical_line_info() {
    return server_context()->critical_line_info_finder()
        ->GetCriticalLine(rewrite_driver());
  }

  const Panel& panels(int index) {
    return critical_line_info()->panels(index);
  }

  void MatchXpathUnits(SplitHtmlConfig* config,
                       const GoogleString& xpath,
                       const GoogleString& tag,
                       const GoogleString& attribute_value,
                       int child_number) {
    XpathMap::const_iterator it = config->xpath_map()->find(xpath);
    const XpathUnits& units = *it->second;
    EXPECT_EQ(1, units.size());
    EXPECT_EQ(tag, units[0].tag_name);
    EXPECT_EQ(attribute_value, units[0].attribute_value);
    EXPECT_EQ(child_number, units[0].child_number);
  }

  void MatchPanel(const Panel& panel,
                  const GoogleString& start_xpath,
                  const GoogleString& end_marker_xpath) {
    EXPECT_EQ(start_xpath, panel.start_xpath());
    EXPECT_EQ(end_marker_xpath, panel.end_marker_xpath());
  }

  void MatchPanelSpec(SplitHtmlConfig* config,
                      int panelid,
                      const GoogleString& start_xpath,
                      const GoogleString& end_marker_xpath) {
    GoogleString panelid_str = StringPrintf("panel-id.%d", panelid);
    PanelIdToSpecMap::const_iterator it =
        config->panel_id_to_spec()->find(panelid_str);
    const Panel& panel = *it->second;
    EXPECT_EQ(start_xpath, panel.start_xpath());
    EXPECT_EQ(end_marker_xpath, panel.end_marker_xpath());
  }

 protected:
  RequestHeaders request_headers_;
};

TEST_F(SplitHtmlConfigTest, BasicTest) {
  SplitHtmlConfig config(rewrite_driver());
  EXPECT_TRUE(critical_line_info() == NULL);
}

TEST_F(SplitHtmlConfigTest, OneXpath) {
  options()->set_critical_line_config("div[@id=\"b\"]");
  SplitHtmlConfig config(rewrite_driver());

  EXPECT_EQ(1, critical_line_info()->panels_size());
  EXPECT_EQ("div[@id=\"b\"]", panels(0).start_xpath());
  EXPECT_FALSE(panels(0).has_end_marker_xpath());
}

TEST_F(SplitHtmlConfigTest, OneXpathPair) {
  options()->set_critical_line_config("div[@id=\"b\"]:div[4]");
  SplitHtmlConfig config(rewrite_driver());

  EXPECT_EQ(1, critical_line_info()->panels_size());
  MatchPanel(panels(0), "div[@id=\"b\"]", "div[4]");

  EXPECT_EQ(2, config.xpath_map()->size());
  MatchXpathUnits(&config, "div[@id=\"b\"]", "div", "b", 0);
  MatchXpathUnits(&config, "div[4]", "div", "", 4);

  EXPECT_EQ(1, config.panel_id_to_spec()->size());
  MatchPanelSpec(&config, 0, "div[@id=\"b\"]", "div[4]");
}

TEST_F(SplitHtmlConfigTest, TwoXPaths) {
  options()->set_critical_line_config("div[1]:div[2],div[3]:div[4]");
  SplitHtmlConfig config(rewrite_driver());

  EXPECT_EQ(2, critical_line_info()->panels_size());
  MatchPanel(panels(0), "div[1]", "div[2]");
  MatchPanel(panels(1), "div[3]", "div[4]");

  EXPECT_EQ(4, config.xpath_map()->size());
  MatchXpathUnits(&config, "div[1]", "div", "", 1);
  MatchXpathUnits(&config, "div[2]", "div", "", 2);
  MatchXpathUnits(&config, "div[3]", "div", "", 3);
  MatchXpathUnits(&config, "div[4]", "div", "", 4);

  EXPECT_EQ(2, config.panel_id_to_spec()->size());
  MatchPanelSpec(&config, 0, "div[1]", "div[2]");
  MatchPanelSpec(&config, 1, "div[3]", "div[4]");
}

TEST_F(SplitHtmlConfigTest, ConfigInHeader) {
  request_headers_.Add(HttpAttributes::kXPsaSplitConfig,
                       "div[@id=\"b\"]:div[4]");
  rewrite_driver()->SetRequestHeaders(request_headers_);
  SplitHtmlConfig config(rewrite_driver());

  EXPECT_EQ(1, critical_line_info()->panels_size());
  MatchPanel(panels(0), "div[@id=\"b\"]", "div[4]");

  EXPECT_EQ(2, config.xpath_map()->size());
  MatchXpathUnits(&config, "div[@id=\"b\"]", "div", "b", 0);
  MatchXpathUnits(&config, "div[4]", "div", "", 4);

  EXPECT_EQ(1, config.panel_id_to_spec()->size());
  MatchPanelSpec(&config, 0, "div[@id=\"b\"]", "div[4]");
}

}  // namespace net_instaweb
