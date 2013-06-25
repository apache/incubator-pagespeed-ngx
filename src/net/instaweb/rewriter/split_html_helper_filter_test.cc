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
// Test code for SplitHtmlHelper filter.

#include "net/instaweb/rewriter/public/split_html_helper_filter.h"

#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

class SplitHtmlHelperFilterTest : public RewriteTestBase {
 public:
  SplitHtmlHelperFilterTest() {}
  virtual bool AddBody() const { return true; }

 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->EnableFilter(RewriteOptions::kSplitHtmlHelper);
    options()->set_critical_line_config("div[@id=\"b\"]");
    rewrite_driver()->AddFilters();
    rewrite_driver()->set_critical_images_info(new CriticalImagesInfo);
  }

  void CheckNumCriticalImages(int expected) {
    CriticalImagesInfo* critical_images_info =
        rewrite_driver()->critical_images_info();
    EXPECT_EQ(expected, critical_images_info->html_critical_images.size());
  }

  void CheckCriticalImage(GoogleString url) {
    CriticalImagesInfo* critical_images_info =
        rewrite_driver()->critical_images_info();
    EXPECT_NE(critical_images_info->html_critical_images.end(),
              critical_images_info->html_critical_images.find(url));
  }

  void CheckLoggingStatus(RewriterHtmlApplication::Status status) {
    rewrite_driver()->log_record()->WriteLog();
    for (int i = 0; i < logging_info()->rewriter_stats_size(); i++) {
      if (logging_info()->rewriter_stats(i).id() == "se" &&
          logging_info()->rewriter_stats(i).has_html_status()) {
        EXPECT_EQ(status, logging_info()->rewriter_stats(i).html_status());
        return;
      }
    }
    FAIL();
  }

  RequestHeaders request_headers_;
};

TEST_F(SplitHtmlHelperFilterTest, BasicTest) {
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kSplitHtmlHelper));
  ValidateNoChanges("split_helper_basic_test", "");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, DisabledTest1) {
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kSplitHtmlHelper));
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("");
  ValidateNoChanges("split_helper_disabled1", "");
  CheckLoggingStatus(RewriterHtmlApplication::DISABLED);
}

TEST_F(SplitHtmlHelperFilterTest, DisabledTest2) {
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kSplitHtmlHelper));
  rewrite_driver()->SetUserAgent("does_not_support_split");
  ValidateNoChanges("split_helper_disabled2", "");
  CheckLoggingStatus(RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequest) {
  rewrite_driver()->request_context()->set_is_split_btf_request(false);

  ValidateExpected(
      "split_helper_atf",
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>",
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=></div>"
      "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>");
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestWithCriticalImages) {
  rewrite_driver()->request_context()->set_is_split_btf_request(false);

  CriticalImagesInfo* critical_images_info = new CriticalImagesInfo;
  critical_images_info->html_critical_images.insert("http://test.com/1.jpeg");
  critical_images_info->html_critical_images.insert("http://test.com/4.jpeg");
  rewrite_driver()->set_critical_images_info(critical_images_info);
  CheckNumCriticalImages(2);

  ValidateExpected(
      "split_helper_atf",
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>",
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=></div>"
      "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>");
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, BtfRequest) {
  rewrite_driver()->request_context()->set_is_split_btf_request(true);

  CriticalImagesInfo* critical_images_info = new CriticalImagesInfo;
  critical_images_info->html_critical_images.insert("http://test.com/1.jpeg");
  rewrite_driver()->set_critical_images_info(critical_images_info);
  CheckNumCriticalImages(1);
  ValidateNoChanges(
      "split_helper_btf",
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>");
  CheckNumCriticalImages(0);
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestTwoXpaths) {
  rewrite_driver()->request_context()->set_is_split_btf_request(false);
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("div[@id=\"b\"]:div[@id=\"d\"]");

  ValidateExpected(
      "split_helper_atf_2xpaths",
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'>"
        "<div id='c'><img src='3.jpeg'></div>"
      "</div>"
      "<div id='d'><img src='4.jpeg'></div>",

      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=>"
        "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "</div>"
      "<div id='d'><img src='4.jpeg'></div>");
  CheckNumCriticalImages(2);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckCriticalImage("http://test.com/4.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestXPathWithChildCount) {
  rewrite_driver()->request_context()->set_is_split_btf_request(false);
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("div[2]:div[4]");

  ValidateExpected(
      "split_helper_atf_xpath_with_child_count",
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "<div id='d'><img src='4.jpeg'></div>",

      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=></div>"
      "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "<div id='d'><img src='4.jpeg'></div>");
  CheckNumCriticalImages(2);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckCriticalImage("http://test.com/4.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestNoDeferCases) {
  rewrite_driver()->request_context()->set_is_split_btf_request(false);
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("div[2]:div[4]");

  ValidateExpected(
      "split_helper_atf_nodefer_cases",
      "<div id='a'><img src='1.jpeg'></div>"
      "<script pagespeed_no_defer=\"\"></script>"
      "<div id='b'><img src='2.jpeg'>"
        "<script pagespeed_no_defer=\"\"></script>"
        "<div id='c' pagespeed_no_defer=\"\"></div>"
      "</div>"
      "<div id='d'><img src='3.jpeg'></div>"
      "<div id='e'><img src='4.jpeg'></div>",

      "<div id='a'><img src='1.jpeg'></div>"
      "<script pagespeed_no_defer=\"\"></script>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=>"
        "<script pagespeed_no_defer=\"\"></script>"
        "<div id='c' pagespeed_no_defer=\"\"></div>"
      "</div>"
      "<div id='d'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "<div id='e'><img src='4.jpeg'></div>");
  CheckNumCriticalImages(2);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckCriticalImage("http://test.com/4.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestNonCountedChildren) {
  rewrite_driver()->request_context()->set_is_split_btf_request(false);
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("div[2]");

  ValidateExpected(
      "split_helper_atf_nodefer_cases",
      "<div id='a'><img src='1.jpeg'></div>"
      "<link>"
      "<script></script>"
      "<noscript></noscript>"
      "<style></style>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>",

      "<div id='a'><img src='1.jpeg'></div>"
      "<link>"
      "<script></script>"
      "<noscript></noscript>"
      "<style></style>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=></div>"
      "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>");
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, BtfRequestConfigInHeader) {
  rewrite_driver()->request_context()->set_is_split_btf_request(true);
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("");
  request_headers_.Add(HttpAttributes::kXPsaSplitConfig, "div[2]");
  rewrite_driver()->SetRequestHeaders(request_headers_);

  ValidateNoChanges(
      "split_helper_btf_request_config_in_header",
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>");
  CheckNumCriticalImages(0);
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

}  // namespace net_instaweb
