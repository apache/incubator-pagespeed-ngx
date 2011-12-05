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

// Generated html is matched approximately because different versions of
// libjpeg are yeilding different low_res_image_data.
const char kSampleJpegData[] = "data:image/jpeg;base64*";
const char kSampleWebpData[] = "data:image/webp;base64*";
}  // namespace

namespace net_instaweb {

class DelayImagesFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    AddFilter(RewriteOptions::kDelayImages);
  }

  void TestOutput(const GoogleString& html_input, const GoogleString& expected) {
    Parse("delay_images", html_input);
    GoogleString full_html = doctype_string_ + AddHtmlBody(expected);
    EXPECT_TRUE(Wildcard(full_html).Match(output_buffer_));
    output_buffer_.clear();
  }
};

TEST_F(DelayImagesFilterTest, DelayWebPImage) {
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head><script type=\"text/javascript\">",
      DelayImagesFilter::kDelayScript, "</script></head>"
      "<body><img pagespeed_high_res_src=\"http://test.com/1.webp\"/>"
      "<script type=\"text/javascript\">var pagespeed_inline_map = {};"
      "\npagespeed_inline_map[\'http://test.com/1.webp\'] = \'",
      kSampleWebpData, "\';\n",
      DelayImagesFilter::kShowInlineScript, "</script></body>");
  TestOutput(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, DelayJpegImage) {
  AddFileToMockFetcher("http://test.com/1.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head><script type=\"text/javascript\">",
      DelayImagesFilter::kDelayScript, "</script></head>"
      "<body><img pagespeed_high_res_src=\"http://test.com/1.jpeg\"/>"
      "<script type=\"text/javascript\">var pagespeed_inline_map = {};"
      "\npagespeed_inline_map[\'http://test.com/1.jpeg\'] = \'",
      kSampleJpegData, "\';\n",
      DelayImagesFilter::kShowInlineScript, "</script></body>");
  TestOutput(input_html, output_html);
}


TEST_F(DelayImagesFilterTest, DelayMultipleSameImage) {
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);

  // pagespeed_inline_map size will be 1. For same images, delay_images_filter
  // make only one entry in pagespeed_inline_map.
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.webp\" />"
      "<img src=\"http://test.com/1.webp\" />"
      "</body>";
  GoogleString output_html = StrCat(
      "<head><script type=\"text/javascript\">",
      DelayImagesFilter::kDelayScript, "</script></head>"
      "<body><img pagespeed_high_res_src=\"http://test.com/1.webp\"/>"
      "<img pagespeed_high_res_src=\"http://test.com/1.webp\"/>"
      "<script type=\"text/javascript\">var pagespeed_inline_map = {};"
      "\npagespeed_inline_map[\'http://test.com/1.webp\'] = \'",
      kSampleWebpData, "\';\n",
      DelayImagesFilter::kShowInlineScript, "</script></body>");
  TestOutput(input_html, output_html);
}

TEST_F(DelayImagesFilterTest, NoHeadTag) {
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  GoogleString input_html = "<body>"
      "<img src=\"http://test.com/1.webp\"/>"
      "</body>";
  // No change in the html if script is not inserted in the head.
  ValidateExpected("delay_images", input_html, input_html);
}

TEST_F(DelayImagesFilterTest, MultipleBodyTags) {
  AddFileToMockFetcher("http://test.com/1.webp", kSampleWebpFile,
                       kContentTypeWebp, 100);
  AddFileToMockFetcher("http://test.com/2.jpeg", kSampleJpgFile,
                       kContentTypeJpeg, 100);

  // No change in the subsequent body tags.
  GoogleString input_html = "<head></head>"
      "<body><img src=\"http://test.com/1.webp\"/></body>"
      "<body><img src=\"http://test.com/2.jpeg\"/></body>";
  GoogleString output_html = StrCat(
      "<head><script type=\"text/javascript\">",
      DelayImagesFilter::kDelayScript, "</script></head>"
      "<body><img pagespeed_high_res_src=\"http://test.com/1.webp\"/>"
      "<script type=\"text/javascript\">var pagespeed_inline_map = {};"
      "\npagespeed_inline_map[\'http://test.com/1.webp\'] = \'",
      kSampleWebpData, "\';\n",
      DelayImagesFilter::kShowInlineScript, "</script></body>"
      "<body><img src=\"http://test.com/2.jpeg\"/></body>");
  TestOutput(input_html, output_html);
}

}  // namespace net_instaweb
