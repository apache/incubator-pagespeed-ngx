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
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
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

}  // namespace

namespace net_instaweb {

class DelayImagesFilterTest : public ResourceManagerTestBase {
 public:
  DelayImagesFilterTest() {
    head_html_ = StrCat("<head><script type=\"text/javascript\">",
                        DelayImagesFilter::kDelayScript,
                        "\npagespeed.delayImagesInit()</script></head>");
    inline_script_ = StrCat("<script type=\"text/javascript\">",
                            DelayImagesFilter::kInlineScript,
                            "\npagespeed.delayImagesInlineInit();");
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

  GoogleString head_html_;
  GoogleString inline_script_;
};

TEST_F(DelayImagesFilterTest, DelayWebPImage) {
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\" />"
      "</body>";
  GoogleString output_html = StrCat(head_html_,
      "<body><img pagespeed_high_res_src=\"http://test.com/1.webp\"/>",
      inline_script_,
      GenerateAddLowResString("http://test.com/1.webp", kSampleWebpData),
      "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script></body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayJpegImage) {
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\" />"
      "</body>";
  GoogleString output_html = StrCat(head_html_,
      "<body><img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      inline_script_,
      GenerateAddLowResString("http://test.com/1.jpeg", kSampleJpegData),
      "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script></body>");
  MatchOutputAndCountBytes(input_html, output_html);
}


TEST_F(DelayImagesFilterTest, DelayMultipleSameImage) {
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
  GoogleString output_html = StrCat(head_html_,
      "<body><img pagespeed_high_res_src=\"http://test.com/1.webp\"/>"
      "<img pagespeed_high_res_src=\"http://test.com/1.webp\"/>",
      inline_script_,
      GenerateAddLowResString("http://test.com/1.webp", kSampleWebpData),
      "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script></body>");
  MatchOutputAndCountBytes(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, NoHeadTag) {
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<body>"
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>";
  // No change in the html if script is not inserted in the head.
  ValidateExpected("delay_images", input_html, input_html);
}

TEST_F(DelayImagesFilterTest, MultipleBodyTags) {
  AddFilter(RewriteOptions::kDelayImages);
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  AddFileToMockFetcher("http://test.com/2.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);

  // No change in the subsequent body tags.
  GoogleString input_html = "<head></head>"
      "<body><img src=\"http://test.com/1.webp\"/></body>"
      "<body><img src=\"http://test.com/2.jpeg\"/></body>";
  GoogleString output_html = StrCat(head_html_,
      "<body><img pagespeed_high_res_src=\"http://test.com/1.webp\"/>",
      inline_script_,
      GenerateAddLowResString("http://test.com/1.webp", kSampleWebpData),
      "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script></body>",
      "<body><img src=\"http://test.com/2.jpeg\"/></body>");
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
      head_html_,
      "<body><img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      inline_script_,
      GenerateAddLowResString("http://test.com/1.jpeg", kSampleJpegData),
      "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script></body>");

  // Mobile output should be smaller than desktop because inlined low quality
  // image is resized smaller for mobile.
  // Do desktop and mobile rewriting twice. They should not affect each other.
  rewrite_driver()->set_user_agent("Safari");
  int byte_count_desktop1 = MatchOutputAndCountBytes(input_html, output_html);
  rewrite_driver()->set_user_agent("Android 3.1");
  int byte_count_android1 = MatchOutputAndCountBytes(input_html, output_html);
  EXPECT_LT(byte_count_android1, byte_count_desktop1);

  rewrite_driver()->set_user_agent("MSIE 8.0");
  int byte_count_desktop2 = MatchOutputAndCountBytes(input_html, output_html);
  rewrite_driver()->set_user_agent("Android 4");
  int byte_count_android2 = MatchOutputAndCountBytes(input_html, output_html);
  EXPECT_EQ(byte_count_android1, byte_count_android2);
  EXPECT_EQ(byte_count_desktop1, byte_count_desktop2);

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
      head_html_,
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
      head_html_,
      "<body><img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>",
      inline_script_,
      GenerateAddLowResString("http://test.com/1.jpeg", kSampleJpegData),
      "\npagespeed.delayImagesInline.replaceWithLowRes();\n</script></body>");

  // If kResizeMobileImages is not explicitly enabled, desktop and mobile
  // outputs will have the same size.
  rewrite_driver()->set_user_agent("Safari");
  int byte_count_desktop = MatchOutputAndCountBytes(input_html, output_html);
  rewrite_driver()->set_user_agent("Android 3.1");
  int byte_count_mobile = MatchOutputAndCountBytes(input_html, output_html);
  EXPECT_EQ(byte_count_mobile, byte_count_desktop);
}
}  // namespace net_instaweb
