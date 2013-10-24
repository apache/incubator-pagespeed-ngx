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

// Author: pulkitg@google.com (Pulkit Goyal)

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/log_record_test_helper.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/image_types.pb.h"
#include "net/instaweb/rewriter/public/critical_images_finder_test_base.h"
#include "net/instaweb/rewriter/public/delay_images_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/wildcard.h"

namespace {
const char kSampleJpgFile[] = "Sample.jpg";
const char kSampleWebpFile[] = "Sample_webp.webp";
const char kLargeJpgFile[] = "Puzzle.jpg";
const char kSmallPngFile[] = "BikeCrashIcn.png";

// Generated html is matched approximately because different versions of
// libjpeg are yeilding different low_res_image_data.
const char kSampleJpegData[] = "data:image/jpeg;base64*";
const char kSampleWebpData[] = "data:image/webp;base64*";
const char kSamplePngData[] = "data:image/png;base64*";

const char kHeadHtml[] = "<head></head>";

const char kScriptTemplate[] =
    "<script pagespeed_no_defer=\"\" type=\"text/javascript\">%s</script>";

}  // namespace

namespace net_instaweb {

class DelayImagesFilterTest : public RewriteTestBase {
 public:
  DelayImagesFilterTest() {
    options()->set_min_image_size_low_resolution_bytes(1 * 1024);
    options()->set_max_inlined_preview_images_index(-1);
  }

 protected:
  // TODO(matterbury): Delete this method as it should be redundant.
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetHtmlMimetype();  // Prevent insertion of CDATA tags to static JS.
  }

  virtual bool AddHtmlTags() const { return false; }
  // Match rewritten html content and return its byte count.
  int MatchOutputAndCountBytes(const GoogleString& html_input,
                               const GoogleString& expected) {
    Parse("inline_preview_images", html_input);
    GoogleString full_html = doctype_string_ + AddHtmlBody(expected);
    EXPECT_TRUE(Wildcard(full_html).Match(output_buffer_)) <<
        "Expected:\n" << full_html << "\n\nGot:\n" << output_buffer_;
    int output_size = output_buffer_.size();
    output_buffer_.clear();
    return output_size;
  }

  GoogleString GetNoscript() const {
    return StringPrintf(
        kNoScriptRedirectFormatter,
        "http://test.com/inline_preview_images.html?ModPagespeed=noscript",
        "http://test.com/inline_preview_images.html?ModPagespeed=noscript");
  }

  GoogleString GenerateAddLowResScript(const GoogleString& url,
                                       const GoogleString& image_data) {
    return StringPrintf(
        kScriptTemplate,
        StrCat("\npagespeed.delayImagesInline.addLowResImages(\'", url,
               "\', \'", image_data, "\');\n"
               "pagespeed.delayImagesInline.replaceWithLowRes();\n").c_str());
  }

  GoogleString GenerateRewrittenImageTag(const GoogleString& url,
                                         const GoogleString& low_res_src) {
    return StrCat(
        "<img pagespeed_high_res_src=\"", url, "\" src=\"", low_res_src,
        "\" onload=\"", DelayImagesFilter::kOnloadFunction, "\"/>");
  }

  GoogleString GetInlineScript() {
    return StringPrintf(
        kScriptTemplate,
        StrCat(GetDelayImagesInlineCode(),
               GetJsCode(StaticAssetManager::kDelayImagesJs,
                          DelayImagesFilter::kDelayImagesSuffix)).c_str());
  }

  GoogleString GetHighResScript() {
    return StringPrintf(kScriptTemplate,
                        "\npagespeed.delayImages.replaceWithHighRes();\n");
  }

  GoogleString GetLazyHighResScript() {
    return StringPrintf(kScriptTemplate,
                        "\npagespeed.delayImages.registerLazyLoadHighRes();\n");
  }

  GoogleString GetDelayImagesInlineCode() {
    return GetJsCode(StaticAssetManager::kDelayImagesInlineJs,
                     DelayImagesFilter::kDelayImagesInlineSuffix);
  }

