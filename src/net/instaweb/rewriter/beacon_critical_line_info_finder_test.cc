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

#include "net/instaweb/rewriter/public/beacon_critical_line_info_finder.h"

#include <map>

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

namespace {
const char kRequestUrl[] = "http://www.test.com";
}  // namespace

class BeaconCriticalLineInfoFinderTest : public RewriteTestBase {
 protected:
  BeaconCriticalLineInfoFinderTest() {}

  virtual ~BeaconCriticalLineInfoFinderTest() {}

  void ResetDriver() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    PropertyCache* pcache = server_context_->page_property_cache();
    MockPropertyPage* page = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page);
    pcache->set_enabled(true);
    pcache->Read(page);
  }

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    ResetDriver();
    // Setup the beacon_cohort. ServerContext will take ownership.
    const PropertyCache::Cohort* beacon_cohort =
        SetupCohort(page_property_cache(), RewriteDriver::kBeaconCohort);
    server_context()->set_beacon_cohort(beacon_cohort);
    // Create a new finder with the beacon_cohort. ServerContext will take
    // ownership.
    server_context()->set_critical_line_info_finder(
        new BeaconCriticalLineInfoFinder(beacon_cohort,
                                         factory()->nonce_generator()));
    // Setup the property page. ServerContext will take ownership.
    MockPropertyPage* page = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page);
    server_context()->page_property_cache()->Read(page);
  }

  void WriteXPathsWithBeacon(const StringSet& xpaths,
                             BeaconStatus expected_beacon_status) {
    BeaconMetadata metadata = server_context()
                                  ->critical_line_info_finder()
                                  ->PrepareForBeaconInsertion(rewrite_driver());
    ASSERT_EQ(expected_beacon_status, metadata.status);
    BeaconCriticalLineInfoFinder::WriteXPathsToPropertyCacheFromBeacon(
        xpaths, metadata.nonce, page_property_cache(),
        server_context()->beacon_cohort(), rewrite_driver()->property_page(),
        message_handler(), factory()->mock_timer());
    rewrite_driver()->property_page()->WriteCohort(
        server_context()->beacon_cohort());
    ResetDriver();
  }

  void VerifyCriticalLineInfo(const StringSet& xpath_set) {
    const CriticalLineInfo* info =
        server_context()->critical_line_info_finder()->GetCriticalLine(
            rewrite_driver());
    ASSERT_TRUE(info != NULL);
    ASSERT_EQ(xpath_set.size(), info->panels_size());
    for (int i = 0; i < xpath_set.size(); ++i) {
      Panel panel = info->panels(i);
      GoogleString panel_str = panel.start_xpath();
      if (!panel.end_marker_xpath().empty()) {
        panel_str += ":" + panel.end_marker_xpath();
      }
      EXPECT_TRUE(xpath_set.find(panel_str) != xpath_set.end());
    }
  }
};

TEST_F(BeaconCriticalLineInfoFinderTest, NoCriticalLines) {
  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  EXPECT_TRUE(rewrite_driver()->beacon_critical_line_info() == NULL);
  EXPECT_TRUE(server_context()->critical_line_info_finder()->GetCriticalLine(
                  rewrite_driver()) == NULL);
  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  EXPECT_TRUE(rewrite_driver()->beacon_critical_line_info() != NULL);
}

TEST_F(BeaconCriticalLineInfoFinderTest, CriticalLinesFromBeacon) {
  StringSet xpaths;
  xpaths.insert("div[1]:div[2]");
  xpaths.insert("div[4]");
  WriteXPathsWithBeacon(xpaths, kBeaconWithNonce);

  VerifyCriticalLineInfo(xpaths);
  EXPECT_TRUE(rewrite_driver()->critical_line_info() != NULL);
  EXPECT_TRUE(rewrite_driver()->beacon_critical_line_info() != NULL);
}

TEST_F(BeaconCriticalLineInfoFinderTest, CriticalLinesFromConfig) {
  // Verify that if a manual split_html config is set, it still gets used
  // instead of any beacon data.
  GoogleString config = "div[@id='a']:div[1]";
  options()->set_critical_line_config(config);
  StringSet config_xpaths, beacon_xpaths;
  config_xpaths.insert(config);
  beacon_xpaths.insert("div[2]:div[3]");
  WriteXPathsWithBeacon(beacon_xpaths, kDoNotBeacon);
  VerifyCriticalLineInfo(config_xpaths);
}

}  // namespace net_instaweb
