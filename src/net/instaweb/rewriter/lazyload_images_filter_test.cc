/*
 * Copyright 2011 Google Inc.
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

// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/rewriter/public/lazyload_images_filter.h"

#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/public/mock_critical_images_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class LazyloadImagesFilterTest : public RewriteTestBase {
 protected:
  LazyloadImagesFilterTest()
      : blank_image_src_("/psajs/1.0.gif") {}

  // TODO(matterbury): Delete this method as it should be redundant.
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetHtmlMimetype();  // Prevent insertion of CDATA tags to static JS.
  }

  virtual void InitLazyloadImagesFilter(bool debug) {
    if (debug) {
      options()->EnableFilter(RewriteOptions::kDebug);
    }
    options()->DisallowTroublesomeResources();
    lazyload_images_filter_.reset(
        new LazyloadImagesFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(lazyload_images_filter_.get());
  }

  GoogleString GetScriptHtml(const StringPiece& script) {
    return StrCat("<script type=\"text/javascript\"",
                  " pagespeed_no_defer=\"\"", ">", script, "</script>");
  }

  GoogleString GetLazyloadScriptHtml() {
    return GetScriptHtml(
        LazyloadImagesFilter::GetLazyloadJsSnippet(
            options(), server_context()->static_asset_manager()));
  }

  GoogleString GetOverrideAttributesScriptHtml() {
    return GetScriptHtml(LazyloadImagesFilter::kOverrideAttributeFunctions);
  }

  GoogleString GenerateRewrittenImageTag(
      const StringPiece& tag,
      const StringPiece& url,
      const StringPiece& additional_attributes) {
    return StrCat("<", tag, " pagespeed_lazy_src=\"", url, "\" ",
                  additional_attributes,
                  StrCat("src=\"",
                         blank_image_src_,
                         "\" onload=\"", LazyloadImagesFilter::kImageOnloadCode,
                         "\"/>"));
  }

  void ExpectLogRecord(int index, int status, bool is_blacklisted,
                       bool is_critical) {
    const RewriterInfo& rewriter_info = logging_info()->rewriter_info(index);
    EXPECT_EQ("ll", rewriter_info.id());
    EXPECT_EQ(status, rewriter_info.status());
    EXPECT_EQ(is_blacklisted,
              rewriter_info.rewrite_resource_info().is_blacklisted());
    EXPECT_EQ(is_critical,
              rewriter_info.rewrite_resource_info().is_critical());
  }

  GoogleString blank_image_src_;
  scoped_ptr<LazyloadImagesFilter> lazyload_images_filter_;
};

TEST_F(LazyloadImagesFilterTest, SingleHead) {
  InitLazyloadImagesFilter(false);

  ValidateExpected("lazyload_images",
      "<head></head>"
      "<body>"
      "<img />"
      "<img src=\"\" />"
      "<noscript>"
      "<img src=\"noscript.jpg\" />"
      "</noscript>"
      "<noembed>"
      "<img src=\"noembed.jpg\" />"
      "</noembed>"
      "<marquee>"
      "<img src=\"marquee.jpg\" />"
      "</marquee>"
      "<img src=\"1.jpg\" />"
      "<img src=\"1.jpg\" pagespeed_no_defer/>"
      "<img src=\"1.jpg\" data-src=\"2.jpg\"/>"
      "<img src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhE\"/>"
      "<img src=\"2's.jpg\" height=\"300\" width=\"123\" />"
      "<input src=\"12.jpg\"type=\"image\" />"
      "<input src=\"12.jpg\" />"
      "<img src=\"1.jpg\" onload=\"blah();\" />"
      "<img src=\"1.jpg\" class=\"123 dfcg-metabox\" />"
      "</body>",
      StrCat("<head></head><body><img/>"
             "<img src=\"\"/>"
             "<noscript>"
             "<img src=\"noscript.jpg\"/>"
             "</noscript>",
             "<noembed>"
             "<img src=\"noembed.jpg\"/>"
             "</noembed>"
             "<marquee>"
             "<img src=\"marquee.jpg\"/>"
             "</marquee>",
             GetLazyloadScriptHtml(),
             GenerateRewrittenImageTag("img", "1.jpg", ""),
             "<img src=\"1.jpg\" pagespeed_no_defer />"
             "<img src=\"1.jpg\" data-src=\"2.jpg\"/>",
             StrCat("<img src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhE\"/>",
                    GenerateRewrittenImageTag("img", "2's.jpg",
                                              "height=\"300\" width=\"123\" "),
                    "<input src=\"12.jpg\" type=\"image\"/>"
                    "<input src=\"12.jpg\"/>"
                    "<img src=\"1.jpg\" onload=\"blah();\"/>"
                    "<img src=\"1.jpg\" class=\"123 dfcg-metabox\"/>",
                    GetOverrideAttributesScriptHtml(),
                    "</body>")));
  EXPECT_EQ(4, logging_info()->rewriter_info().size());
  ExpectLogRecord(
      0, RewriterApplication::APPLIED_OK /* img with src 1.jpg */,
      false, false);
  ExpectLogRecord(
      1, RewriterApplication::NOT_APPLIED /* img with src 1.jpg and data-src */,
      false, false);
  ExpectLogRecord(
      2, RewriterApplication::APPLIED_OK /* img with src 2's.jpg*/,
      false, false);
  ExpectLogRecord(
      3, RewriterApplication::NOT_APPLIED /* img with src 1.jpg and onload */,
      false, false);
}