  GoogleString GetJsCode(StaticAssetManager::StaticAsset module,
                         const StringPiece& call) {
    StringPiece code =
        server_context()->static_asset_manager()->GetAsset(module, options());
    return StrCat(code, call);
  }

  void SetupUserAgentTest(StringPiece user_agent) {
    ClearRewriteDriver();
    rewrite_driver()->SetUserAgent(user_agent);
    SetHtmlMimetype();  // Prevent insertion of CDATA tags to static JS.
  }

  void ExpectLogRecord(int index, const RewriterInfo& expected_info) {
    ScopedMutex lock(rewrite_driver()->log_record()->mutex());
    ASSERT_LT(index, rewrite_driver()->log_record()->logging_info()
              ->rewriter_info_size());
    const RewriterInfo& actual_info = rewrite_driver()->log_record()
        ->logging_info()->rewriter_info(index);
    EXPECT_EQ(expected_info.id(), actual_info.id());
    EXPECT_EQ(expected_info.status(), actual_info.status());
    EXPECT_EQ(expected_info.has_rewrite_resource_info(),
              actual_info.has_rewrite_resource_info());
    EXPECT_EQ(expected_info.has_image_rewrite_resource_info(),
              actual_info.has_image_rewrite_resource_info());
    if (expected_info.has_rewrite_resource_info()) {
      EXPECT_EQ(expected_info.rewrite_resource_info().is_inlined(),
                actual_info.rewrite_resource_info().is_inlined());
      EXPECT_EQ(expected_info.rewrite_resource_info().is_critical(),
                actual_info.rewrite_resource_info().is_critical());
    }
    if (expected_info.has_image_rewrite_resource_info()) {
      const ImageRewriteResourceInfo& expected_image_info =
          expected_info.image_rewrite_resource_info();
      const ImageRewriteResourceInfo& actual_image_info =
          actual_info.image_rewrite_resource_info();
      EXPECT_EQ(expected_image_info.is_low_res_src_inserted(),
                actual_image_info.is_low_res_src_inserted());
      EXPECT_GE(expected_image_info.low_res_size(),
                actual_image_info.low_res_size());
    }
  }
};

TEST_F(DelayImagesFilterTest, DelayImagesAcrossDifferentFlushWindow) {
  options()->set_lazyload_highres_images(true);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                         kContentTypeWebp, 100);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString flush1 = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\" />";
  GoogleString flush2="<img src=\"http://test.com/1.jpeg\" />"
      "</body>";
  SetMockLogRecord();
  MockLogRecord* log = mock_log_record();
  EXPECT_CALL(*log,
              MockLogImageRewriteActivity(LogImageRewriteActivityMatcher(
                  StrEq("ic"),
                  StrEq("http://test.com/1.webp"),
                  RewriterApplication::NOT_APPLIED,
                  false /* is_image_inlined */,
                  true /* is_critical_image */,
                  false /* is_url_rewritten */,
                  1780 /* original size */,
                  true /* try_low_res_src_insertion */,
                  true /* low_res_src_inserted */,
                  IMAGE_WEBP,
                  _ /* low_res_data_size */)));
  EXPECT_CALL(*log,
              MockLogImageRewriteActivity(LogImageRewriteActivityMatcher(
                  StrEq("ic"),
                  StrEq("http://test.com/1.jpeg"),
                  RewriterApplication::NOT_APPLIED,
                  false /* is_image_inlined */,
                  true /* is_critical_image */,
                  false /* is_url_rewritten */,
                  8010 /* original size */,
                  true /* try_low_res_src_insertion */,
                  true /* low_res_src_inserted */,
                  IMAGE_JPEG,
                  _ /* low_res_data_size */)));
  SetupWriter();
  html_parse()->StartParse("http://test.com/inline_preview_images.html");
  html_parse()->ParseText(flush1);
  html_parse()->Flush();
  html_parse()->ParseText(flush2);
  html_parse()->FinishParse();

  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      GenerateRewrittenImageTag("http://test.com/1.webp",
                                kSampleWebpData),
      GenerateRewrittenImageTag("http://test.com/1.jpeg",
                                kSampleJpegData),
      "</body>");
  EXPECT_TRUE(Wildcard(output_html).Match(output_buffer_));
  EXPECT_TRUE(AppliedRewriterStringFromLog().find("di") !=
              GoogleString::npos);

  RewriterInfo expected;
  expected.set_id("di");
  expected.set_status(RewriterApplication::APPLIED_OK);

  ExpectLogRecord(0, expected);
  ExpectLogRecord(1, expected);

  rewrite_driver_->log_record()->WriteLog();
  LoggingInfo* logging_info = rewrite_driver_->log_record()->logging_info();
  for (int i = 0; i < logging_info->rewriter_stats_size(); i++) {
    if (logging_info->rewriter_stats(i).id() == "di" &&
        logging_info->rewriter_stats(i).has_html_status()) {
      EXPECT_EQ(RewriterHtmlApplication::ACTIVE,
                logging_info->rewriter_stats(i).html_status());
      return;
    }
  }
  FAIL();
}

