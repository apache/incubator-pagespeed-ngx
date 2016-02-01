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

#include "net/instaweb/rewriter/public/delay_images_filter.h"
#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"

namespace net_instaweb {

namespace {

const char kPuzzleJpgFile[] = "Puzzle.jpg";        // 1023 x 766
const char kCuppaPngFile[] = "Cuppa.png";          //   65 x  70
const char kIronChefGifFile[] = "IronChef2.gif";   //  192 x 256
const char k1x1GifFile[] = "another-blank.gif";    //    1 x   1
const char k16x16PngFile[] = "small_16x16.png";    //   16 x  16

static const char* k16x16PngDataUrl =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAA"
    "BGdBTUEAALGPC/xhBQAAAAFzUkdCAK7OHOkAAAAgY0hSTQAAeiYAAICEAAD6AAAAgOgAAH"
    "UwAADqYAAAOpgAABdwnLpRPAAAAAZQTFRFAAD/////e9yZLAAAAAFiS0dEAf8CLd4AAAAJ"
    "cEhZcwAAAEgAAABIAEbJaz4AAAAMSURBVAjXY2AgDQAAADAAAceqhY4AAAAldEVYdGRhdG"
    "U6Y3JlYXRlADIwMTUtMDMtMjVUMTg6MDA6MTctMDQ6MDCr+Rs2AAAAJXRFWHRkYXRlOm1v"
    "ZGlmeQAyMDE1LTAzLTI1VDE4OjAwOjE3LTA0OjAw2qSjigAAAABJRU5ErkJggg==";

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

    AddFileToMockFetcher(StrCat(kTestDomain, "small_16x16.png"), k16x16PngFile,
                         kContentTypePng, 100);
  }

  virtual bool AddHtmlTags() const { return false; }

  void TestSimple(int width, int height, StringPiece filename,
                  StringPiece full_density, StringPiece final_ext,
                  bool include_zoom_script) {
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
        EncodeImage(1.5 * width, 1.5 * height, filename, "0", final_ext),
        " 1.5x,",
        EncodeImage(2 * width, 2 * height, filename, "0", final_ext), " 2x,",
        EncodeImage(3 * width, 3 * height, filename, "0", final_ext), " 3x,");
    StrAppend(
        &output_html, EncodeImage(-1, -1, filename, "0", final_ext),
        " ", full_density, "x\">");
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
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  rewrite_driver()->AddFilters();

  TestSimple(100, 100, "a.jpg", "10.23", "jpg", false);
}

TEST_F(ResponsiveImageFilterTest, SimplePng) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  rewrite_driver()->AddFilters();

  TestSimple(10, 10, "b.png", "6.5", "png", false);
}

TEST_F(ResponsiveImageFilterTest, SimpleGif) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->EnableFilter(RewriteOptions::kConvertGifToPng);
  rewrite_driver()->AddFilters();

  TestSimple(10, 10, "c.gif", "19.2", "png", false);
}

TEST_F(ResponsiveImageFilterTest, SimpleWebp) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  rewrite_driver()->AddFilters();
  SetupForWebp();

  TestSimple(100, 100, "a.jpg", "10.23", "webp", false);
}

TEST_F(ResponsiveImageFilterTest, Zoom) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResponsiveImagesZoom);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  rewrite_driver()->AddFilters();

  // Add zoom script.
  TestSimple(100, 100, "a.jpg", "10.23", "jpg", true);
}

