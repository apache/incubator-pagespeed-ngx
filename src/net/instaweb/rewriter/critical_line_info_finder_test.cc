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

#include "net/instaweb/rewriter/public/critical_line_info_finder.h"

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/string.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

class CriticalLineInfoFinderTest : public RewriteTestBase {
 protected:
  GoogleString panel_start(int index) {
    return rewrite_driver()->critical_line_info()->panels(index).start_xpath();
  }

  GoogleString panel_end(int index) {
    return rewrite_driver()->critical_line_info()
        ->panels(index).end_marker_xpath();
  }

  CriticalLineInfo CreateCriticalLineInfo(
      const GoogleString& start_xpath1,
      const GoogleString& end_xpath1,
      const GoogleString& start_xpath2,
      const GoogleString& end_xpath2) {
    CriticalLineInfo config;
    Panel* panel = config.add_panels();
    panel->set_start_xpath(start_xpath1);
    panel->set_end_marker_xpath(end_xpath1);
    panel = config.add_panels();
    panel->set_start_xpath(start_xpath2);
    panel->set_end_marker_xpath(end_xpath2);
    return config;
  }

  void WriteToPcache(CriticalLineInfo config) {
    server_context()->page_property_cache()->set_enabled(true);

    const PropertyCache::Cohort* cohort = finder_->cohort();
    rewrite_driver()->set_property_page(NewMockPage("http://www.test.com"));
    server_context()->page_property_cache()->Read(
        rewrite_driver()->property_page());

    GoogleString buf;
    StringOutputStream sstream(&buf);
    ASSERT_TRUE(config.SerializeToZeroCopyStream(&sstream));
    PropertyPage* page = rewrite_driver()->property_page();
    ASSERT_TRUE(page != NULL);
    ASSERT_EQ("beacon_cohort", cohort->name());
    page->GetProperty(cohort,
                      CriticalLineInfoFinder::kCriticalLineInfoPropertyName);
    page->UpdateValue(
        cohort, CriticalLineInfoFinder::kCriticalLineInfoPropertyName, buf);
    rewrite_driver()->property_page()->WriteCohort(cohort);
  }

 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    const PropertyCache::Cohort* beacon_cohort =
        SetupCohort(page_property_cache(), RewriteDriver::kBeaconCohort);
    server_context()->set_beacon_cohort(beacon_cohort);
    finder_ = new CriticalLineInfoFinder(beacon_cohort);
    server_context()->set_critical_line_info_finder(finder_);
  }

  CriticalLineInfoFinder* finder_;
  RequestHeaders request_headers_;
};

TEST_F(CriticalLineInfoFinderTest, BasicTest) {
  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  server_context()->critical_line_info_finder()
      ->GetCriticalLine(rewrite_driver());
  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
}

TEST_F(CriticalLineInfoFinderTest, ConfigInHeader) {
  request_headers_.Add(HttpAttributes::kXPsaSplitConfig,
                       "div[@id='b']:div[4]");
  rewrite_driver()->SetRequestHeaders(request_headers_);

  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  server_context()->critical_line_info_finder()
      ->GetCriticalLine(rewrite_driver());
  EXPECT_EQ(1, rewrite_driver()->critical_line_info()->panels_size());
  EXPECT_EQ("div[@id='b']", panel_start(0));
  EXPECT_EQ("div[4]", panel_end(0));
}

TEST_F(CriticalLineInfoFinderTest, ConfigInDomainOptions) {
  options()->set_critical_line_config("div[@id='b']:div[4]");

  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  server_context()->critical_line_info_finder()
      ->GetCriticalLine(rewrite_driver());
  EXPECT_EQ(1, rewrite_driver()->critical_line_info()->panels_size());
  EXPECT_EQ("div[@id='b']", panel_start(0));
  EXPECT_EQ("div[4]", panel_end(0));
}