TEST_F(DelayImagesFilterTest, DelayImagesPreserveURLsOn) {
  // Make sure that we don't delay images when preserve urls is on.
  options()->set_image_preserve_urls(true);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  const char kInputHtml[] =
      "<html><head></head><body>"
      "<img src=\"http://test.com/1.jpeg\"/>"
      "</body></html>";

  MatchOutputAndCountBytes(kInputHtml, kInputHtml);
}

TEST_F(DelayImagesFilterTest, DelayImageInsideNoscript) {
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<noscript><img src=\"http://test.com/1.webp\" /></noscript>"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head>",
      StrCat("<body>", GetNoscript(), "<noscript>"
             "<img src=\"http://test.com/1.webp\"/></noscript></body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageWithUnsupportedUserAgent) {
  AddFilter(RewriteOptions::kDelayImages);
  SetupUserAgentTest("unsupported");
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>";
  MatchOutputAndCountBytes(input_html, input_html);
  rewrite_driver_->log_record()->WriteLog();
  LoggingInfo* logging_info = rewrite_driver_->log_record()->logging_info();
  for (int i = 0; i < logging_info->rewriter_stats_size(); i++) {
    if (logging_info->rewriter_stats(i).id() == "di" &&
        logging_info->rewriter_stats(i).has_html_status()) {
      EXPECT_EQ(RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED,
                logging_info->rewriter_stats(i).html_status());
      return;
    }
  }
  FAIL();
}

TEST_F(DelayImagesFilterTest, DelayImageWithQueryParam) {
  options()->DisableFilter(RewriteOptions::kInlineImages);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp?a=b&c=d", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp?a=b&amp;c=d\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      GenerateRewrittenImageTag("http://test.com/1.webp?a=b&amp;c=d",
                                kSampleWebpData),
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageWithUnescapedQueryParam) {
  options()->DisableFilter(RewriteOptions::kInlineImages);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp?a=b&c=d", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp?a=b&c=d\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      GenerateRewrittenImageTag("http://test.com/1.webp?a=b&c=d",
                                kSampleWebpData),
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageWithOnlyUrlValuedAttribute) {
  options()->AddUrlValuedAttribute("img", "data-src", semantic_type::kImage);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head><body>"
      "<img data-src=\"http://test.com/1.webp\"/>"
      "</body>";
  // No change made.
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img data-src=\"http://test.com/1.webp\"/>",
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageWithSrcAndUrlValuedAttribute) {
  options()->AddUrlValuedAttribute("img", "data-src", semantic_type::kImage);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  AddFileToMockFetcher("http://test.com/2.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head><body>"
      "<img src=\"http://test.com/1.webp\""
      "     data-src=\"http://test.com/2.jpeg\"/>"
      "</body>";
  // Inlined image will be a blank png instead of a low res webp.
  GoogleString output_html = StrCat(
      "<head></head><body>", GetNoscript(),
      "<img pagespeed_high_res_src=\"http://test.com/1.webp\" "
      "data-src=\"http://test.com/2.jpeg\" src=\"", kSampleWebpData,
      "\" onload=\"", DelayImagesFilter::kOnloadFunction, "\"/></body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageWithBlankImage) {
  options()->set_use_blank_image_for_inline_preview(true);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head><body>"
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>";
  // Inlined image will be a blank png instead of a low res webp.
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      GenerateRewrittenImageTag("http://test.com/1.webp", kSamplePngData),
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageWithBlankImageOnMobile) {
  options()->set_enable_aggressive_rewriters_for_mobile(true);
  options()->set_use_blank_image_for_inline_preview(true);
  AddFilter(RewriteOptions::kDelayImages);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head><body>"
      "<img src=\"http://test.com/1.jpeg\"/>"
      "</body>";
  // Inlined image will be a blank png instead of a low res Jpeg.
  // Even for the mobile user agent, the image is inlined if its a blank image.
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      GenerateRewrittenImageTag("http://test.com/1.jpeg", kSamplePngData),
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageWithMobileAggressiveEnabled) {
  options()->set_enable_aggressive_rewriters_for_mobile(true);
  AddFilter(RewriteOptions::kDelayImages);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResScript("http://test.com/1.jpeg", kSampleJpegData),
             GetHighResScript(), "</body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageMobileWithUrlValuedAttribute) {
  options()->set_enable_aggressive_rewriters_for_mobile(true);
  options()->AddUrlValuedAttribute("img", "data-src", semantic_type::kImage);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head><body>"
      "<img data-src=\"http://test.com/1.webp\"/>"
      "</body>";
  // No inlining.
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img data-src=\"http://test.com/1.webp\"/>",
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageWithMobileLazyLoad) {
  options()->set_enable_aggressive_rewriters_for_mobile(true);
  options()->set_lazyload_highres_images(true);
  AddFilter(RewriteOptions::kDelayImages);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResScript("http://test.com/1.jpeg", kSampleJpegData),
             GetLazyHighResScript(), "</body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayJpegImageOnInputElement) {
  options()->set_enable_aggressive_rewriters_for_mobile(true);
  AddFilter(RewriteOptions::kDelayImages);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<input type=\"image\" src=\"http://test.com/1.jpeg\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<input type=\"image\""
      " pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResScript("http://test.com/1.jpeg", kSampleJpegData),
             GetHighResScript(), "</body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, TestMinImageSizeLowResolutionBytesFlag) {
  options()->set_min_image_size_low_resolution_bytes(2 * 1024);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  // Size of 1.webp is 1780 and size of 1.jpeg is 6245. As
  // MinImageSizeLowResolutionBytes is set to 2 KB only jpeg low quality image
  // will be generated.
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\" />"
      "<img src=\"http://test.com/1.jpeg\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img src=\"http://test.com/1.webp\"/>",
      GenerateRewrittenImageTag("http://test.com/1.jpeg", kSampleJpegData),
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, TestMaxImageSizeLowResolutionBytesFlag) {
  options()->set_max_image_size_low_resolution_bytes(4 * 1024);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  // Size of 1.webp is 1780 and size of 1.jpeg is 6245. As
  // MaxImageSizeLowResolutionBytes is set to 4 KB only webp low quality image
  // will be generated.
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\" />"
      "<img src=\"http://test.com/1.jpeg\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      GenerateRewrittenImageTag("http://test.com/1.webp", kSampleWebpData),
      "<img src=\"http://test.com/1.jpeg\"/>",
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, TestMaxInlinedPreviewImagesIndexFlag) {
  options()->set_max_inlined_preview_images_index(1);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\" />"
      "<img src=\"http://test.com/1.webp\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      GenerateRewrittenImageTag("http://test.com/1.jpeg", kSampleJpegData),
      "<img src=\"http://test.com/1.webp\"/>",
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayMultipleSameImage) {
  options()->set_enable_aggressive_rewriters_for_mobile(true);
  AddFilter(RewriteOptions::kDelayImages);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeWebp, 100);

  // pagespeed_inline_map size will be 1. For same images, delay_images_filter
  // make only one entry in pagespeed_inline_map.
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\" />"
      "<img src=\"http://test.com/1.jpeg\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      "<img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResScript("http://test.com/1.jpeg", kSampleJpegData),
             GetHighResScript(), "</body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, NoHeadTag) {
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<body>"
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      "<body>",
      GetNoscript(),
      GenerateRewrittenImageTag("http://test.com/1.webp", kSampleWebpData),
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, PcacheMiss) {
  TestCriticalImagesFinder* finder =
      new TestCriticalImagesFinder(NULL, statistics());
  server_context()->set_critical_images_finder(finder);

  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head><body>"
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img src=\"http://test.com/1.webp\"/></body>");
  MatchOutputAndCountBytes(input_html, output_html);

  rewrite_driver_->log_record()->WriteLog();
  ScopedMutex lock(rewrite_driver()->log_record()->mutex());
  EXPECT_EQ(RewriterHtmlApplication::PROPERTY_CACHE_MISS,
            logging_info()->rewriter_stats(0).html_status());
  EXPECT_EQ("di", logging_info()->rewriter_stats(0).id());
}