TEST_F(ResponsiveImageFilterTest, OddRatio) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  rewrite_driver()->AddFilters();
  SetupForWebp();

  // Important, only 2 digits after decimal.
  TestSimple(99, 99, "a.jpg", "10.33", "webp", false);
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
      "<img src=", EncodeImage(-1, -1, "a.jpg", "0", "jpg"),
      " width=1023 height=766>");
  // We do not add a srcset because this is already native size.
  ValidateExpected("recompress_larger", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, RecompressLarger2) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  // Note: This is 2/3 the native size of a.jpg.
  const char input_html[] = "<img src=a.jpg width=682 height=511>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(682, 511, "a.jpg", "0", "jpg"),
      " width=682 height=511 srcset=\"",
      EncodeImage(-1, -1, "a.jpg", "0", "jpg"), " 1.5x\">");
  // We do not add a 2x version because the 1.5x is already native size.
  ValidateExpected("recompress_larger2", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, RecompressLarger3) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  // Note: This is slightly less than 2/3 the native size of a.jpg.
  const char input_html[] = "<img src=a.jpg width=682 height=510>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(682, 510, "a.jpg", "0", "jpg"),
      " width=682 height=510 srcset=\"",
      EncodeImage(1023, 765, "a.jpg", "0", "jpg"), " 1.5x,",
      // Note this 2x version is actually only 1023x766.
      EncodeImage(-1, -1, "a.jpg", "0", "jpg"), " 2x\">");
  // TODO(sligocki): We shouldn't include the 1.5x version because it's so
  // close to the 2x version. Update this test when that is fixed.
  ValidateExpected("recompress_larger3", input_html, output_html);
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

TEST_F(ResponsiveImageFilterTest, InlineNativeSize) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();

  // This image is displayed at native resolution, therefore we will not add
  // a srcset to make it responsive. Instead we just inline it.
  const char input_html[] = "<img src=small_16x16.png width=16 height=16>";
  GoogleString output_html =
      StrCat("<img width=16 height=16 src=\"", k16x16PngDataUrl, "\">");
  ValidateExpected("inline_native", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, InlineSmall) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();

  // This image is displayed at 1/2 native resolution, we could add a srcset
  // to allow multiple resolutions, but instead we just inline the largest
  // version because even the largest one is pretty small.
  const char input_html[] = "<img src=small_16x16.png width=8 height=8>";
  GoogleString output_html =
      StrCat("<img width=8 height=8 src=\"", k16x16PngDataUrl, "\">");
  ValidateExpected("inline_small", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, InlineSmall2) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();

  // Like InlineSmall test, but with at an odd resolution.
  const char input_html[] = "<img src=small_16x16.png width=11 height=11>";
  GoogleString output_html =
      StrCat("<img width=11 height=11 src=\"", k16x16PngDataUrl, "\">");
  ValidateExpected("inline_small2", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, NoPartialInline) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  // Original image is 292 bytes, 8x8 resize is 83 bytes. So we choose a
  // value between.
  options()->set_image_inline_max_bytes(200);
  rewrite_driver()->AddFilters();

  const char input_html[] = "<img src=small_16x16.png width=8 height=8>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(8, 8, "small_16x16.png", "0", "png"),
      " width=8 height=8 srcset=\"",
      EncodeImage(12, 12, "small_16x16.png", "0", "png"), " 1.5x,"
      "small_16x16.png 2x\">");
  ValidateExpected("no_partial_inline", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, NoPartialInline2) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  // Original image is 292 bytes, 11x11 resize is 83 bytes. So we choose a
  // value between.
  options()->set_image_inline_max_bytes(200);
  rewrite_driver()->AddFilters();

  const char input_html[] = "<img src=small_16x16.png width=11 height=11>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(11, 11, "small_16x16.png", "0", "png"),
      " width=11 height=11 srcset=\"small_16x16.png 1.5x\">");
  ValidateExpected("no_partial_inline2", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, LocalStorageFilter) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->EnableFilter(RewriteOptions::kLocalStorageCache);
  rewrite_driver()->AddFilters();
  SetHtmlMimetype();

  GoogleString local_storage_cache_js =
      StrCat("<script type=\"text/javascript\" data-pagespeed-no-defer>",
             server_context()->static_asset_manager()->GetAsset(
                 StaticAssetEnum::LOCAL_STORAGE_CACHE_JS, options()),
             LocalStorageCacheFilter::kLscInitializer,
             "</script>");

  // Note: Currently images used by the responsive filter do not get
  // local storage attributes and thus cannot be saved into local storage.
  // If we figure out a way (and think it's worth the effort) to make these
  // work together, we will need to update this test.
  const char input_html[] = "<img src=small_16x16.png width=16 height=16>";
  GoogleString output_html =
      StrCat(local_storage_cache_js,
             "<img width=16 height=16 src=\"", k16x16PngDataUrl, "\">");
  ValidateExpected("local_storage", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, DataUrl) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  // Don't mess with data URLs.
  GoogleString input_html = StrCat("<img src=\"", k16x16PngDataUrl, "\">");
  ValidateNoChanges("data_url", input_html);
}

