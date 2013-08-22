/*
 * Copyright 2012 Google Inc.
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

// Author: rahulbansal@google.com (Rahul Bansal)

#include "net/instaweb/rewriter/public/split_html_filter.h"

#include "net/instaweb/htmlparse/public/html_writer_filter.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/critical_line_info.pb.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/split_html_helper_filter.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string_writer.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/html/html_name.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.test.com";

const char kHtmlInputPart1[] =
    "<html>"
    "<head>\n"
    "<script orig_index=1>blah</script>"
    "<script orig_index=2>blah2</script>"
    "</head>\n"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div id=\"item\">"
         "<img src=\"image1\" pagespeed_high_res_src=\"image1_high_res\""
           " onload=\"func\">"
         "<img src=\"image2\" pagespeed_high_res_src=\"image2_high_res\">"
      "</div>"
      "<span id=\"between\"> This is in between </span>"
      "<div id=\"inspiration\">"
         "<script orig_index=3></script>"
         "<img src=\"image11\">"
      "</div>";

const char kHtmlInputPart2[] =
      "<h3 id=\"afterInspirations\"> This is after Inspirations </h3>"
    "</div>"
    "<img id=\"image\" src=\"image_panel.1\">"
    "<script pagespeed_no_defer=\"\"></script>"
    "<h1 id=\"footer\" name style>"
      "This is the footer"
    "</h1>"
    "</body></html>";

const char kSplitHtmlPrefix[] =
    "<html><head>"
    "\n<script orig_index=1>blah</script>"
    "<script orig_index=2>blah2</script>";

const char kSplitHtmlMiddle[] =
    "</head>\n"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div id=\"item\">"
        "<img src=\"image1\" pagespeed_high_res_src=\"image1_high_res\">"
        "<img src=\"image2\" pagespeed_high_res_src=\"image2_high_res\">"
      "</div>"
      "<span id=\"between\"> This is in between </span>"
      "<!--GooglePanel begin panel-id.0--><!--GooglePanel end panel-id.0-->"
    "</div>"
    "<!--GooglePanel begin panel-id.1--><!--GooglePanel end panel-id.1-->"
    "<script pagespeed_no_defer=\"\"></script>"
    "<h1 id=\"footer\" name style>"
      "This is the footer"
    "</h1>"
    "</body></html>";

const char kSplitHtmlMiddleWithoutPanelStubs[] =
    "</head>\n"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div id=\"item\">"
         "<img src=\"image1\" pagespeed_high_res_src=\"image1_high_res\">"
         "<img src=\"image2\" pagespeed_high_res_src=\"image2_high_res\">"
      "</div>"
      "<span id=\"between\"> This is in between </span>"
      "<div id=\"inspiration\">"
         "<script orig_index=3></script>"
         "<img src=\"image11\">"
      "</div>";

const char kSplitHtmlBelowTheFoldData[] =
       "{\"panel-id.0\":[{\"instance_html\":\"<div id=\\\"inspiration\\\" panel-id=\\\"panel-id.0\\\"><script orig_index=3><\\/script><img src=\\\"image11\\\"></div><h3 id=\\\"afterInspirations\\\" panel-id=\\\"panel-id.0\\\"> This is after Inspirations </h3>\"}],"
       "\"panel-id.1\":[{\"instance_html\":\"<img id=\\\"image\\\" src=\\\"image_panel.1\\\" panel-id=\\\"panel-id.1\\\">\"}]}";

const char kHtmlInputForLazyload[] = "<html><head></head><body></body></html>";

const char kHtmlInputForIgnoreScript[] =
    "<html><body>%s<h1></h1>%s<h1></h1></body></html>";

const char kHtmlExpectedOutputForIgnoreScript1[] =
    "<html><head>%s</head><body>%s<h1></h1>%s"
    "<!--GooglePanel begin panel-id.0--><!--GooglePanel end panel-id.0-->"
    "</body></html>%s";

const char kHtmlExpectedOutputForIgnoreScript2[] =
    "<html><head></head><body>%s"
    "<!--GooglePanel begin panel-id.0--><!--GooglePanel end panel-id.0-->"
    "</body></html>%s";

class SplitHtmlFilterTest : public RewriteTestBase {
 public:
  SplitHtmlFilterTest(): writer_(&output_) {}

  virtual bool AddHtmlTags() const { return false; }

 protected:
  virtual void SetUp() {
    delete options_;
    options_ = new RewriteOptions(factory()->thread_system());
    options_->DisableFilter(RewriteOptions::kHtmlWriterFilter);
    RewriteTestBase::SetUp();

    rewrite_driver()->SetWriter(&writer_);
    SplitHtmlFilter* filter = new SplitHtmlFilter(rewrite_driver());
    html_writer_filter_.reset(filter);
    html_writer_filter_->set_writer(&writer_);
    rewrite_driver()->AddFilter(html_writer_filter_.get());
    rewrite_driver()->SetUserAgent(
        UserAgentMatcherTestBase::kChrome18UserAgent);

    response_headers_.set_status_code(HttpStatus::kOK);
    response_headers_.SetDateAndCaching(
        MockTimer::kApr_5_2010_ms, 10000, ",no-cache");
    response_headers_.Add(HttpAttributes::kPragma, "no-cache");
    response_headers_.Add(HttpAttributes::kAge, "1000");
    rewrite_driver()->set_response_headers_ptr(&response_headers_);
    output_.clear();
    StaticAssetManager* static_asset_manager =
        rewrite_driver()->server_context()->static_asset_manager();
    blink_js_url_ = static_asset_manager->GetAssetUrl(
        StaticAssetManager::kBlinkJs, options_).c_str();
    nodefer_str_ = HtmlKeywords::KeywordToString(HtmlName::kPagespeedNoDefer);
  }

  // TODO(marq): This looks reusable enough to go into RewriteTestBase. Perhaps
  // it should also know the rewriter-under-test's ID so there's less
  // boilerplate?
  void VerifyAppliedRewriters(GoogleString expected_rewriters) {
    EXPECT_STREQ(expected_rewriters, AppliedRewriterStringFromLog());
  }

  void VerifyJsonSize(int64 expected_size) {
    int64 actual_size = 0;
    if (logging_info()->has_split_html_info()) {
      actual_size = logging_info()->split_html_info().json_size();
    }
    EXPECT_EQ(expected_size, actual_size);
  }

  void SetBtfRequest() {
    rewrite_driver()->request_context()->set_split_request_type(
        RequestContext::SPLIT_BELOW_THE_FOLD);
  }

  void SetAtfRequest() {
    rewrite_driver()->request_context()->set_split_request_type(
        RequestContext::SPLIT_ABOVE_THE_FOLD);
  }

  GoogleString output_;
  RequestHeaders request_headers_;
  const char* blink_js_url_;
  ResponseHeaders response_headers_;
  const StringPiece* nodefer_str_;

 private:
  StringWriter writer_;
  SplitHtmlFilter* split_html_filter_;
};

TEST_F(SplitHtmlFilterTest, SplitHtmlWithDriverHavingCriticalLineInfo) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  CriticalLineInfo* config = new CriticalLineInfo;
  Panel* panel = config->add_panels();
  panel->set_start_xpath("div[@id = \"container\"]/div[4]");
  panel = config->add_panels();
  panel->set_start_xpath("img[3]");
  panel->set_end_marker_xpath("h1[@id = \"footer\"]");
  rewrite_driver()->set_critical_line_info(config);

  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString suffix(StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                                   blink_js_url_,
                                   SplitHtmlFilter::kLoadHiResImages, 2,
                                   kSplitHtmlBelowTheFoldData, "false"));
  EXPECT_EQ(StrCat(kSplitHtmlPrefix,
                   kSplitHtmlMiddle, suffix),
            output_);
  VerifyAppliedRewriters(
      RewriteOptions::FilterId(RewriteOptions::kSplitHtml));
  VerifyJsonSize(strlen(kSplitHtmlBelowTheFoldData));
}

TEST_F(SplitHtmlFilterTest, SplitHtmlAddMetaReferer) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_hide_referer_using_meta(true);
  CriticalLineInfo* config = new CriticalLineInfo;
  Panel* panel = config->add_panels();
  panel->set_start_xpath("div[@id = \"container\"]/div[4]");
  panel = config->add_panels();
  panel->set_start_xpath("img[3]");
  panel->set_end_marker_xpath("h1[@id = \"footer\"]");
  rewrite_driver()->set_critical_line_info(config);

  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString suffix(StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                                   blink_js_url_,
                                   SplitHtmlFilter::kLoadHiResImages, 2,
                                   kSplitHtmlBelowTheFoldData, "false"));
  EXPECT_EQ(StrCat("<html><head>",
                   SplitHtmlFilter::kMetaReferer,
                   "\n<script orig_index=1>blah</script>"
                   "<script orig_index=2>blah2</script>",
                   kSplitHtmlMiddle, suffix),
            output_);
  VerifyAppliedRewriters(
      RewriteOptions::FilterId(RewriteOptions::kSplitHtml));
  VerifyJsonSize(strlen(kSplitHtmlBelowTheFoldData));
}

TEST_F(SplitHtmlFilterTest,
       SplitTwoChunksHtmlWithDriverHavingCriticalLineInfoATF) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  CriticalLineInfo* config = new CriticalLineInfo;
  Panel* panel = config->add_panels();
  panel->set_start_xpath("div[@id = \"container\"]/div[4]");
  panel = config->add_panels();
  panel->set_start_xpath("img[3]");
  panel->set_end_marker_xpath("h1[@id = \"footer\"]");
  rewrite_driver()->set_critical_line_info(config);
  rewrite_driver()->AddOwnedEarlyPreRenderFilter(
      new SplitHtmlHelperFilter(rewrite_driver()));

  Parse("split_with_pcache?\"test",
        StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString suffix(
      StringPrintf(
          SplitHtmlFilter::kSplitTwoChunkSuffixJsFormatString,
          HttpAttributes::kXPsaSplitConfig,
          "div[@id = \"container\"]/div[4],img[3]:h1[@id = \"footer\"],", "1",
          SplitHtmlFilter::kLoadHiResImages, blink_js_url_, 2));
  EXPECT_EQ(StrCat(kSplitHtmlPrefix,
                   kSplitHtmlMiddle, suffix),
            output_);
  EXPECT_STREQ("1000", response_headers_.Lookup1(HttpAttributes::kAge));
  EXPECT_STREQ("no-cache", response_headers_.Lookup1(HttpAttributes::kPragma));
  ConstStringStarVector values;
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kCacheControl, &values));
  EXPECT_STREQ("max-age=10,no-cache", JoinStringStar(values, ","));
}

TEST_F(SplitHtmlFilterTest,
       SplitTwoChunksHtmlWithDriverHavingCriticalLineInfoATFAndCacheTime) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_max_html_cache_time_ms(30000);
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  CriticalLineInfo* config = new CriticalLineInfo;
  Panel* panel = config->add_panels();
  panel->set_start_xpath("div[@id = \"container\"]/div[4]");
  panel = config->add_panels();
  panel->set_start_xpath("img[3]");
  panel->set_end_marker_xpath("h1[@id = \"footer\"]");
  rewrite_driver()->set_critical_line_info(config);

  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString suffix(
      StringPrintf(
          SplitHtmlFilter::kSplitTwoChunkSuffixJsFormatString,
          HttpAttributes::kXPsaSplitConfig,
          "div[@id = \"container\"]/div[4],img[3]:h1[@id = \"footer\"],", "1",
          SplitHtmlFilter::kLoadHiResImages, blink_js_url_, 2));
  EXPECT_EQ(StrCat(kSplitHtmlPrefix,
                   kSplitHtmlMiddle, suffix),
            output_);
  EXPECT_EQ(NULL, response_headers_.Lookup1(
      HttpAttributes::kAccessControlAllowOrigin));
  EXPECT_EQ(NULL, response_headers_.Lookup1(
      HttpAttributes::kAccessControlAllowCredentials));
  EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kAge));
  EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kPragma));
  ConstStringStarVector values;
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kCacheControl, &values));
  EXPECT_STREQ("max-age=30,private", JoinStringStar(values, ","));
}

TEST_F(SplitHtmlFilterTest, SplitTwoChunksHtmlATFAndNoBTF) {
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  CriticalLineInfo* config = new CriticalLineInfo;
  Panel* panel = config->add_panels();
  // Use a non-existent xpath.
  panel->set_start_xpath("div[@id = \"abcd\"]/div[4]");
  rewrite_driver()->set_critical_line_info(config);

  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString expected_output(kSplitHtmlPrefix);
  GoogleString suffix(
      StringPrintf(SplitHtmlFilter::kSplitTwoChunkSuffixJsFormatString,
                   HttpAttributes::kXPsaSplitConfig,
                   "div[@id = \"abcd\"]/div[4],", "",
                   SplitHtmlFilter::kLoadHiResImages,
                   blink_js_url_, 3));
  StrAppend(&expected_output,
            kSplitHtmlMiddleWithoutPanelStubs,
            kHtmlInputPart2, suffix);

  EXPECT_EQ(expected_output, output_);
}

TEST_F(SplitHtmlFilterTest, SplitTwoChunksHtmlATFWithFlushAndHelper) {
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  options_->set_critical_line_config("div[@id = \"abcd\"]/div[4]");
  rewrite_driver()->AddOwnedEarlyPreRenderFilter(
      new SplitHtmlHelperFilter(rewrite_driver()));

  html_parse()->SetWriter(&write_to_string_);
  html_parse()->StartParse("http://example.com");
  html_parse()->ParseText(kHtmlInputPart1);
  html_parse()->Flush();
  html_parse()->ParseText(kHtmlInputPart2);
  html_parse()->FinishParse();

  GoogleString expected_output(kSplitHtmlPrefix);
  GoogleString suffix(
      StringPrintf(SplitHtmlFilter::kSplitTwoChunkSuffixJsFormatString,
                   HttpAttributes::kXPsaSplitConfig,
                   "div[@id = \"abcd\"]/div[4],", "",
                   SplitHtmlFilter::kLoadHiResImages,
                   blink_js_url_, 3));
  StrAppend(&expected_output,
            kSplitHtmlMiddleWithoutPanelStubs,
            kHtmlInputPart2, suffix);

  EXPECT_EQ(expected_output, output_buffer_);
}

TEST_F(SplitHtmlFilterTest, FlushBeforeParse) {
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  options_->set_critical_line_config("div[@id = \"abcd\"]/div[4]");
  rewrite_driver()->AddOwnedEarlyPreRenderFilter(
      new SplitHtmlHelperFilter(rewrite_driver()));

  html_parse()->SetWriter(&write_to_string_);
  html_parse()->StartParse("http://example.com");
  html_parse()->Flush();
  html_parse()->ParseText(kHtmlInputPart1);
  html_parse()->ParseText(kHtmlInputPart2);
  html_parse()->FinishParse();

  GoogleString expected_output(kSplitHtmlPrefix);
  GoogleString suffix(
      StringPrintf(SplitHtmlFilter::kSplitTwoChunkSuffixJsFormatString,
                   HttpAttributes::kXPsaSplitConfig,
                   "div[@id = \"abcd\"]/div[4],", "",
                   SplitHtmlFilter::kLoadHiResImages,
                   blink_js_url_, 3));
  StrAppend(&expected_output,
            kSplitHtmlMiddleWithoutPanelStubs,
            kHtmlInputPart2, suffix);

  EXPECT_EQ(expected_output, output_buffer_);
}

TEST_F(SplitHtmlFilterTest, ATFHeadersWithAllowAllOrigins) {
  request_headers_.Add(HttpAttributes::kOrigin, "abc.com");
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  options_->set_serve_xhr_access_control_headers(true);
  options_->set_access_control_allow_origins("*");
  CriticalLineInfo* config = new CriticalLineInfo;
  rewrite_driver()->set_critical_line_info(config);

  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString expected_output(kSplitHtmlPrefix);
  GoogleString suffix(
      StringPrintf(SplitHtmlFilter::kSplitTwoChunkSuffixJsFormatString,
                   HttpAttributes::kXPsaSplitConfig, "", "",
                   SplitHtmlFilter::kLoadHiResImages,
                   blink_js_url_, 3));
  StrAppend(&expected_output,
            kSplitHtmlMiddleWithoutPanelStubs,
            kHtmlInputPart2, suffix);
  EXPECT_EQ(expected_output, output_);
  EXPECT_STREQ("abc.com", response_headers_.Lookup1(
      HttpAttributes::kAccessControlAllowOrigin));
  EXPECT_STREQ("true", response_headers_.Lookup1(
      HttpAttributes::kAccessControlAllowCredentials));
}

TEST_F(SplitHtmlFilterTest, ATFHeadersCrossOriginAllowed) {
  request_headers_.Add(HttpAttributes::kOrigin, "http://cross-domain.com");
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  options_->set_serve_xhr_access_control_headers(true);
  options_->set_access_control_allow_origins(
      "example.com, *cross-domain.com, abc.com");
  CriticalLineInfo* config = new CriticalLineInfo;
  rewrite_driver()->set_critical_line_info(config);

  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString expected_output(kSplitHtmlPrefix);
  GoogleString suffix(
      StringPrintf(SplitHtmlFilter::kSplitTwoChunkSuffixJsFormatString,
                   HttpAttributes::kXPsaSplitConfig, "", "",
                   SplitHtmlFilter::kLoadHiResImages,
                   blink_js_url_, 3));
  StrAppend(&expected_output,
            kSplitHtmlMiddleWithoutPanelStubs,
            kHtmlInputPart2, suffix);
  EXPECT_EQ(expected_output, output_);
  EXPECT_STREQ("http://cross-domain.com", response_headers_.Lookup1(
      HttpAttributes::kAccessControlAllowOrigin));
  EXPECT_STREQ("true", response_headers_.Lookup1(
      HttpAttributes::kAccessControlAllowCredentials));
}

TEST_F(SplitHtmlFilterTest, ATFHeadersCrossOriginDisAllowed) {
  request_headers_.Add(HttpAttributes::kOrigin, "disallowed-domain.com");
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  options_->set_serve_xhr_access_control_headers(true);
  options_->set_access_control_allow_origins(
      "example.com, cross-domain.com, http://disallowed-domain.com, abc.com");
  CriticalLineInfo* config = new CriticalLineInfo;
  rewrite_driver()->set_critical_line_info(config);

  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString expected_output(kSplitHtmlPrefix);
  GoogleString suffix(
      StringPrintf(SplitHtmlFilter::kSplitTwoChunkSuffixJsFormatString,
                   HttpAttributes::kXPsaSplitConfig, "", "",
                   SplitHtmlFilter::kLoadHiResImages,
                   blink_js_url_, 3));
  StrAppend(&expected_output,
            kSplitHtmlMiddleWithoutPanelStubs,
            kHtmlInputPart2, suffix);
  EXPECT_EQ(expected_output, output_);
  EXPECT_EQ(NULL, response_headers_.Lookup1(
      HttpAttributes::kAccessControlAllowOrigin));
  EXPECT_STREQ(NULL, response_headers_.Lookup1(
      HttpAttributes::kAccessControlAllowCredentials));
}

TEST_F(SplitHtmlFilterTest,
       SplitTwoChunksHtmlWithDriverHavingCriticalLineInfoBTF) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_serve_split_html_in_two_chunks(true);
  SetBtfRequest();
  CriticalLineInfo* config = new CriticalLineInfo;
  Panel* panel = config->add_panels();
  panel->set_start_xpath("div[@id = \"container\"]/div[4]");
  panel = config->add_panels();
  panel->set_start_xpath("img[3]");
  panel->set_end_marker_xpath("h1[@id = \"footer\"]");
  rewrite_driver()->set_critical_line_info(config);

  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  EXPECT_EQ(kSplitHtmlBelowTheFoldData, output_);
}

TEST_F(SplitHtmlFilterTest,
       SplitTwoChunksHtmlWithRequestHeaderDriverHavingNoCriticalLineInfoBTF) {
  options_->set_serve_split_html_in_two_chunks(true);
  SetBtfRequest();
  rewrite_driver()->set_critical_line_info(NULL);
  request_headers_.Add(
      HttpAttributes::kXPsaSplitConfig,
      "div[@id = \"container\"]/div[4],img[3]:h1[@id = \"footer\"],");
  rewrite_driver()->SetRequestHeaders(request_headers_);
  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  EXPECT_EQ(kSplitHtmlBelowTheFoldData, output_);
}

TEST_F(SplitHtmlFilterTest,
       SplitTwoChunksHtmlWithRequestHeaderDriverHavingCriticalLineInfoBTF) {
  options_->set_serve_split_html_in_two_chunks(true);
  SetBtfRequest();
  CriticalLineInfo* config = new CriticalLineInfo;
  Panel* panel = config->add_panels();
  panel->set_start_xpath("div[@id = \"blah\"]/div[5]");
  rewrite_driver()->set_critical_line_info(config);
  request_headers_.Add(
      HttpAttributes::kXPsaSplitConfig,
      "div[@id = \"container\"]/div[4],img[3]:h1[@id = \"footer\"],");
  rewrite_driver()->SetRequestHeaders(request_headers_);

  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  EXPECT_EQ("{}", output_);
}

TEST_F(SplitHtmlFilterTest, SplitHtmlWithFlushingCachedHtml) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  CriticalLineInfo* config = new CriticalLineInfo;
  Panel* panel = config->add_panels();
  panel->set_start_xpath("div[@id = \"container\"]/div[4]");
  panel = config->add_panels();
  panel->set_start_xpath("img[3]");
  panel->set_end_marker_xpath("h1[@id = \"footer\"]");
  rewrite_driver()->set_critical_line_info(config);
  rewrite_driver()->set_flushing_cached_html(true);

  Parse("split_with_pcache", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString suffix(StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                                   blink_js_url_,
                                   SplitHtmlFilter::kLoadHiResImages, 2,
                                   kSplitHtmlBelowTheFoldData, "true"));
  EXPECT_EQ(StrCat(kSplitHtmlPrefix,
                   kSplitHtmlMiddle, suffix),
            output_);
  VerifyAppliedRewriters(
      RewriteOptions::FilterId(RewriteOptions::kSplitHtml));
  VerifyJsonSize(strlen(kSplitHtmlBelowTheFoldData));
}

TEST_F(SplitHtmlFilterTest, SplitHtmlWithOptions) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_critical_line_config(
      "div[@id = \"container\"]/div[4],"
      "img[3]:h1[@id = \"footer\"]");
  Parse("split_with_options", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString suffix(StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                                   blink_js_url_,
                                   SplitHtmlFilter::kLoadHiResImages, 2,
                                   kSplitHtmlBelowTheFoldData, "false"));
  EXPECT_EQ(StrCat(kSplitHtmlPrefix,
                   kSplitHtmlMiddle, suffix),
            output_);
  VerifyAppliedRewriters(
      RewriteOptions::FilterId(RewriteOptions::kSplitHtml));
  VerifyJsonSize(strlen(kSplitHtmlBelowTheFoldData));
}

TEST_F(SplitHtmlFilterTest, SplitHtmlWithFlushes) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_critical_line_config(
      "div[@id = \"container\"]/div[4],"
      "img[3]:h1[@id = \"footer\"]");
  html_parse()->StartParse("http://test.com/");
  html_parse()->ParseText(kHtmlInputPart1);
  html_parse()->Flush();
  html_parse()->ParseText(kHtmlInputPart2);
  html_parse()->FinishParse();
  GoogleString suffix(StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                                   blink_js_url_,
                                   SplitHtmlFilter::kLoadHiResImages, 2,
                                   kSplitHtmlBelowTheFoldData, "false"));
  EXPECT_EQ(StrCat(kSplitHtmlPrefix,
                   kSplitHtmlMiddle, suffix),
            output_);
  VerifyAppliedRewriters(
      RewriteOptions::FilterId(RewriteOptions::kSplitHtml));
  VerifyJsonSize(strlen(kSplitHtmlBelowTheFoldData));
}

TEST_F(SplitHtmlFilterTest, FlushEarlyHeadSuppress) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->ForceEnableFilter(
      RewriteOptions::RewriteOptions::kFlushSubresources);
  options_->set_critical_line_config(
      "div[@id = \"container\"]/div[4],"
      "img[3]:h1[@id = \"footer\"]");

  GoogleString pre_head_input = "<!DOCTYPE html><html><head>";
  GoogleString post_head_input =
      "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
      "<script src=\"b.js\"></script>"
      "</head>"
      "<body></body></html>";
  GoogleString suffix(StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                                   blink_js_url_,
                                   SplitHtmlFilter::kLoadHiResImages,
                                   -1, "{}", "false"));
  GoogleString post_head_output = StrCat(
      "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
      "<script src=\"b.js\"></script>",
      "</head><body></body></html>", suffix);
  GoogleString html_input = StrCat(pre_head_input, post_head_input);

  Parse("not_flushed_early", html_input);
  EXPECT_EQ(StrCat(pre_head_input, post_head_output), output_);
  VerifyAppliedRewriters("");
  VerifyJsonSize(0);

  // SuppressPreheadFilter should have populated the flush_early_proto with the
  // appropriate pre head information.
  EXPECT_EQ(pre_head_input,
            rewrite_driver()->flush_early_info()->pre_head());

  // pre head is suppressed if the dummy head was flushed early.
  output_.clear();
  rewrite_driver()->set_flushed_early(true);
  Parse("flushed_early", html_input);
  EXPECT_EQ(post_head_output, output_);
  VerifyAppliedRewriters("");
  VerifyJsonSize(0);
}

TEST_F(SplitHtmlFilterTest, FlushEarlyDisabled) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_critical_line_config(
      "div[@id = \"container\"]/div[4],"
      "img[3]:h1[@id = \"footer\"]");

  const char pre_head_input[] = "<!DOCTYPE html><html>";
  const char post_head_input[] =
      "<head>"
        "<link type=\"text/css\" rel=\"stylesheet\" href=\"a.css\"/>"
        "<script src=\"b.js\"></script>"
      "</head>"
      "<body></body></html>";
  GoogleString html_input = StrCat(pre_head_input, post_head_input);

  Parse("not_flushed_early", html_input);

  // SuppressPreheadFilter should not have populated the flush_early_proto.
  EXPECT_EQ("", rewrite_driver()->flush_early_info()->pre_head());
  VerifyAppliedRewriters("");
  VerifyJsonSize(0);
}

TEST_F(SplitHtmlFilterTest, SplitHtmlNoXpaths) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  CriticalLineInfo* info = new CriticalLineInfo;
  rewrite_driver()->set_critical_line_info(info);
  options_->set_critical_line_config("");
  Parse("split_without_xpaths", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString expected_output(kSplitHtmlPrefix);
  GoogleString suffix(StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                                   blink_js_url_,
                                   SplitHtmlFilter::kLoadHiResImages,
                                   3, "{}", "false"));
  StrAppend(&expected_output,
            kSplitHtmlMiddleWithoutPanelStubs,
            kHtmlInputPart2, suffix);
  EXPECT_EQ(expected_output, output_);
  VerifyAppliedRewriters("");
  VerifyJsonSize(0);
}

TEST_F(SplitHtmlFilterTest, SplitHtmlNoXpathsTwoChunksATF) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  CriticalLineInfo* info = new CriticalLineInfo;
  rewrite_driver()->set_critical_line_info(info);
  options_->set_critical_line_config("");
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  Parse("split_without_xpaths", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  GoogleString expected_output(kSplitHtmlPrefix);
  GoogleString suffix(
      StringPrintf(SplitHtmlFilter::kSplitTwoChunkSuffixJsFormatString,
                   HttpAttributes::kXPsaSplitConfig, "", "",
                   SplitHtmlFilter::kLoadHiResImages,
                   blink_js_url_, 3));
  StrAppend(&expected_output,
            kSplitHtmlMiddleWithoutPanelStubs,
            kHtmlInputPart2, suffix);
  EXPECT_EQ(expected_output, output_);
  VerifyAppliedRewriters("");
  VerifyJsonSize(0);
}

TEST_F(SplitHtmlFilterTest, SplitHtmlNoXpathsTwoChunksBTF) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  CriticalLineInfo* info = new CriticalLineInfo;
  rewrite_driver()->set_critical_line_info(info);
  options_->set_critical_line_config("");
  SetBtfRequest();
  options_->set_serve_split_html_in_two_chunks(true);
  Parse("split_without_xpaths", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  EXPECT_EQ("{}", output_);
  VerifyAppliedRewriters("");
  VerifyJsonSize(0);
}

TEST_F(SplitHtmlFilterTest, SplitHtmlNoInfoTwoChunksATF) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  rewrite_driver()->set_critical_line_info(NULL);
  options_->set_serve_split_html_in_two_chunks(true);
  const GoogleString html(StrCat(kHtmlInputPart1, kHtmlInputPart2));
  Parse("split_cache_miss", html);
  EXPECT_EQ(html, output_);
  VerifyAppliedRewriters("");
  VerifyJsonSize(0);
}

TEST_F(SplitHtmlFilterTest, SplitHtmlNoInfoTwoChunksBTF) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  rewrite_driver()->set_critical_line_info(NULL);
  SetBtfRequest();
  options_->set_serve_split_html_in_two_chunks(true);
  const GoogleString html(StrCat(kHtmlInputPart1, kHtmlInputPart2));
  Parse("split_cache_miss", html);
  EXPECT_EQ(html, output_);
  VerifyAppliedRewriters("");
  VerifyJsonSize(0);
}

TEST_F(SplitHtmlFilterTest, SplitHtmlNoInfo) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  rewrite_driver()->set_critical_line_info(NULL);
  const GoogleString html(StrCat(kHtmlInputPart1, kHtmlInputPart2));
  Parse("split_cache_miss", html);
  GoogleString expected_output(kSplitHtmlPrefix);
  GoogleString suffix(StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                                   blink_js_url_,
                                   SplitHtmlFilter::kLoadHiResImages,
                                   3, "{}", "false"));
  StrAppend(&expected_output,
            kSplitHtmlMiddleWithoutPanelStubs,
            kHtmlInputPart2, suffix);
  EXPECT_EQ(expected_output, output_);
  VerifyAppliedRewriters("");
  VerifyJsonSize(0);
}

TEST_F(SplitHtmlFilterTest, SplitHtmlWithUnsupportedUserAgent) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_critical_line_config(
      "div[@id = \"container\"]/div[4],"
      "img[3]:h1[@id = \"footer\"]");
  rewrite_driver()->SetUserAgent("BlackListUserAgent");
  Parse("split_with_options", StrCat(kHtmlInputPart1, kHtmlInputPart2));
  EXPECT_EQ(StrCat(kHtmlInputPart1, kHtmlInputPart2), output_);
  VerifyAppliedRewriters("");
  VerifyJsonSize(0);
}

TEST_F(SplitHtmlFilterTest, SplitHtmlIgnoreScriptNoscript1) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_critical_line_config("h1[2]");
  GoogleString expected_output_suffix(
      StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                   blink_js_url_,
                   SplitHtmlFilter::kLoadHiResImages, -1,
                   "{\"panel-id.0\":[{\"instance_html\":"
                   "\"<h1 panel-id=\\\"panel-id.0\\\">"
                   "</h1>\"}]}", "false"));
  GoogleString input(StringPrintf(kHtmlInputForIgnoreScript, "", ""));
  Parse("split_ignore_script1", input);
  EXPECT_EQ(StringPrintf(kHtmlExpectedOutputForIgnoreScript1,
                         "", "", "",
                         expected_output_suffix.c_str()).c_str(), output_);
  VerifyAppliedRewriters("sh");
}

TEST_F(SplitHtmlFilterTest, SplitHtmlIgnoreScriptNoscript2) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_critical_line_config("h1[2]");
  GoogleString expected_output_suffix(
      StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                   blink_js_url_,
                   SplitHtmlFilter::kLoadHiResImages, -1,
                   "{\"panel-id.0\":[{\"instance_html\":"
                   "\"<h1 panel-id=\\\"panel-id.0\\\">"
                   "</h1>\"}]}", "false"));
  GoogleString input = StringPrintf(kHtmlInputForIgnoreScript, "",
                                    "<script></script><noscript></noscript>");
  Parse("split_ignore_script2", input);
  EXPECT_EQ(StringPrintf(kHtmlExpectedOutputForIgnoreScript1,
                         "", "",
                         "<script></script><noscript></noscript>",
                         expected_output_suffix.c_str()).c_str(), output_);
  VerifyAppliedRewriters("sh");
}

TEST_F(SplitHtmlFilterTest, SplitHtmlIgnoreScriptNoscript3) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_critical_line_config("h1[2]");
  GoogleString expected_output_suffix(
      StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                   blink_js_url_,
                   SplitHtmlFilter::kLoadHiResImages, -1,
                   "{\"panel-id.0\":[{\"instance_html\":"
                   "\"<h1 panel-id=\\\"panel-id.0\\\">"
                   "</h1>\"}]}", "false"));
  GoogleString input = StringPrintf(kHtmlInputForIgnoreScript,
                                    "<script></script><noscript></noscript>",
                                    "<script></script><noscript></noscript>");
  Parse("split_ignore_script3", input);
  EXPECT_EQ(StringPrintf(kHtmlExpectedOutputForIgnoreScript1,
                         "", "<script></script><noscript></noscript>",
                         "<script></script><noscript></noscript>",
                         expected_output_suffix.c_str()).c_str(), output_);
  VerifyAppliedRewriters("sh");
}

TEST_F(SplitHtmlFilterTest, SplitHtmlIgnoreScriptNoscript4) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_critical_line_config("h1[1]");
  GoogleString expected_output_suffix(
      StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                   blink_js_url_,
                   SplitHtmlFilter::kLoadHiResImages, -1,
                   "{\"panel-id.0\":[{\"instance_html\":"
                   "\"<h1 panel-id=\\\"panel-id.0\\\">"
                   "</h1>"
                   "<h1 panel-id=\\\"panel-id.0\\\">"
                   "</h1>\"}]}", "false"));
  GoogleString input = StringPrintf(kHtmlInputForIgnoreScript, "", "");
  Parse("split_ignore_script4", input);
  EXPECT_EQ(StringPrintf(kHtmlExpectedOutputForIgnoreScript2,
                         "",
                         expected_output_suffix.c_str()).c_str(), output_);
  VerifyAppliedRewriters("sh");
}

TEST_F(SplitHtmlFilterTest, SplitHtmlIgnoreScriptNoscript5) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->set_critical_line_config("h1[1]");
  GoogleString expected_output_suffix(
      StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                   blink_js_url_,
                   SplitHtmlFilter::kLoadHiResImages, -1,
                   "{\"panel-id.0\":[{\"instance_html\":"
                   "\"<h1 panel-id=\\\"panel-id.0\\\">"
                   "</h1>"
                   "<h1 panel-id=\\\"panel-id.0\\\">"
                   "</h1>\"}]}", "false"));
  GoogleString input = StringPrintf(
      kHtmlInputForIgnoreScript,
      "<script></script><noscript></noscript>"
      "<style></style><link href=\"http://a.com/\">", "");
      Parse("split_ignore_script5", input);
  EXPECT_EQ(StringPrintf(kHtmlExpectedOutputForIgnoreScript2,
                         "<script></script><noscript></noscript>"
                         "<style></style><link href=\"http://a.com/\">",
                         expected_output_suffix.c_str()).c_str(), output_);
  VerifyAppliedRewriters("sh");
}

TEST_F(SplitHtmlFilterTest, SplitHtmlWithGhostClickBuster) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  options_->ClearSignatureForTesting();
  options_->set_serve_ghost_click_buster_with_split_html(true);
  options_->set_critical_line_config("h1[2]");
  GoogleString expected_output_suffix(
      StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                   blink_js_url_,
                   SplitHtmlFilter::kLoadHiResImages, -1,
                   "{\"panel-id.0\":[{\"instance_html\":"
                   "\"<h1 panel-id=\\\"panel-id.0\\\">"
                   "</h1>\"}]}", "false"));
  GoogleString input(StringPrintf(kHtmlInputForIgnoreScript, "", ""));
  Parse("split_ignore_script1", input);
  StaticAssetManager* static_asset_manager =
        rewrite_driver_->server_context()->static_asset_manager();

  EXPECT_EQ(StringPrintf(kHtmlExpectedOutputForIgnoreScript1,
                         StrCat("<script type=\"text/javascript\">",
                                static_asset_manager->GetAsset(
                                    StaticAssetManager::kGhostClickBusterJs,
                                    options_), "</script>").c_str(),
                         "", "",
                         expected_output_suffix.c_str()).c_str(), output_);
  VerifyAppliedRewriters("sh");
}

TEST_F(SplitHtmlFilterTest, SplitHtmlWithNestedPanels) {
  rewrite_driver()->SetRequestHeaders(request_headers_);
  GoogleString input_html =
      "<html><head></head><body>"
        "<div id=\"outer\">"
          "<div id=\"inner\"></div>"
        "</div>"
      "</body></html>";
  options_->set_critical_line_config(
      "div[@id = \"outer\"],"
      "div[@id = \"inner\"]");
    GoogleString expected_output_suffix(
      StringPrintf(
          SplitHtmlFilter::kSplitSuffixJsFormatString,
          blink_js_url_,
          SplitHtmlFilter::kLoadHiResImages, -1,
          "{\"panel-id.0\":[{\"instance_html\":"
            "\"<div id=\\\"outer\\\" panel-id=\\\"panel-id.0\\\">"
            "<div id=\\\"inner\\\"></div></div>\"}]}",
          "false"));
  Parse("split_with_options", input_html);
  EXPECT_EQ(StrCat("<html><head></head><body>"
                    "<!--GooglePanel begin panel-id.0-->"
                    "<!--GooglePanel end panel-id.0-->"
                    "</body></html>",
                    expected_output_suffix),
            output_);
  VerifyAppliedRewriters("sh");
}

// When an ATF request is received, we defer the instrumentation script.
TEST_F(SplitHtmlFilterTest, Instrumentation1) {
  options()->set_critical_line_config("div[@id=\"god\"]");
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  rewrite_driver()->AddOwnedEarlyPreRenderFilter(
      new AddInstrumentationFilter(rewrite_driver()));
  Parse("defer_instrumentation",
        "<html><head>"
        "</head><body>"
        "<div id='1'/>"
        "<div id='god'/><div id='2'/>"
        "</body></html>");
  GoogleString expected =
      "<html><head>"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_start = Number(new Date());</script>"
      "</head><body>"
      "<div id='1'/>"
      "<!--GooglePanel begin panel-id.0--><!--GooglePanel end panel-id.0-->"
      "</body></html>";
  EXPECT_TRUE(output_.find(expected) != GoogleString::npos);
  EXPECT_TRUE(output_.find(nodefer_str_->as_string()) == GoogleString::npos);
}

// When an ATF request is received but the useragent is unsupported, we don't
// defer the instrumentation script.
TEST_F(SplitHtmlFilterTest, Instrumentation2) {
  rewrite_driver()->SetUserAgent("BlackListUserAgent");
  options()->set_critical_line_config("div[@id=\"god\"]");
  options_->set_serve_split_html_in_two_chunks(true);
  SetAtfRequest();
  rewrite_driver()->AddOwnedEarlyPreRenderFilter(
      new AddInstrumentationFilter(rewrite_driver()));
  Parse("nodefer_instrumentation",
        "<html><head>"
        "</head><body>"
        "<div id='1'/>"
        "<div id='god'/><div id='2'/>"
        "</body></html>");
  GoogleString expected =
      "<html><head>"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_start = Number(new Date());</script>"
      "</head><body>"
      "<div id='1'/>"
      "<div id='god'/><div id='2'/>";
  EXPECT_TRUE(output_.find(expected) != GoogleString::npos);
  EXPECT_TRUE(output_.find(nodefer_str_->as_string()) != GoogleString::npos);
}

}  // namespace

}  // namespace net_instaweb