TEST_F(LazyloadImagesFilterTest, Blacklist) {
  options()->Disallow("*blacklist*");
  InitLazyloadImagesFilter(false);

  GoogleString input_html= "<head></head>"
      "<body>"
      "<img src=\"http://www.1.com/blacklist.jpg\"/>"
      "<img src=\"http://www.1.com/img1\"/>"
      "<img src=\"img2\"/>"
      "</body>";

  ValidateExpected(
      "lazyload_images",
      input_html,
      StrCat("<head></head><body>"
             "<img src=\"http://www.1.com/blacklist.jpg\"/>",
             GetLazyloadScriptHtml(),
             StrCat(
                 GenerateRewrittenImageTag(
                     "img", "http://www.1.com/img1", ""),
                 GenerateRewrittenImageTag(
                     "img", "img2", ""),
                 GetOverrideAttributesScriptHtml(),
                 "</body>")));
  EXPECT_EQ(3, logging_info()->rewriter_info().size());
  ExpectLogRecord(0, RewriterApplication::NOT_APPLIED, true, false);
  ExpectLogRecord(1, RewriterApplication::APPLIED_OK, false, false);
  ExpectLogRecord(2, RewriterApplication::APPLIED_OK, false, false);
}

TEST_F(LazyloadImagesFilterTest, CriticalImages) {
  InitLazyloadImagesFilter(false);
  MockCriticalImagesFinder* finder = new MockCriticalImagesFinder(statistics());
  server_context()->set_critical_images_finder(finder);

  StringSet* critical_images = new StringSet;
  critical_images->insert("http://www.1.com/critical");
  critical_images->insert("www.1.com/critical2");
  critical_images->insert("http://test.com/critical3");
  critical_images->insert("http://test.com/critical4.jpg");
  finder->set_critical_images(critical_images);

  GoogleString rewritten_url = Encode(
      "http://test.com/", "ce", "HASH", "critical4.jpg", "jpg");

  GoogleString input_html= StrCat(
      "<head></head>"
      "<body>"
      "<img src=\"http://www.1.com/critical\"/>"
      "<img src=\"http://www.1.com/critical2\"/>"
      "<img src=\"critical3\"/>"
      "<img src=\"", rewritten_url, "\"/>"
      "</body>");

  ValidateExpected(
      "lazyload_images",
      input_html,
      StrCat("<head></head><body>"
             "<img src=\"http://www.1.com/critical\"/>",
             GetLazyloadScriptHtml(),
             StrCat(
                 GenerateRewrittenImageTag(
                     "img", "http://www.1.com/critical2", ""),
                 "<img src=\"critical3\"/>"
                 "<img src=\"", rewritten_url, "\"/>",
                 GetOverrideAttributesScriptHtml(),
                 "</body>")));
  EXPECT_EQ(4, logging_info()->rewriter_info().size());
  ExpectLogRecord(0, RewriterApplication::NOT_APPLIED, false, true);
  ExpectLogRecord(1, RewriterApplication::APPLIED_OK, false, false);
  ExpectLogRecord(2, RewriterApplication::NOT_APPLIED, false, true);
  ExpectLogRecord(3, RewriterApplication::NOT_APPLIED, false, true);
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());
  rewrite_driver_->log_record()->WriteLog();
  for (int i = 0; i < logging_info()->rewriter_stats_size(); i++) {
    if (logging_info()->rewriter_stats(i).id() == "ll" &&
        logging_info()->rewriter_stats(i).has_html_status()) {
      EXPECT_EQ(RewriterHtmlApplication::ACTIVE,
                logging_info()->rewriter_stats(i).html_status());
      const RewriteStatusCount& count_applied =
          logging_info()->rewriter_stats(i).status_counts(0);
      EXPECT_EQ(RewriterApplication::APPLIED_OK,
                count_applied.application_status());
      EXPECT_EQ(1, count_applied.count());
      const RewriteStatusCount& count_not_applied =
          logging_info()->rewriter_stats(i).status_counts(1);
      EXPECT_EQ(RewriterApplication::NOT_APPLIED,
                count_not_applied.application_status());
      EXPECT_EQ(3, count_not_applied.count());
      return;
    }
  }
  FAIL();
}