TEST_F(ResponsiveImageFilterTest, CommasInUrls) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  AddFileToMockFetcher(StrCat(kTestDomain, "comma,middle"), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  AddFileToMockFetcher(StrCat(kTestDomain, "comma,end,"), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  AddFileToMockFetcher(StrCat(kTestDomain, ",comma,begin"), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);

  // srcset added. Commas are allowed in the middle of URLs in srcsets.
  ValidateExpected(
      "comma_middle",
      "<img src='comma,middle' width=682 height=511>",
      StrCat("<img src='", EncodeImage(682, 511, "comma,middle", "0", "jpg"),
             "' width=682 height=511 srcset=\"comma,middle 1.5x\">"));

  // No srcset added. Commas are not allowed at end of URLs in srcset.
  ValidateExpected(
      "comma_end",
      "<img src='comma,end,' width=682 height=511>",
      StrCat("<img src='", EncodeImage(682, 511, "comma,end,", "0", "jpg"),
             "' width=682 height=511>"));

  // No srcset added. Commas are not allowed at beginning of URLs in srcset.
  ValidateExpected(
      "comma_begin",
      "<img src=',comma,begin' width=682 height=511>",
      StrCat("<img src='", EncodeImage(682, 511, ",comma,begin", "0", "jpg"),
             "' width=682 height=511>"));
}

TEST_F(ResponsiveImageFilterTest, SpacesInUrls) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  AddFileToMockFetcher(StrCat(kTestDomain, "space%20%20in%20%0C%20URL"),
                       kPuzzleJpgFile, kContentTypeJpeg, 100);

  // All whitespace chars should be escaped in srcset.
  ValidateExpected(
      "comma_middle",
      "<img src='space \t in \n\r\f URL' width=682 height=511>",
      StrCat("<img src='",
             EncodeImage(682, 511, "space%20%20in%20%0C%20URL", "0", "jpg"),
             "' width=682 height=511 "
             "srcset=\"space%20%09%20in%20%0A%0D%0C%20URL 1.5x\">"));
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

TEST_F(ResponsiveImageFilterTest, DataNoTransform) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  const char input_html[] =
      "<img src=a.jpg width=100 height=100 data-pagespeed-no-transform>";
  const char output_html[] = "<img src=a.jpg width=100 height=100>";
  ValidateExpected("data-no-transform", input_html, output_html);
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

TEST_F(ResponsiveImageFilterTest, CustomDensities) {
  RewriteOptions::ResponsiveDensities densities;
  EXPECT_TRUE(RewriteOptions::ParseFromString("2, 4.7, 0.5", &densities));
  options()->set_responsive_image_densities(densities);

  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();

  const char input_html[] = "<img src=a.jpg width=100 height=100>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(100, 100, "a.jpg", "0", "jpg"),
      " width=100 height=100 srcset=\"",
      // Note: Resolutions are sorted.
      EncodeImage(50, 50, "a.jpg", "0", "jpg"), " 0.5x,",
      EncodeImage(200, 200, "a.jpg", "0", "jpg"), " 2x,",
      EncodeImage(470, 470, "a.jpg", "0", "jpg"), " 4.7x,"
      "a.jpg 10.23x\">");
  ValidateExpected("custom_densities", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, Debug) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  EnableDebug();

  ValidateExpected(
      "no_transform",
      "<img src=a.jpg width=100 height=100 data-pagespeed-no-transform>",

      "<img src=a.jpg width=100 height=100>"
      "<!--ResponsiveImageFilter: Not adding srcset because of "
      "data-pagespeed-no-transform attribute.-->");

  ValidateExpected(
      "with_srcset",
      "<img src=a.jpg width=100 height=100 srcset='a.jpg 1x, b.png 2x'>",

      StrCat("<img src=", EncodeImage(100, 100, "a.jpg", "0", "jpg"),
             " width=100 height=100 srcset='a.jpg 1x, b.png 2x'>"
             "<!--Resized image from 1023x766 to 100x100-->"
             "<!--ResponsiveImageFilter: Not adding srcset because image "
             "already has one.-->"));

  ValidateExpected(
      "no_dims",
      "<img src=a.jpg>",

      StrCat("<img src=", EncodeImage(-1, -1, "a.jpg", "0", "jpg"), ">"
             "<!--Image does not appear to need resizing.-->"
             "<!--ResponsiveImageFilter: Not adding srcset because image does "
             "not have dimensions (or a src URL).-->"));

  ValidateExpected(
      "tracking_pixel",
      "<img src=small_1x1.gif width=1 height=1>",

      "<img src=small_1x1.gif width=1 height=1>"
      "<!--Image does not appear to need resizing.-->"
      "<!--ResponsiveImageFilter: Not adding srcset to tracking pixel.-->");

  ValidateExpected(
      "native_size",
      // Input
      "<img src=a.jpg width=1023 height=766>",

      // Expected output
      // 1.5x virtual image debug messages:
      StrCat("<!--ResponsiveImageFilter: Any debug messages after this refer "
             "to the virtual 1.5x image with src=",
             EncodeImage(-1, -1, "a.jpg", "0", "jpg"),
             " width=1534 height=1149-->"
             "<!--Image does not appear to need resizing.-->"

             // 2x virtual image debug messages:
             "<!--ResponsiveImageFilter: Any debug messages after this refer "
             "to the virtual 2x image with src=",
             EncodeImage(-1, -1, "a.jpg", "0", "jpg"),
             " width=2046 height=1532-->"
             "<!--Image does not appear to need resizing.-->"

             // 3x virtual image debug messages:
             "<!--ResponsiveImageFilter: Any debug messages after this refer "
             "to the virtual 3x image with src=",
             EncodeImage(-1, -1, "a.jpg", "0", "jpg"),
             " width=3069 height=2298-->"
             "<!--Image does not appear to need resizing.-->"

             // Inlinable virtual image debug messages:
             "<!--ResponsiveImageFilter: Any debug messages after this refer "
             "to the virtual inlinable 3x image with src=",
             EncodeImage(-1, -1, "a.jpg", "0", "jpg"),
             " width=3069 height=2298-->"
             "<!--Image does not appear to need resizing.-->"

             // Full virtual image debug messages:
             "<!--ResponsiveImageFilter: Any debug messages after this refer "
             "to the virtual full-sized image with src=",
             EncodeImage(-1, -1, "a.jpg", "0", "jpg"),
             " width= height=-->"
             "<!--Image does not appear to need resizing.-->"

             // Actual image + debug messages:
             "<img src=", EncodeImage(-1, -1, "a.jpg", "0", "jpg"),
             " width=1023 height=766>"
             "<!--ResponsiveImageFilter: Not adding 1x candidate to srcset "
             "because it is the same as previous candidate.-->"
             "<!--ResponsiveImageFilter: Not adding 3x candidate to srcset "
             "because it is the same as previous candidate.-->"
             "<!--ResponsiveImageFilter: Not adding 2x candidate to srcset "
             "because it is the same as previous candidate.-->"
             "<!--ResponsiveImageFilter: Not adding 1.5x candidate to srcset "
             "because it is the same as previous candidate.-->"
             "<!--Image does not appear to need resizing.-->"));

  ValidateExpected(
      "same_src",
      // Input
      "<img src=http://other-domain.com/a.jpg width=100 height=100>",

      // Expected output
      // 1.5x virtual image debug messages:
      "<!--ResponsiveImageFilter: Any debug messages after this refer "
      "to the virtual 1.5x image with "
      "src=http://other-domain.com/a.jpg width=150 height=150-->"
      "<!--The preceding resource was not rewritten because its domain "
      "(other-domain.com) is not authorized-->"

      // 2x virtual image debug messages:
      "<!--ResponsiveImageFilter: Any debug messages after this refer "
      "to the virtual 2x image with "
      "src=http://other-domain.com/a.jpg width=200 height=200-->"
      "<!--The preceding resource was not rewritten because its domain "
      "(other-domain.com) is not authorized-->"

      // 3x virtual image debug messages:
      "<!--ResponsiveImageFilter: Any debug messages after this refer "
      "to the virtual 3x image with "
      "src=http://other-domain.com/a.jpg width=300 height=300-->"
      "<!--The preceding resource was not rewritten because its domain "
      "(other-domain.com) is not authorized-->"

      // Inlinable virtual image debug messages:
      "<!--ResponsiveImageFilter: Any debug messages after this refer "
      "to the virtual inlinable 3x image with "
      "src=http://other-domain.com/a.jpg width=300 height=300-->"
      "<!--The preceding resource was not rewritten because its domain "
      "(other-domain.com) is not authorized-->"

      // Full virtual image debug messages:
      "<!--ResponsiveImageFilter: Any debug messages after this refer "
      "to the virtual full-sized image with "
      "src=http://other-domain.com/a.jpg width= height=-->"
      "<!--The preceding resource was not rewritten because its domain "
      "(other-domain.com) is not authorized-->"

      // Actual image + debug messages:
      "<img src=http://other-domain.com/a.jpg width=100 height=100>"
      "<!--ResponsiveImageFilter: Not adding 3x candidate to srcset "
      "because it is the same as previous candidate.-->"
      "<!--ResponsiveImageFilter: Not adding 2x candidate to srcset "
      "because it is the same as previous candidate.-->"
      "<!--ResponsiveImageFilter: Not adding 1.5x candidate to srcset "
      "because it is the same as previous candidate.-->"
      "<!--The preceding resource was not rewritten because its domain "
      "(other-domain.com) is not authorized-->");
}

