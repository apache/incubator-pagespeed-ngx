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

#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
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

  // This image is displayed at 1/4 native resolution, we could add a srcset
  // to allow multiple resolutions, but instead we just inline the largest
  // version because even the largest one is pretty small.
  const char input_html[] = "<img src=small_16x16.png width=4 height=4>";
  GoogleString output_html =
      StrCat("<img width=4 height=4 src=\"", k16x16PngDataUrl, "\">");
  ValidateExpected("inline_small", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, InlineSmall2) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  rewrite_driver()->AddFilters();

  // Like InlineSmall test, but with 1/2 native resolution instead of 1/4.
  const char input_html[] = "<img src=small_16x16.png width=7 height=7>";
  GoogleString output_html =
      StrCat("<img width=7 height=7 src=\"", k16x16PngDataUrl, "\">");
  ValidateExpected("inline_small2", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, NoPartialInline) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  // Original image is 292 bytes, 4x4 resize is 83 bytes. So we choose a
  // value between.
  options()->set_image_inline_max_bytes(200);
  rewrite_driver()->AddFilters();

  const char input_html[] = "<img src=small_16x16.png width=4 height=4>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(4, 4, "small_16x16.png", "0", "png"),
      " width=4 height=4 srcset=\"",
      EncodeImage(8, 8, "small_16x16.png", "0", "png"), " 2x,",
      "small_16x16.png 4x\">");
  ValidateExpected("no_partial_inline", input_html, output_html);
}

TEST_F(ResponsiveImageFilterTest, NoPartialInline2) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  options()->EnableFilter(RewriteOptions::kInlineImages);
  // Original image is 292 bytes, 7x7 resize is 83 bytes. So we choose a
  // value between.
  options()->set_image_inline_max_bytes(200);
  rewrite_driver()->AddFilters();

  const char input_html[] = "<img src=small_16x16.png width=7 height=7>";
  GoogleString output_html = StrCat(
      "<img src=", EncodeImage(7, 7, "small_16x16.png", "0", "png"),
      " width=7 height=7 srcset=\"",
      EncodeImage(14, 14, "small_16x16.png", "0", "png"), " 2x,"
      "small_16x16.png 4x\">");
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
      StrCat("<script type=\"text/javascript\" pagespeed_no_defer>",
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

TEST_F(ResponsiveImageFilterTest, Debug) {
  options()->EnableFilter(RewriteOptions::kResponsiveImages);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  options()->EnableFilter(RewriteOptions::kResizeImages);
  rewrite_driver()->AddFilters();
  EnableDebug();

  ValidateExpected(
      "no_transform",
      "<img src=a.jpg width=100 height=100 pagespeed_no_transform>",

      "<img src=a.jpg width=100 height=100>"
      "<!--ResponsiveImageFilter: Not adding srcset because of "
      "pagespeed_no_transform attribute.-->");

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
      // First virtual image debug messages:
      StrCat("<!--ResponsiveImageFilter: Any debug messages after this refer "
             "to the virtual 2x image with src=",
             EncodeImage(2046, 1532, "a.jpg", "0", "jpg"),
             " width=2046 height=1532-->"
             "<!--Image does not appear to need resizing.-->"

             // Second virtual image debug messages:
             "<!--ResponsiveImageFilter: Any debug messages after this refer "
             "to the virtual 4x image with src=",
             EncodeImage(4092, 3064, "a.jpg", "0", "jpg"),
             " width=4092 height=3064-->"
             "<!--Image does not appear to need resizing.-->"

             // Third virtual image debug messages:
             "<!--ResponsiveImageFilter: Any debug messages after this refer "
             "to the virtual inlinable 4x image with src=",
             EncodeImage(4092, 3064, "a.jpg", "0", "jpg"),
             " width=4092 height=3064-->"
             "<!--Image does not appear to need resizing.-->"

             // Actual image + debug messages:
             "<img src=", EncodeImage(1023, 766, "a.jpg", "0", "jpg"),
             " width=1023 height=766>"
             "<!--ResponsiveImageFilter: Not adding 4x candidate to srcset "
             "because native image was not high enough resolution.-->"
             "<!--ResponsiveImageFilter: Not adding 2x candidate to srcset "
             "because native image was not high enough resolution.-->"
             "<!--Image does not appear to need resizing.-->"));

  ValidateExpected(
      "same_src",
      // Input
      "<img src=http://other-domain.com/a.jpg width=100 height=100>",

      // Expected output
      // First virtual image debug messages:
      "<!--ResponsiveImageFilter: Any debug messages after this refer "
      "to the virtual 2x image with "
      "src=http://other-domain.com/a.jpg width=200 height=200-->"
      "<!--The preceding resource was not rewritten because its domain "
      "(other-domain.com) is not authorized-->"

      // Second virtual image debug messages:
      "<!--ResponsiveImageFilter: Any debug messages after this refer "
      "to the virtual 4x image with "
      "src=http://other-domain.com/a.jpg width=400 height=400-->"
      "<!--The preceding resource was not rewritten because its domain "
      "(other-domain.com) is not authorized-->"

      // Third virtual image debug messages:
      "<!--ResponsiveImageFilter: Any debug messages after this refer "
      "to the virtual inlinable 4x image with "
      "src=http://other-domain.com/a.jpg width=400 height=400-->"
      "<!--The preceding resource was not rewritten because its domain "
      "(other-domain.com) is not authorized-->"

      // Actual image + debug messages:
      "<img src=http://other-domain.com/a.jpg width=100 height=100>"
      "<!--ResponsiveImageFilter: Not adding 4x candidate to srcset "
      "because it is the same as previous candidate.-->"
      "<!--ResponsiveImageFilter: Not adding 2x candidate to srcset "
      "because it is the same as previous candidate.-->"
      "<!--The preceding resource was not rewritten because its domain "
      "(other-domain.com) is not authorized-->");
}

}  // namespace

}  // namespace net_instaweb