TEST_F(LazyloadImagesFilterTest, SingleHeadLoadOnOnload) {
  options()->set_lazyload_images_after_onload(true);
  InitLazyloadImagesFilter(false);
  ValidateExpected("lazyload_images",
      "<head></head>"
      "<body>"
      "<img src=\"1.jpg\" />"
      "</body>",
      StrCat("<head></head>"
             "<body>",
             GetLazyloadScriptHtml(),
             GenerateRewrittenImageTag("img", "1.jpg", ""),
             GetOverrideAttributesScriptHtml(),
             "</body>"));
}

TEST_F(LazyloadImagesFilterTest, MultipleBodies) {
  InitLazyloadImagesFilter(false);
  ValidateExpected("lazyload_images",
      "<body><img src=\"1.jpg\" /></body>"
      "<body></body>"
      "<body>"
      "<script></script>"
      "<img src=\"2.jpg\" />"
      "<script></script>"
      "<img src=\"3.jpg\" />"
      "<script></script>"
      "</body>",
      StrCat(
          "<body>",
          GetLazyloadScriptHtml(),
          GenerateRewrittenImageTag("img", "1.jpg", ""),
          GetOverrideAttributesScriptHtml(),
          StrCat(
              "</body><body></body><body>"
              "<script></script>",
              GenerateRewrittenImageTag("img", "2.jpg", ""),
              GetOverrideAttributesScriptHtml()),
          StrCat(
              "<script></script>",
              GenerateRewrittenImageTag("img", "3.jpg", ""),
              GetOverrideAttributesScriptHtml(),
              "<script></script>",
              "</body>")));
}

TEST_F(LazyloadImagesFilterTest, NoHeadTag) {
  InitLazyloadImagesFilter(false);
  ValidateExpected("lazyload_images",
      "<body>"
      "<img src=\"1.jpg\" />"
      "</body>",
      StrCat("<body>",
             GetLazyloadScriptHtml(),
             GenerateRewrittenImageTag("img", "1.jpg", ""),
             GetOverrideAttributesScriptHtml(),
             "</body>"));
}

TEST_F(LazyloadImagesFilterTest, LazyloadImagesPreserveURLsOn) {
  // Make sure that we do not lazyload images when preserve urls is off.
  // This is a modification of the NoHeadTag test.
  options()->set_image_preserve_urls(true);
  options()->set_support_noscript_enabled(false);
  AddFilter(RewriteOptions::kLazyloadImages);

  ValidateNoChanges("lazyload_images",
      "<body>"
      "<img src=\"1.jpg\"/>"
      "</body>");
}

TEST_F(LazyloadImagesFilterTest, CustomImageUrl) {
  GoogleString blank_image_url = "http://blank.com/1.gif";
  options()->set_lazyload_images_blank_url(blank_image_url);
  blank_image_src_ = blank_image_url;
  InitLazyloadImagesFilter(false);
  ValidateExpected("lazyload_images",
      "<body>"
      "<img src=\"1.jpg\" />"
      "</body>",
      StrCat("<body>",
             GetLazyloadScriptHtml(),
             GenerateRewrittenImageTag("img", "1.jpg", ""),
             GetOverrideAttributesScriptHtml(),
             "</body>"));
}

TEST_F(LazyloadImagesFilterTest, DfcgClass) {
  InitLazyloadImagesFilter(false);
  GoogleString input_html = "<body class=\"dfcg-slideshow\">"
      "<img src=\"1.jpg\"/>"
      "<div class=\"dfcg\">"
      "<img src=\"1.jpg\"/>"
      "</div>"
      "</body>";
  ValidateNoChanges("lazyload_images", input_html);
}

TEST_F(LazyloadImagesFilterTest, NivoClass) {
  InitLazyloadImagesFilter(false);
  GoogleString input_html = "<body>"
      "<div class=\"nivo_sl\">"
      "<img src=\"1.jpg\"/>"
      "</div>"
      "<img class=\"nivo\" src=\"1.jpg\"/>"
      "</body>";
  ValidateNoChanges("lazyload_images", input_html);
}

TEST_F(LazyloadImagesFilterTest, ClassContainsSlider) {
  InitLazyloadImagesFilter(false);
  GoogleString input_html = "<body>"
      "<div class=\"SliderName2\">"
      "<img src=\"1.jpg\"/>"
      "</div>"
      "<img class=\"my_sLiDer\" src=\"1.jpg\"/>"
      "</body>";
  ValidateNoChanges("lazyload_images", input_html);
}