TEST_F(ResponsiveImageFilterTest, InlinePreview) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  // inline_preview_images -> kDelayImages.
  options()->EnableFilter(RewriteOptions::kDelayImages);
  SetHtmlMimetype();  // Prevent insertion of CDATA tags to static JS.
  rewrite_driver()->AddFilters();

  const GoogleString kInlinePreviewScript = StrCat(
      "<script data-pagespeed-no-defer type=\"text/javascript\">",
      DelayImagesFilter::kImageOnloadJsSnippet, "</script>");
  const char kLowResSource[] = "data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAA"
      "AQABAAD/2wBDAFA3PEY8MlBGQUZaVVBfeMiCeG5uePWvuZHI////////////////////"
      "////////////////////////////////2wBDAVVaWnhpeOuCguv/////////////////"
      "////////////////////////////////////////////////////////wAARCABkAGQD"
      "ASIAAhEBAxEB/8QAGQABAQEBAQEAAAAAAAAAAAAAAAECAwQF/8QAKRAAAgIBAwQABQUA"
      "AAAAAAAAAAECEQMhMUEEElFhcYGRofAUI0LR4f/EABYBAQEBAAAAAAAAAAAAAAAAAAAB"
      "Av/EABYRAQEBAAAAAAAAAAAAAAAAAAABEf/aAAwDAQACEQMRAD8A9oBLSVgUEKAAAAAg"
      "FBwfURU3Fp0uTrGcZq4tMDQIZjkjNtRlbQGwABynkrRHCeRtVwJSM72Z1qR1xZe3R7Ho"
      "TTVo8RuGSUNvuXSx6wefDmlPI4y8HoKy5ZpuELjued9RNrVr6DqcjeSk9EcQK23JkTad"
      "p17M2RgdJZpyVOTozGTjJSi6aMgD6GPqISinJqL5QPngDvuVEQMtqAUCwl2yUvB3nkXZ"
      "e2h5XLtklydOobUa8ljNeeUr1vcw9Q7RCoaotoWAJsUi3oq0YAHtw4scsad2AOJeSSfb"
      "qE01a1Mtr4JKXZG+S2tzhOXfL0BE252z1dTN9sEktVex5Unex3yu8UH6o0y4Mhrfgy1q"
      "EUiBQKmBHcbegKvr8waUJSVpfYAdckaRiGKcXxXtnqy429Uc6vVMlWMOHctWVYorg6KD"
      "9lWOVf2RWFFJ6JGc8f2o6cnoWP2cupgli08lSvJ+asya/NjPJUC8EKtv9AI3jg5zUUbw"
      "41PTVa736PVhwrEn5YG4RUIqK2QNACCl4BQIUAAZnBTjTNADiumx8pv4sj6XG/K+DO4A"
      "8/6SPEmZfSX/AD+x6gBwxYHjknafyO4AAAAQoAAAAAAAAAAAAAAAAAH/2Q==";

  const char input_html[] = "<img src=a.jpg width=100 height=100>";
  GoogleString output_html = StrCat(
      kInlinePreviewScript,
      "<img data-pagespeed-high-res-src=",
      EncodeImage(100, 100, "a.jpg", "0", "jpg"),
      " width=100 height=100 data-pagespeed-high-res-srcset=\"",
      EncodeImage(150, 150, "a.jpg", "0", "jpg"), " 1.5x,",
      EncodeImage(200, 200, "a.jpg", "0", "jpg"), " 2x,",
      EncodeImage(300, 300, "a.jpg", "0", "jpg"), " 3x,"
      "a.jpg 10.23x\" src=\"", kLowResSource,
      "\" onload=\"pagespeed.switchToHighResAndMaybeBeacon(this);\" onerror=\""
      "this.onerror=null;pagespeed.switchToHighResAndMaybeBeacon(this);\">");
  ValidateExpected("inline_preview", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, Lazyload) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kLazyloadImages);
  // Disable beaconing so that the image is automatically lazyloaded.
  options()->set_critical_images_beacon_enabled(false);
  // Set User-Agent so that Lazyload will work.
  SetCurrentUserAgent(
      UserAgentMatcherTestBase::kChrome18UserAgent);
  SetHtmlMimetype();  // Prevent insertion of CDATA tags to static JS.
  rewrite_driver()->AddFilters();

  const char input_html[] = "<img src=a.jpg width=100 height=100>";
  GoogleString output_html = StrCat(
      GetLazyloadScriptHtml(),
      "<img data-pagespeed-lazy-src=",
      EncodeImage(100, 100, "a.jpg", "0", "jpg"),
      " width=100 height=100 data-pagespeed-lazy-srcset=\"",
      EncodeImage(150, 150, "a.jpg", "0", "jpg"), " 1.5x,",
      EncodeImage(200, 200, "a.jpg", "0", "jpg"), " 2x,",
      EncodeImage(300, 300, "a.jpg", "0", "jpg"), " 3x,"
      "a.jpg 10.23x\" src=\"/psajs/1.0.gif\" "
      "onload=\"pagespeed.lazyLoadImages.loadIfVisibleAndMaybeBeacon(this);\" "
      "onerror=\"this.onerror=null;pagespeed.lazyLoadImages."
      "loadIfVisibleAndMaybeBeacon(this);\">");
  ValidateExpected("lazyload", input_html, output_html);
}

}  // namespace

}  // namespace net_instaweb