TEST_F(CriticalLineInfoFinderTest, ConfigError) {
  options()->set_critical_line_config("div[1]:div[2]:div[3]");

  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  server_context()->critical_line_info_finder()
      ->GetCriticalLine(rewrite_driver());
  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
}

TEST_F(CriticalLineInfoFinderTest, MultipleXpathPairs) {
  options()->set_critical_line_config("div[1]:div[2],div[3]:div[4],div[5]");

  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  server_context()->critical_line_info_finder()
      ->GetCriticalLine(rewrite_driver());
  EXPECT_EQ(3, rewrite_driver()->critical_line_info()->panels_size());
  EXPECT_EQ("div[1]", panel_start(0));
  EXPECT_EQ("div[2]", panel_end(0));
  EXPECT_EQ("div[3]", panel_start(1));
  EXPECT_EQ("div[4]", panel_end(1));
  EXPECT_EQ("div[5]", panel_start(2));
  EXPECT_EQ("", panel_end(2));
}

TEST_F(CriticalLineInfoFinderTest, ConfigInPcache) {
  CriticalLineInfo config = CreateCriticalLineInfo(
      "div[1]", "div[2]", "div[3]", "");
  WriteToPcache(config);

  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  server_context()->critical_line_info_finder()
      ->GetCriticalLine(rewrite_driver());
  EXPECT_EQ(2, rewrite_driver()->critical_line_info()->panels_size());
  EXPECT_EQ("div[1]", panel_start(0));
  EXPECT_EQ("div[2]", panel_end(0));
  EXPECT_EQ("div[3]", panel_start(1));
  EXPECT_EQ("", panel_end(1));
}

// Pcache config is preferred over domain config.
TEST_F(CriticalLineInfoFinderTest, ConfigInMultipleSources1) {
  CriticalLineInfo config = CreateCriticalLineInfo(
      "div[1]", "div[2]", "div[3]", "");
  WriteToPcache(config);
  options()->set_critical_line_config("div[10]:div[11]");

  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  server_context()->critical_line_info_finder()
      ->GetCriticalLine(rewrite_driver());
  EXPECT_EQ(2, rewrite_driver()->critical_line_info()->panels_size());
  EXPECT_EQ("div[1]", panel_start(0));
  EXPECT_EQ("div[2]", panel_end(0));
  EXPECT_EQ("div[3]", panel_start(1));
  EXPECT_EQ("", panel_end(1));
}

// HTTP header is preferred over domain config.
TEST_F(CriticalLineInfoFinderTest, ConfigInMultipleSources2) {
  request_headers_.Add(HttpAttributes::kXPsaSplitConfig,
                       "div[1]:div[2]");
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options()->set_critical_line_config("div[10]:div[11]");

  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  server_context()->critical_line_info_finder()
      ->GetCriticalLine(rewrite_driver());
  EXPECT_EQ(1, rewrite_driver()->critical_line_info()->panels_size());
  EXPECT_EQ("div[1]", panel_start(0));
  EXPECT_EQ("div[2]", panel_end(0));
}

// HTTP header is preferred over Pcache.
TEST_F(CriticalLineInfoFinderTest, ConfigInMultipleSources3) {
  request_headers_.Add(HttpAttributes::kXPsaSplitConfig,
                       "div[1]:div[2]");
  rewrite_driver()->SetRequestHeaders(request_headers_);
  CriticalLineInfo config = CreateCriticalLineInfo(
      "div[10]", "div[11]", "div[12]", "");
  WriteToPcache(config);

  EXPECT_TRUE(rewrite_driver()->critical_line_info() == NULL);
  server_context()->critical_line_info_finder()
      ->GetCriticalLine(rewrite_driver());
  EXPECT_EQ(1, rewrite_driver()->critical_line_info()->panels_size());
  EXPECT_EQ("div[1]", panel_start(0));
  EXPECT_EQ("div[2]", panel_end(0));
}

}  // namespace net_instaweb
