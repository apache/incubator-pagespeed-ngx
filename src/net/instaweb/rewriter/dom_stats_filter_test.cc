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

// Author: bharathbhushan@google.com (Bharath Bhushan)

#include "net/instaweb/rewriter/public/dom_stats_filter.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/rewriter/public/mock_critical_images_finder.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/string.h"
#include "testing/base/public/gunit.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class DomStatsFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kComputeStatistics);
    RewriteTestBase::SetUp();
    rewrite_driver()->AddFilters();
    filter_ = rewrite_driver()->dom_stats_filter();
  }

  DomStatsFilter* filter_;
};

TEST_F(DomStatsFilterTest, ImgTest) {
  const GoogleString input_html =
      "<html><body><img><img src='a'>"
      "<noscript><img><img src='b'></noscript></body></html>";
  ValidateNoChanges("img_tags", input_html);
  EXPECT_EQ(4, filter_->num_img_tags());
}

TEST_F(DomStatsFilterTest, InlinedImgTest) {
  const GoogleString input_html =
      "<html><body><img src='data:abc'></body></html>";
  ValidateNoChanges("inlined_img", input_html);
  EXPECT_EQ(1, filter_->num_inlined_img_tags());
}

TEST_F(DomStatsFilterTest, ExternalCssTest) {
  const GoogleString input_html =
      "<html><body><link rel=stylesheet href='abc'>"
      "<link rel='alternate stylesheet' href='def'>"
      "<link rel='stylesheet'><link rel='junk' href='ghi'>"
      "</body></html>";
  ValidateNoChanges("external_css", input_html);
  EXPECT_EQ(2, filter_->num_external_css());
}

TEST_F(DomStatsFilterTest, NumScriptsTest) {
  const GoogleString input_html =
      "<html><body><script src='abc'></script>"
      "<script></script></body></html>";
  ValidateNoChanges("num_scripts", input_html);
  EXPECT_EQ(2, filter_->num_scripts());
}

TEST_F(DomStatsFilterTest, CriticalImagesUsedTest) {
  const GoogleString input_html =
      "<html><body><img src='a'><img src='a'><img src='b'></body></html>";
  MockCriticalImagesFinder* finder =
      new MockCriticalImagesFinder(statistics());
  server_context()->set_critical_images_finder(finder);
  StringSet* critical_images = new StringSet;
  critical_images->insert(StrCat(kTestDomain, "a"));
  critical_images->insert(StrCat(kTestDomain, "c"));
  critical_images->insert(StrCat(kTestDomain, "d"));
  finder->set_critical_images(critical_images);
  ValidateNoChanges("critical_images_used", input_html);
  // Image 'a' is the only critical image used and it is used twice.
  EXPECT_EQ(2, filter_->num_critical_images_used());
}

}  // namespace net_instaweb