TEST_F(DelayImagesFilterTest, MultipleBodyTags) {
  options()->set_enable_aggressive_rewriters_for_mobile(true);
  AddFilter(RewriteOptions::kDelayImages);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  AddFileToMockFetcher("http://test.com/2.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);

  // No change in the subsequent body tags.
  GoogleString input_html = "<head></head>"
      "<body><img src=\"http://test.com/1.webp\"/></body>"
      "<body><img src=\"http://test.com/2.jpeg\"/></body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img pagespeed_high_res_src=\"http://test.com/1.webp\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResScript("http://test.com/1.webp", kSampleWebpData),
             GetHighResScript(), "</body>"),
      "<body>",
      "<img pagespeed_high_res_src=\"http://test.com/2.jpeg\"/>",
      StrCat(GenerateAddLowResScript("http://test.com/2.jpeg", kSampleJpegData),
             GetHighResScript(), "</body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, MultipleFlushWindowsForExperimental) {
  options()->set_enable_aggressive_rewriters_for_mobile(true);
  AddFilter(RewriteOptions::kDelayImages);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  AddFileToMockFetcher("http://test.com/2.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);

  SetupWriter();
  html_parse()->StartParse("http://test.com/inline_preview_images.html");
  html_parse()->ParseText(
      "<head></head><body><img src=\"http://test.com/1.webp\"/>");
  html_parse()->Flush();
  html_parse()->ParseText("<img src=\"http://test.com/2.jpeg\"/></body>");
  html_parse()->FinishParse();

  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img pagespeed_high_res_src=\"http://test.com/1.webp\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResScript("http://test.com/1.webp",
                                     kSampleWebpData)),
      "<img pagespeed_high_res_src=\"http://test.com/2.jpeg\"/>",
      StrCat(GenerateAddLowResScript("http://test.com/2.jpeg", kSampleJpegData),
             GetHighResScript(), "</body>"));
  EXPECT_TRUE(Wildcard(output_html).Match(output_buffer_));
}

