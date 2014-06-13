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

#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_images_finder_test_base.h"
#include "net/instaweb/rewriter/public/delay_images_filter.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/wildcard.h"  // for Wildcard
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

namespace {
const char kSampleJpegData[] = "data:image/jpeg;base64*";
}  // namespace

class SplitHtmlHelperFilterTest : public RewriteTestBase {
 public:
  SplitHtmlHelperFilterTest() {}

 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    rewrite_driver()->SetUserAgent(
        UserAgentMatcherTestBase::kChrome18UserAgent);
    SetHtmlMimetype();  // Prevent insertion of CDATA tags to static JS.
  }

  void Init() {
    options()->EnableFilter(RewriteOptions::kSplitHtmlHelper);
    options()->set_critical_line_config("div[@id=\"b\"],"
                                        "div[@id=\"c\"]");
    rewrite_driver()->AddFilters();
    rewrite_driver()->set_critical_images_info(new CriticalImagesInfo);
  }

  void CheckNumCriticalImages(int expected) {
    CriticalImagesInfo* critical_images_info =
        rewrite_driver()->critical_images_info();
    EXPECT_EQ(expected, critical_images_info->html_critical_images.size());
  }

  void CheckCriticalImage(StringPiece url) {
    CriticalImagesFinder* finder =
        rewrite_driver()->server_context()->critical_images_finder();
    EXPECT_TRUE(finder->IsHtmlCriticalImage(url, rewrite_driver()));
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

  void SetBtfRequest() {
    rewrite_driver()->request_context()->set_split_request_type(
        RequestContext::SPLIT_BELOW_THE_FOLD);
  }

  void SetAtfRequest() {
    rewrite_driver()->request_context()->set_split_request_type(
        RequestContext::SPLIT_ABOVE_THE_FOLD);
  }

  GoogleString GetLazyloadImageTag(const GoogleString& url, bool no_transform) {
    return StrCat("<img pagespeed_lazy_src='", url, "'",
        no_transform ? " pagespeed_no_transform=" : "",
        " src=\"/psajs/1.0.gif\" "
        "onload=\"", LazyloadImagesFilter::kImageOnloadCode, "\">");
  }

  GoogleString GetInlinePreviewImageTag(const GoogleString& url,
                                        const GoogleString& low_res_src) {
    return StrCat(
        "<img pagespeed_high_res_src=\"", url, "\" src=\"", low_res_src,
        "\" onload=\"", DelayImagesFilter::kImageOnloadCode, "\">");
  }

  GoogleString GetImageOnloadScriptBlock() const {
    return StrCat("<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
                  DelayImagesFilter::kImageOnloadJsSnippet,
                  "</script>");
  }

  RequestHeaders request_headers_;
};

TEST_F(SplitHtmlHelperFilterTest, BasicTest) {
  Init();
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kSplitHtmlHelper));
  ValidateNoChanges("split_helper_basic_test", "");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, DisabledTest1) {
  Init();
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kSplitHtmlHelper));
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("");
  ValidateNoChanges("split_helper_disabled1", "");
  CheckLoggingStatus(RewriterHtmlApplication::DISABLED);
}