TEST_F(LazyloadImagesFilterTest, NoImages) {
  InitLazyloadImagesFilter(false);
  GoogleString input_html = "<head></head><body></body>";
  ValidateNoChanges("lazyload_images", input_html);
  EXPECT_EQ(0, logging_info()->rewriter_info().size());
}

TEST_F(LazyloadImagesFilterTest, LazyloadScriptOptimized) {
  InitLazyloadImagesFilter(false);
  Parse("optimized",
        "<head></head><body><img src=\"1.jpg\"></body>");
  EXPECT_EQ(GoogleString::npos, output_buffer_.find("/*"))
      << "There should be no comments in the optimized code";
}

TEST_F(LazyloadImagesFilterTest, LazyloadScriptDebug) {
  InitLazyloadImagesFilter(true);
  Parse("debug",
        "<head></head><body><img src=\"1.jpg\"></body>");
  EXPECT_NE(GoogleString::npos, output_buffer_.find("/*"))
      << "There should still be some comments in the debug code";
}

TEST_F(LazyloadImagesFilterTest, LazyloadDisabledWithJquerySlider) {
  InitLazyloadImagesFilter(false);
  GoogleString input_html = "<body>"
      "<head>"
      "<script src=\"jquery.sexyslider.js\"/>"
      "</head>"
      "<body>"
      "<img src=\"1.jpg\"/>"
      "</body>";
  // No change in the html.
  ValidateNoChanges("lazyload_images", input_html);
}

TEST_F(LazyloadImagesFilterTest, LazyloadDisabledWithJquerySliderAfterHead) {
  InitLazyloadImagesFilter(false);
  GoogleString input_html = "<head>"
      "</head>"
      "<body>"
      "<script src=\"jquery.sexyslider.js\"/>"
      "<img src=\"1.jpg\"/>"
      "</body>";
  ValidateNoChanges("abort_script_inserted", input_html);
}

TEST_F(LazyloadImagesFilterTest, LazyloadDisabledForOldBlackberry) {
  rewrite_driver()->SetUserAgent(
      UserAgentMatcherTestBase::kBlackBerryOS5UserAgent);
  InitLazyloadImagesFilter(false);
  GoogleString input_html = "<head>"
      "</head>"
      "<body>"
      "<img src=\"1.jpg\"/>"
      "</body>";
  ValidateNoChanges("blackberry_useragent", input_html);
}

TEST_F(LazyloadImagesFilterTest, LazyloadDisabledForGooglebot) {
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kGooglebotUserAgent);
  InitLazyloadImagesFilter(false);
  GoogleString input_html = "<head>"
      "</head>"
      "<body>"
      "<img src=\"1.jpg\"/>"
      "</body>";
  ValidateNoChanges("googlebot_useragent", input_html);
  rewrite_driver_->log_record()->WriteLog();
  LoggingInfo* logging_info = rewrite_driver_->log_record()->logging_info();
  for (int i = 0; i < logging_info->rewriter_stats_size(); i++) {
    if (logging_info->rewriter_stats(i).id() == "ll" &&
        logging_info->rewriter_stats(i).has_html_status()) {
      EXPECT_EQ(RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED,
                logging_info->rewriter_stats(i).html_status());
      return;
    }
  }
  FAIL();
}

TEST_F(LazyloadImagesFilterTest, LazyloadDisabledForXHR) {
  InitLazyloadImagesFilter(false);
  RequestHeaders request_headers;
  request_headers.Add(
      HttpAttributes::kXRequestedWith, HttpAttributes::kXmlHttpRequest);
  rewrite_driver_->SetRequestHeaders(request_headers);
  GoogleString input_html = "<head>"
      "</head>"
      "<body>"
      "<img src=\"1.jpg\"/>"
      "</body>";
  ValidateNoChanges("xhr_requests", input_html);
  rewrite_driver_->log_record()->WriteLog();
  LoggingInfo* logging_info = rewrite_driver_->log_record()->logging_info();
  for (int i = 0; i < logging_info->rewriter_stats_size(); i++) {
    if (logging_info->rewriter_stats(i).id() == "ll" &&
        logging_info->rewriter_stats(i).has_html_status()) {
      EXPECT_EQ(RewriterHtmlApplication::DISABLED,
                logging_info->rewriter_stats(i).html_status());
      EXPECT_TRUE(logging_info->has_is_xhr());
      EXPECT_TRUE(logging_info->is_xhr());
      return;
    }
  }
  FAIL();
}

}  // namespace net_instaweb
