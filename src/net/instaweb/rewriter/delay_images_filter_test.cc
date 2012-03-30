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
#include "net/instaweb/rewriter/public/delay_images_filter.h"
#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"
#include "net/instaweb/rewriter/public/js_disable_filter.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/wildcard.h"

namespace {
const char kSampleJpgFile[] = "Sample.jpg";
const char kSampleWebpFile[] = "Sample_webp.webp";
const char kLargeJpgFile[] = "Puzzle.jpg";
const char kSmallPngFile[] = "BikeCrashIcn.png";

// Generated html is matched approximately because different versions of
// libjpeg are yeilding different low_res_image_data.
const char kSampleJpegData[] = "data:image/jpeg;base64*";
const char kSampleWebpData[] = "data:image/webp;base64*";

const char kHeadHtml[] = "<head></head>";

const char kHeadHtmlWithLazyloadTemplate[] =
    "<head>"
    "<script type=\"text/javascript\">"
    "%s"
    "\npagespeed.lazyLoadInit(false);\n"
    "</script></head>";

const char kInlineScriptTemplate[] =
    "<script type=\"text/javascript\">%s";

const char kScriptTemplate[] =
    "<script type=\"text/javascript\">%s</script>";

}  // namespace

namespace net_instaweb {

class DelayImagesFilterTest : public ResourceManagerTestBase {
 public:
  DelayImagesFilterTest()
      : kDisableDeferJsExperimentalString(StrCat(
            "<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
            net_instaweb::JsDisableFilter::kDisableJsExperimental,
            "</script>")) {
    options_->set_min_image_size_low_resolution_bytes(1 * 1024);
    options_->set_max_inlined_preview_images_index(-1);
  }

 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
  }

  // Match rewritten html content and return its byte count.
  int MatchOutputAndCountBytes(const GoogleString& html_input,
                               const GoogleString& expected) {
    Parse("inline_preview_images", html_input);
    GoogleString full_html = doctype_string_ + AddHtmlBody(expected);
    EXPECT_TRUE(Wildcard(full_html).Match(output_buffer_));
    int output_size = output_buffer_.size();
    output_buffer_.clear();
    return output_size;
  }

  GoogleString GenerateAddLowResString(const GoogleString& url,
                                       const GoogleString& image_data) {
    return StrCat("\npagespeed.delayImagesInline.addLowResImages(\'", url,
        "\', \'", image_data, "\');");
  }

  GoogleString GenerateRewrittenImageTag(const StringPiece& url) {
    return StrCat("<img pagespeed_lazy_src=\"", url, "\" src=\"",
                  LazyloadImagesFilter::kDefaultInlineImage, "\" onload=\"",
                  LazyloadImagesFilter::kImageOnloadCode, "\"/>");
  }

  GoogleString GetHeadHtmlWithLazyload() {
    return StringPrintf(kHeadHtmlWithLazyloadTemplate,
                        GetLazyloadImagesCode().c_str());
  }

  GoogleString GetInlineScript() {
    return StringPrintf(kInlineScriptTemplate,
                        GetDelayImagesInlineCode().c_str());
  }

  GoogleString GetDeferJs() {
    return StringPrintf(kScriptTemplate, GetDeferJsCode().c_str());
  }

  GoogleString GetDelayImages() {
    return StringPrintf(kScriptTemplate, GetDelayImagesCode().c_str());
  }

  GoogleString GetDeferJsCode() {
    return GetJsCode(StaticJavascriptManager::kDeferJs,
                     JsDeferDisabledFilter::kSuffix);
  }

  GoogleString GetDelayImagesCode() {
    return GetJsCode(StaticJavascriptManager::kDelayImagesJs,
                     DelayImagesFilter::kDelayImagesSuffix);
  }

  GoogleString GetDelayImagesInlineCode() {
    return GetJsCode(StaticJavascriptManager::kDelayImagesInlineJs,
                     DelayImagesFilter::kDelayImagesInlineSuffix);
  }

  GoogleString GetLazyloadImagesCode() {
    return resource_manager()->static_javascript_manager()->GetJsSnippet(
        StaticJavascriptManager::kLazyloadImagesJs, options());
  }

  GoogleString GetJsCode(StaticJavascriptManager::JsModule module,
                         const StringPiece& call) {
    StringPiece code =
        resource_manager()->static_javascript_manager()->GetJsSnippet(
            module, options());
    return StrCat(code, call);
  }