TEST_F(DelayImagesFilterTest, ResizeForResolution) {
  options()->EnableFilter(RewriteOptions::kDelayImages);
  options()->EnableFilter(RewriteOptions::kResizeMobileImages);
  options()->set_enable_aggressive_rewriters_for_mobile(false);
  rewrite_driver()->AddFilters();
  AddFileToMockFetcher("http://test.com/1.jpeg", kLargeJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      kHeadHtml,
      StrCat("<body>",
             GetNoscript(),
             "<img pagespeed_high_res_src=\"http://test.com/1.jpeg\" "),
      "src=\"", kSampleJpegData, "\"/>", "</body>");

  // Mobile output should be smaller than desktop because inlined low quality
  // image is resized smaller for mobile.
  // Do desktop and mobile rewriting twice. They should not affect each other.
  SetupUserAgentTest("Safari");
  int byte_count_desktop1 = MatchOutputAndCountBytes(input_html, output_html);

  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  int byte_count_android1 = MatchOutputAndCountBytes(input_html, output_html);
  EXPECT_LT(byte_count_android1, byte_count_desktop1);

  SetupUserAgentTest("MSIE 8.0");
  int byte_count_desktop2 = MatchOutputAndCountBytes(input_html, output_html);

  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidNexusSUserAgent);
  int byte_count_android2 = MatchOutputAndCountBytes(input_html, output_html);
  EXPECT_EQ(byte_count_android1, byte_count_android2);
  EXPECT_EQ(byte_count_desktop1, byte_count_desktop2);

  SetupUserAgentTest("iPhone OS");
  int byte_count_iphone = MatchOutputAndCountBytes(input_html, output_html);
  EXPECT_EQ(byte_count_iphone, byte_count_android1);
}

