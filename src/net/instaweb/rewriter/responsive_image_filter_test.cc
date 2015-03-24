/*
 * Copyright 2015 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/responsive_image_filter.h"

#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

namespace {

const char kPuzzleJpgFile[] = "Puzzle.jpg";        // 1023 x 766
const char kCuppaPngFile[] = "Cuppa.png";          //   65 x  70
const char kIronChefGifFile[] = "IronChef2.gif";   //  192 x 256
const char k1x1GifFile[] = "another-blank.gif";    //    1 x   1
const char k2x2GifFile[] = "small_2x2.gif";        //    2 x   2

static const char* k2x2GifDataUrl =
    "data:image/gif;base64,R0lGODlhAgACAPEAAP8AAP8AAP8AAP8AACH5BAAAAA"
    "AALAAAAAACAAIAAAIDRDQFADs=";

class ResponsiveImageFilterTest : public RewriteTestBase {
 protected:
  ResponsiveImageFilterTest() {
    AddFileToMockFetcher(StrCat(kTestDomain, "a.jpg"), kPuzzleJpgFile,
                         kContentTypeJpeg, 100);

    AddFileToMockFetcher(StrCat(kTestDomain, "b.png"), kCuppaPngFile,
                         kContentTypePng, 100);

    AddFileToMockFetcher(StrCat(kTestDomain, "c.gif"), kIronChefGifFile,
                         kContentTypeGif, 100);

    AddFileToMockFetcher(StrCat(kTestDomain, "small_1x1.gif"), k1x1GifFile,
                         kContentTypeGif, 100);

    AddFileToMockFetcher(StrCat(kTestDomain, "small_2x2.gif"), k2x2GifFile,
                         kContentTypeGif, 100);
  }

  virtual bool AddHtmlTags() const { return false; }

  void TestSimple(int width, int height, StringPiece filename,
                  StringPiece final_ext, bool include_zoom_script) {
    GoogleString width_str = IntegerToString(width);
    GoogleString height_str = IntegerToString(height);
    GoogleString input_html = StrCat(
        "<img src=", filename, " width=", width_str, " height=", height_str,
        ">");
    GoogleString output_html = StrCat(
        "<img src=", EncodeImage(width, height, filename, "0", final_ext),
        " width=", width_str, " height=", height_str);
    StrAppend(
        &output_html, " srcset=\"",
        EncodeImage(width, height, filename, "0", final_ext), " 1x,",
        EncodeImage(2 * width, 2 * height, filename, "0", final_ext), " 2x,",
        EncodeImage(4 * width, 4 * height, filename, "0", final_ext), " 4x\">");
    if (include_zoom_script) {
      StrAppend(&output_html,
                "<script src=\"/psajs/responsive.0.js\"></script>");
    }
    ValidateExpected("test_simple", input_html, output_html);
  }
};

TEST_F(ResponsiveImageFilterTest, SimpleJpg) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  TestSimple(100, 100, "a.jpg", "jpg", false);
}

TEST_F(ResponsiveImageFilterTest, SimplePng) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();

  TestSimple(10, 10, "b.png", "png", false);
}

TEST_F(ResponsiveImageFilterTest, SimpleGif) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  rewrite_driver()->AddFilters();

  TestSimple(10, 10, "c.gif", "png", false);
}

TEST_F(ResponsiveImageFilterTest, SimpleWebp) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  rewrite_driver()->AddFilters();
  rewrite_driver()->SetUserAgent("webp");

  TestSimple(100, 100, "a.jpg", "webp", false);
}

TEST_F(ResponsiveImageFilterTest, Zoom) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResponsiveImagesZoom);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  // Add zoom script.
  TestSimple(100, 100, "a.jpg", "jpg", true);
}

// Nothing happens if we do not enable image resizing.
TEST_F(ResponsiveImageFilterTest, NoResizeImages) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  rewrite_driver()->AddFilters();

  const char input_html[] = "<img src=a.jpg width=100 height=100>";
  ValidateNoChanges("no_resize_images", input_html);
}

TEST_F(ResponsiveImageFilterTest, NoResizeLarger) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  // Note: This is the native size of a.jpg.
  const char input_html[] = "<img src=a.jpg width=1023 height=766>";
  // We do not add a srcset because this is already native size.
  ValidateNoChanges("no_resize_larger", input_html);
}

TEST_F(ResponsiveImageFilterTest, RecompressLarger) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  // Note: This is the native size of a.jpg.
  const char input_html[] = "<img src=a.jpg width=1023 height=766>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(1023, 766, "a.jpg", "0", "jpg"),
      " width=1023 height=766>");
  // We do not add a srcset because this is already native size.
  ValidateExpected("recompress_larger", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, RecompressLarger2) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  // Note: This is half the native size of a.jpg.
  const char input_html[] = "<img src=a.jpg width=512 height=383>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(512, 383, "a.jpg", "0", "jpg"),
      " width=512 height=383 srcset=\"",
      EncodeImage(512, 383, "a.jpg", "0", "jpg"), " 1x,",
      EncodeImage(1024, 766, "a.jpg", "0", "jpg"), " 2x\">");
  // We do not add a 4x version because the 2x is already native size.
  ValidateExpected("recompress_larger2", input_html, output_html);
}

// Do not do any responsive rewriting if there are no dimensions.
TEST_F(ResponsiveImageFilterTest, NoDims) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  const char input_html[] = "<img src=a.jpg>";
  ValidateNoChanges("no_dims", input_html);
}

// Do not do any responsive rewriting if image dimensions are inserted.
// This triggered an early bug where the filter thought that it was in the
// first pass during the second pass.
TEST_F(ResponsiveImageFilterTest, InsertDims) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInsertImageDimensions);
  rewrite_driver()->AddFilters();

  const char input_html[] = "<img src=a.jpg>";
  const char output_html[] = "<img src=a.jpg width=\"1023\" height=\"766\">";
  ValidateExpected("insert_dims", input_html, output_html);
}

// Do not do any responsive rewriting if there is only one dimension.
// TODO(sligocki): Maybe we should allow rewriting with one dim. Seems like
// it would work fine.
TEST_F(ResponsiveImageFilterTest, OneDim) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  const char input_html[] = "<img src=a.jpg width=100>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(100, -1, "a.jpg", "0", "jpg"), " width=100>");
  ValidateExpected("one_dim", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, Inlining) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();

  // Don't add srcset if URL is inlined.
  const char input_html[] = "<img src=small_2x2.gif width=100 height=100>";
  GoogleString output_html =
      StrCat("<img src=", k2x2GifDataUrl, " width=100 height=100>");
  ValidateExpected("inline", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, DataUrl) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  // Don't mess with data URLs.
  GoogleString input_html = StrCat("<img src=\"", k2x2GifDataUrl, "\">");
  ValidateNoChanges("data_url", input_html);
}

TEST_F(ResponsiveImageFilterTest, CommaInUrl) {  // + Spaces in URL.
}

TEST_F(ResponsiveImageFilterTest, NoTransform) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  const char input_html[] =
      "<img src=a.jpg width=100 height=100 pagespeed_no_transform>";
  const char output_html[] = "<img src=a.jpg width=100 height=100>";
  ValidateExpected("no_transform", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, TrackingPixel) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  // Don't add srcset for 1x1 tracking pixels.
  const char input_html[] = "<img src=small_1x1.gif width=1 height=1>";
  ValidateNoChanges("tracking_pixel", input_html);
}

TEST_F(ResponsiveImageFilterTest, InputSrcSet) {
    options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  const char input_html[] =
      "<img src=a.jpg width=100 height=100 srcset='a.jpg 1x, b.png 2x'>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(100, 100, "a.jpg", "0", "jpg"),
      " width=100 height=100 srcset='a.jpg 1x, b.png 2x'>");
  ValidateExpected("input_srcset", input_html, output_html);
}

}  // namespace

}  // namespace net_instaweb