  const GoogleString kDisableDeferJsExperimentalString;
};

TEST_F(DelayImagesFilterTest, DelayImageWithDeferJavascriptDisabled) {
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\" />"
      "</body>";
  GoogleString output_html = StrCat(GetHeadHtmlWithLazyload(),
      "<body><img pagespeed_high_res_src=\"http://test.com/1.webp\" ",
      "src=\"", kSampleWebpData, "\"/>", GetDelayImages(), "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayImageWithLazyLoadDisabled) {
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\" />"
      "</body>";
  GoogleString output_html = StrCat(kHeadHtml,
      "<body>",
      kDisableDeferJsExperimentalString,
      StrCat("<img pagespeed_high_res_src=\"http://test.com/1.webp\" ",
             "src=\"", kSampleWebpData, "\"/>"),
      GetDelayImages(), GetDeferJs(), "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayWebPImage) {
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\" />"
      "<input src=\"http://test.com/1.webp\" type=\"image\"/>"
      "</body>";
  GoogleString output_html = StrCat(GetHeadHtmlWithLazyload(),
      "<body>",
      kDisableDeferJsExperimentalString,
      "<img pagespeed_high_res_src=\"http://test.com/1.webp\"/>",
      "<input src=\"http://test.com/1.webp\" type=\"image\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResString("http://test.com/1.webp", kSampleWebpData),
             "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script>",
             GetDelayImages(), GetDeferJs(), "</body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayJpegImage) {
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\" />"
      "</body>";
  GoogleString output_html = StrCat(GetHeadHtmlWithLazyload(),
      "<body>",
      kDisableDeferJsExperimentalString,
      "<img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResString("http://test.com/1.jpeg", kSampleJpegData),
             "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script>",
             GetDelayImages(), GetDeferJs(), "</body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, TestMinImageSizeLowResolutionBytesFlag) {
  options_->set_min_image_size_low_resolution_bytes(2 * 1024);
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
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
  GoogleString output_html = StrCat(GetHeadHtmlWithLazyload(),
      "<body>",
      kDisableDeferJsExperimentalString,
      GenerateRewrittenImageTag("http://test.com/1.webp"),
      "<img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResString("http://test.com/1.jpeg", kSampleJpegData),
             "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script>",
             GetDelayImages(), GetDeferJs(), "</body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, TestMaxInlinedPreviewImagesIndexFlag) {
  options_->set_max_inlined_preview_images_index(1);
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
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
  GoogleString output_html = StrCat(GetHeadHtmlWithLazyload(),
      "<body>",
      kDisableDeferJsExperimentalString,
      "<img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResString("http://test.com/1.jpeg", kSampleJpegData),
             "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script>",
             GetDelayImages(),
             GenerateRewrittenImageTag("http://test.com/1.webp"),
             GetDeferJs(), "</body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayMultipleSameImage) {
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);

  // pagespeed_inline_map size will be 1. For same images, delay_images_filter
  // make only one entry in pagespeed_inline_map.
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\" />"
      "<img src=\"http://test.com/1.webp\" />"
      "</body>";
  GoogleString output_html = StrCat(GetHeadHtmlWithLazyload(),
      "<body>",
      kDisableDeferJsExperimentalString,
      "<img pagespeed_high_res_src=\"http://test.com/1.webp\"/>"
      "<img pagespeed_high_res_src=\"http://test.com/1.webp\"/>",
      StrCat(GetInlineScript(),
             GenerateAddLowResString("http://test.com/1.webp", kSampleWebpData),
             "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script>",
             GetDelayImages(), GetDeferJs(), "</body>"));
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
      "<body><img pagespeed_high_res_src=\"http://test.com/1.webp\" ",
      "src=\"", kSampleWebpData, "\"/>", GetDelayImages(), "</body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, MultipleBodyTags) {
  options()->EnableFilter(RewriteOptions::kDeferJavascript);
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  AddFileToMockFetcher("http://test.com/2.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);

  // No change in the subsequent body tags.
  GoogleString input_html = "<head></head>"
      "<body><img src=\"http://test.com/1.webp\"/></body>"
      "<body><img src=\"http://test.com/2.jpeg\"/></body>";
  GoogleString output_html = StrCat(GetHeadHtmlWithLazyload(),
      "<body>",
      kDisableDeferJsExperimentalString,
      "<img pagespeed_high_res_src=\"http://test.com/1.webp\"/>",
      GetDeferJs(), "</body>",
      StrCat(GetInlineScript(),
             GenerateAddLowResString("http://test.com/1.webp", kSampleWebpData),
             "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script>",
             GetDelayImages(),"<body>",
             GenerateRewrittenImageTag("http://test.com/2.jpeg"),
             "</body>"));
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, ResizeForResolution) {
  options()->EnableFilter(RewriteOptions::kDelayImages);
  options()->EnableFilter(RewriteOptions::kResizeMobileImages);
  rewrite_driver()->AddFilters();
  AddFileToMockFetcher("http://test.com/1.jpeg", kLargeJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      kHeadHtml,
      "<body><img pagespeed_high_res_src=\"http://test.com/1.jpeg\" ",
      "src=\"", kSampleJpegData, "\"/>", GetDelayImages(), "</body>");

  // Mobile output should be smaller than desktop because inlined low quality
  // image is resized smaller for mobile.
  // Do desktop and mobile rewriting twice. They should not affect each other.
  rewrite_driver()->set_user_agent("Safari");
  int byte_count_desktop1 = MatchOutputAndCountBytes(input_html, output_html);
  rewrite_driver()->Clear();
  rewrite_driver()->set_user_agent("Android 3.1");
  int byte_count_android1 = MatchOutputAndCountBytes(input_html, output_html);
  EXPECT_LT(byte_count_android1, byte_count_desktop1);

  rewrite_driver()->Clear();
  rewrite_driver()->set_user_agent("MSIE 8.0");
  int byte_count_desktop2 = MatchOutputAndCountBytes(input_html, output_html);
  rewrite_driver()->Clear();
  rewrite_driver()->set_user_agent("Android 4");
  int byte_count_android2 = MatchOutputAndCountBytes(input_html, output_html);
  EXPECT_EQ(byte_count_android1, byte_count_android2);
  EXPECT_EQ(byte_count_desktop1, byte_count_desktop2);

  rewrite_driver()->Clear();
  rewrite_driver()->set_user_agent("iPhone OS");
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
      "<body>"
      "<img src=\"http://test.com/1.png\"/>"
      "</body>");

  // No low quality data for an image smaller than kDelayImageWidthForMobile
  // (in image_rewrite_filter.cc).
  rewrite_driver()->set_user_agent("Android 3.1");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, ResizeForResolutionNegative) {
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.jpeg", kLargeJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      kHeadHtml,
      "<body><img pagespeed_high_res_src=\"http://test.com/1.jpeg\" ",
      "src=\"", kSampleJpegData, "\"/>", GetDelayImages(), "</body>");

  // If kResizeMobileImages is not explicitly enabled, desktop and mobile
  // outputs will have the same size.
  rewrite_driver()->set_user_agent("Safari");
  int byte_count_desktop = MatchOutputAndCountBytes(input_html, output_html);
  rewrite_driver()->Clear();
  rewrite_driver()->set_user_agent("Android 3.1");
  int byte_count_mobile = MatchOutputAndCountBytes(input_html, output_html);
  EXPECT_EQ(byte_count_mobile, byte_count_desktop);
}

TEST_F(DelayImagesFilterTest, DelayImagesScriptOptimized) {
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.jpeg", kLargeJpgFile,
                       kContentTypeJpeg, 100);
  rewrite_driver()->set_user_agent("Safari");
  Parse("optimized",
        "<head></head><body><img src=\"http://test.com/1.jpeg\"</body>");
  EXPECT_EQ(GoogleString::npos, output_buffer_.find("/*"))
      << "There should be no comments in the optimized code";
}

TEST_F(DelayImagesFilterTest, DelayImagesScriptDebug) {
  options()->EnableFilter(RewriteOptions::kDebug);
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.jpeg", kLargeJpgFile,
                       kContentTypeJpeg, 100);
  rewrite_driver()->set_user_agent("Safari");
  Parse("debug",
        "<head></head><body><img src=\"http://test.com/1.jpeg\"</body>");
  EXPECT_NE(GoogleString::npos, output_buffer_.find("/*"))
      << "There should still be some comments in the debug code";
}

}  // namespace net_instaweb