TEST_F(SplitHtmlHelperFilterTest, DisabledTest2) {
  Init();
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kSplitHtmlHelper));
  rewrite_driver()->SetUserAgent("does_not_support_split");
  ValidateNoChanges("split_helper_disabled2", "");
  CheckLoggingStatus(RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequest) {
  Init();
  ValidateExpected(
      "split_helper_atf",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=></div>"
      "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "</body>");
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, ExplicitAtfRequest) {
  Init();
  SetAtfRequest();
  ValidateExpected(
      "split_helper_explicit_atf",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=></div>"
      "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "</body>");
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestWithCriticalImages) {
  Init();
  CriticalImagesInfo* critical_images_info = new CriticalImagesInfo;
  critical_images_info->html_critical_images.insert("http://test.com/4.jpeg");
  critical_images_info->html_critical_images.insert("http://test.com/5.jpeg");
  rewrite_driver()->set_critical_images_info(critical_images_info);
  CheckNumCriticalImages(2);

  ValidateExpected(
      "split_helper_atf",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=></div>"
      "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "</body>");
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestWithNullCriticalImages) {
  Init();
  rewrite_driver()->set_critical_images_info(NULL);
  ValidateExpected(
      "split_helper_atf_with_null_critical_images",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=></div>"
      "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "</body>");
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, BtfRequest) {
  Init();
  SetBtfRequest();
  CriticalImagesInfo* critical_images_info = new CriticalImagesInfo;
  critical_images_info->html_critical_images.insert("http://test.com/1.jpeg");
  rewrite_driver()->set_critical_images_info(critical_images_info);
  CheckNumCriticalImages(1);
  ValidateNoChanges(
      "split_helper_btf",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>");
  CheckNumCriticalImages(0);
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestTwoXpaths) {
  Init();
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("div[@id=\"b\"]:div[@id=\"d\"]");

  ValidateExpected(
      "split_helper_atf_2xpaths",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'>"
        "<div id='c'><img src='3.jpeg'></div>"
      "</div>"
      "<div id='d'><img src='4.jpeg'></div>"
      "</body>",

      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=>"
        "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "</div>"
      "<div id='d'><img src='4.jpeg'></div>"
      "</body>");
  CheckNumCriticalImages(2);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckCriticalImage("http://test.com/4.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestXPathWithChildCount) {
  Init();
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("div[2]:div[4]");

  ValidateExpected(
      "split_helper_atf_xpath_with_child_count",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "<div id='d'><img src='4.jpeg'></div>"
      "</body>",

      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=></div>"
      "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "<div id='d'><img src='4.jpeg'></div>"
      "</body>");
  CheckNumCriticalImages(2);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckCriticalImage("http://test.com/4.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestNoDeferCases) {
  Init();
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("div[2]:div[4]");

  ValidateExpected(
      "split_helper_atf_nodefer_cases",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<script pagespeed_no_defer=\"\"></script>"
      "<div id='b'><img src='2.jpeg'>"
        "<script pagespeed_no_defer=\"\"></script>"
        "<div id='c' pagespeed_no_defer=\"\"></div>"
      "</div>"
      "<div id='d'><img src='3.jpeg'></div>"
      "<div id='e'><img src='4.jpeg'></div>"
      "</body>",

      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<script pagespeed_no_defer=\"\"></script>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=>"
        "<script pagespeed_no_defer=\"\"></script>"
        "<div id='c' pagespeed_no_defer=\"\"></div>"
      "</div>"
      "<div id='d'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "<div id='e'><img src='4.jpeg'></div>"
      "</body>");
  CheckNumCriticalImages(2);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckCriticalImage("http://test.com/4.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestNonCountedChildren) {
  Init();
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("div[2]");

  ValidateExpected(
      "split_helper_atf_nodefer_cases",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<link>"
      "<script></script>"
      "<noscript></noscript>"
      "<style></style>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>",

      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<link>"
      "<script></script>"
      "<noscript></noscript>"
      "<style></style>"
      "<div id='b'><img src='2.jpeg' pagespeed_no_transform=></div>"
      "<div id='c'><img src='3.jpeg' pagespeed_no_transform=></div>"
      "</body>");
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, BtfRequestConfigInHeader) {
  Init();
  SetBtfRequest();
  options()->ClearSignatureForTesting();
  options()->set_critical_line_config("");
  request_headers_.Add(HttpAttributes::kXPsaSplitConfig, "div[2]");
  rewrite_driver()->SetRequestHeaders(request_headers_);

  ValidateNoChanges(
      "split_helper_btf_request_config_in_header",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>");
  CheckNumCriticalImages(0);
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestWithLazyload) {
  TestCriticalImagesFinder* finder =
      new TestCriticalImagesFinder(NULL, statistics());
  server_context()->set_critical_images_finder(finder);
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
  options()->set_support_noscript_enabled(false);
  Init();
  ValidateExpected(
      "split_helper_atf_with_lazyload",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>",

      StrCat(
          "<body>"
          "<div id='a'><img src='1.jpeg'></div>"
          "<div id='b'>",
            GetLazyloadScriptHtml(),
            GetLazyloadImageTag("2.jpeg", true),
          "</div>",
          "<div id='c'>", GetLazyloadImageTag("3.jpeg", true), "</div>",
          GetLazyloadPostscriptHtml(),
          "</body>"));
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, BtfRequestWithLazyload) {
  TestCriticalImagesFinder* finder =
      new TestCriticalImagesFinder(NULL, statistics());
  server_context()->set_critical_images_finder(finder);
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
  options()->set_support_noscript_enabled(false);
  Init();
  SetBtfRequest();
  GoogleString expected_output_html = StrCat(
      "<body>"
      "<div id='a'>",
        GetLazyloadScriptHtml(),
        GetLazyloadImageTag("1.jpeg", false),
      "</div>");
  StrAppend(&expected_output_html,
          "<div id='b'>", GetLazyloadImageTag("2.jpeg", false), "</div>"
          "<div id='c'>", GetLazyloadImageTag("3.jpeg", false), "</div>",
          GetLazyloadPostscriptHtml(),
          "</body>");
  ValidateExpected(
      "split_helper_btf_with_lazyload",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>",
      expected_output_html);

  CheckNumCriticalImages(0);
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfRequestWithInlinePreview) {
  TestCriticalImagesFinder* finder =
      new TestCriticalImagesFinder(NULL, statistics());
  server_context()->set_critical_images_finder(finder);
  options()->EnableFilter(RewriteOptions::kDelayImages);
  options()->set_support_noscript_enabled(false);
  Init();
  AddFileToMockFetcher("http://test.com/1.jpeg", "Sample.jpg",
                       kContentTypeJpeg, 100);

  GoogleString input_html = "<body>"
      "<div id='a'><img src=\"1.jpeg\"></div>"
      "<div id='b'><img src=\"2.jpeg\"></div>"
      "<div id='c'><img src=\"3.jpeg\"></div>"
      "</body>";
  GoogleString output_html = StrCat(
      "<html>\n<body>"
      "<div id='a'>",
      GetImageOnloadScriptBlock(),
        GetInlinePreviewImageTag("1.jpeg", kSampleJpegData),
      "</div>"
      "<div id='b'><img src=\"2.jpeg\"></div>"
      "<div id='c'><img src=\"3.jpeg\"></div>"
      "</body>\n</html>");
  Parse("split_helper_atf_with_inline_preview", input_html);
  EXPECT_TRUE(Wildcard(output_html).Match(output_buffer_))
      << "Expected:\n" << output_html << "\n\nGot:\n" << output_buffer_;
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, BtfRequestWithInlinePreview) {
  TestCriticalImagesFinder* finder =
      new TestCriticalImagesFinder(NULL, statistics());
  server_context()->set_critical_images_finder(finder);
  options()->EnableFilter(RewriteOptions::kDelayImages);
  options()->set_support_noscript_enabled(false);
  Init();
  SetBtfRequest();
  AddFileToMockFetcher("http://test.com/1.jpeg", "Sample.jpg",
                       kContentTypeJpeg, 100);
  AddFileToMockFetcher("http://test.com/2.jpeg", "Sample.jpg",
                       kContentTypeJpeg, 100);
  AddFileToMockFetcher("http://test.com/3.jpeg", "Sample.jpg",
                       kContentTypeJpeg, 100);
  ValidateExpected(
      "split_helper_atf",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>",
      "<body>"
      "<div id='a'><img src='1.jpeg'></div>"
      "<div id='b'><img src='2.jpeg'></div>"
      "<div id='c'><img src='3.jpeg'></div>"
      "</body>");
  CheckNumCriticalImages(0);
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

TEST_F(SplitHtmlHelperFilterTest, AtfNestedPanelsRequestWithInlinePreview) {
  TestCriticalImagesFinder* finder =
      new TestCriticalImagesFinder(NULL, statistics());
  server_context()->set_critical_images_finder(finder);
  options()->EnableFilter(RewriteOptions::kDelayImages);
  options()->set_support_noscript_enabled(false);
  Init();
  AddFileToMockFetcher("http://test.com/1.jpeg", "Sample.jpg",
                       kContentTypeJpeg, 100);

  GoogleString input_html = "<body>"
      "<div id='a'><img src=\"1.jpeg\"></div>"
      "<div id='b'>"
        "<div id='c'></div>"
        "<img src=\"2.jpeg\"></div>"
      "<div id='d'><img src=\"3.jpeg\"></div>"
      "</body>";
  GoogleString output_html = StrCat(
      "<html>\n<body>"
      "<div id='a'>",
      GetImageOnloadScriptBlock(),
        GetInlinePreviewImageTag("1.jpeg", kSampleJpegData),
      "</div>"
      "<div id='b'>"
        "<div id='c'></div>"
        "<img src=\"2.jpeg\"></div>"
      "<div id='d'><img src=\"3.jpeg\"></div>"
      "</body>\n</html>");
  Parse("split_helper_atf_with_inline_preview", input_html);
  EXPECT_TRUE(Wildcard(output_html).Match(output_buffer_))
      << "Expected:\n" << output_html << "\n\nGot:\n" << output_buffer_;
  CheckNumCriticalImages(1);
  CheckCriticalImage("http://test.com/1.jpeg");
  CheckLoggingStatus(RewriterHtmlApplication::ACTIVE);
}

}  // namespace net_instaweb