TEST_F(DelayImagesFilterTest, ResizeForResolutionWithSmallImage) {
  options()->EnableFilter(RewriteOptions::kDelayImages);
  options()->EnableFilter(RewriteOptions::kResizeMobileImages);
  rewrite_driver()->AddFilters();
  AddFileToMockFetcher("http://test.com/1.png", kSmallPngFile,
                       kContentTypePng, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.png\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      kHeadHtml,
      "<body>",
      GetNoscript(),
      "<img src=\"http://test.com/1.png\"/>"
      "</body>");

  // No low quality data for an image smaller than kDelayImageWidthForMobile
  // (in image_rewrite_filter.cc).
  rewrite_driver()->SetUserAgent(
      UserAgentMatcherTestBase::kAndroidICSUserAgent);
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, ResizeForResolutionNegative) {
  options()->set_enable_aggressive_rewriters_for_mobile(false);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.jpeg", kLargeJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      kHeadHtml,
      StrCat("<body>",
             GetNoscript(),
             "<img pagespeed_high_res_src=\"http://test.com/1.jpeg\" "),
      "src=\"", kSampleJpegData, "\"/>", "</body>");

  // If kResizeMobileImages is not explicitly enabled, desktop and mobile
  // outputs will have the same size.
  SetupUserAgentTest("Safari");
  int byte_count_desktop = MatchOutputAndCountBytes(input_html, output_html);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  int byte_count_mobile = MatchOutputAndCountBytes(input_html, output_html);
  EXPECT_EQ(byte_count_mobile, byte_count_desktop);
}

TEST_F(DelayImagesFilterTest, DelayImagesScriptOptimized) {
  options()->set_enable_aggressive_rewriters_for_mobile(true);
  AddFilter(RewriteOptions::kDelayImages);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  AddFileToMockFetcher("http://test.com/1.jpeg", kLargeJpgFile,
                       kContentTypeJpeg, 100);
  Parse("optimized",
        "<head></head><body><img src=\"http://test.com/1.jpeg\"/></body>");
  EXPECT_EQ(GoogleString::npos, output_buffer_.find("/*"))
      << "There should be no comments in the optimized code";
}

TEST_F(DelayImagesFilterTest, DelayImagesScriptDebug) {
  options()->EnableFilter(RewriteOptions::kDebug);
  options()->set_enable_aggressive_rewriters_for_mobile(true);
  AddFilter(RewriteOptions::kDelayImages);
  SetupUserAgentTest(UserAgentMatcherTestBase::kAndroidICSUserAgent);
  AddFileToMockFetcher("http://test.com/1.jpeg", kLargeJpgFile,
                       kContentTypeJpeg, 100);
  Parse("debug",
        "<head></head><body><img src=\"http://test.com/1.jpeg\"/></body>");
  EXPECT_EQ(GoogleString::npos, output_buffer_.find("/*"))
      << "There should be no comments in the debug code";
}

TEST_F(DelayImagesFilterTest, DelayImageBasicTest) {
  options()->DisableFilter(RewriteOptions::kInlineImages);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      GenerateRewrittenImageTag("http://test.com/1.webp", kSampleWebpData),
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageSizeLimitTest) {
  options()->DisableFilter(RewriteOptions::kInlineImages);
  // If the low res is small, the image is not inline previewed.
  options()->set_max_low_res_image_size_bytes(615);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageSizePercentageLimitTest) {
  options()->DisableFilter(RewriteOptions::kInlineImages);
  // If the low-res-size / full-res-size > 0.3, the image is not inline
  // previewed.
  options()->set_max_low_res_to_full_res_image_size_percentage(30);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body>",
      GetNoscript(),
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

}  // namespace net_instaweb
