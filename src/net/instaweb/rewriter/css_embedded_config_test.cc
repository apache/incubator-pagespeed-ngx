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

// Author: jmarantz@google.com (Joshua Marantz)

#include <cstddef>

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kBikePngFile[] = "BikeCrashIcn.png";
const char kCuppaPngFile[] = "Cuppa.png";
const char kDummyContent[] = "Invalid PNG but it does not matter for this test";
const char kPuzzleJpgFile[] = "Puzzle.jpg";
const char kEmbedCss[] = "embed.css";

}  // namespace

namespace net_instaweb {

// Test infrastructure for css files with encoded options.   Note that all
// the image-related options can affect the output hash of the CSS
// file.
//
// Note that it is not practical to test the hash of the images or the
// CSS that references them because the image algorithms produce
// different bits on different platforms.
class CssEmbeddedConfigTest : public CssRewriteTestBase {
 protected:
  virtual void SetUp() {
    // Don't call CssRewriteTestBase::SetUp() here because that calls AddFilter
    // and makes it inconvenient for us to add more.  Instead each test method
    // should call AddFilterAndSetup.
    options()->set_add_options_to_urls(true);

    SetResponseWithDefaultHeaders("foo.png", kContentTypePng, kDummyContent,
                                  100);
    css_url_ = StrCat(kTestDomain, kEmbedCss);
    html_input_ = MakeHtmlWithExternalCssLink(css_url_, 0);
  }

  // Rewrites an image in a CSS file and returns the resulting CSS filename.
  GoogleString RewriteImageInCss(StringPiece image_url_in) {
    SetResponseWithDefaultHeaders(css_url_, kContentTypeCss,
                                  MakeIndentedCssWithImage(image_url_in), 300);

    ParseUrl(StrCat(kTestDomain, "embed_config.html"), html_input_);
    StringVector css_links;
    CollectCssLinks("embedded_config", output_buffer_, &css_links);
    if (css_links.size() != 1) {
      return "";
    }
    return css_links[0];
  }

  GoogleString ExtractImageFromCssFilename(StringPiece css_link) {
    // Fetch the resultant CSS file.
    GoogleString css_out;
    ResponseHeaders css_headers, image_headers;
    ClearStats();
    EXPECT_TRUE(FetchResourceUrl(css_link, &css_out, &css_headers));
    EXPECT_EQ(1, lru_cache()->num_hits());
    EXPECT_EQ(0, lru_cache()->num_misses());
    ClearStats();

    // Find the image URL in the css output.
    size_t pos = css_out.find("url(");
    if (pos != GoogleString::npos) {
      pos += 4;  // skip past "url(".
      size_t end_pos = css_out.find(")", pos);
      if (end_pos != GoogleString::npos) {
        return css_out.substr(pos, end_pos - pos);
      }
    }
    return "";
  }

  GoogleString FetchImageFromCache(StringPiece image_url) {
    ResponseHeaders image_headers;
    GoogleString image;
    EXPECT_TRUE(FetchResourceUrl(image_url, &image, &image_headers));
    EXPECT_EQ(1, lru_cache()->num_hits());
    EXPECT_EQ(0, lru_cache()->num_misses());
    return image;
  }

  void AddFilterAndSetup(RewriteOptions::Filter filter) {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(filter);
    CssRewriteTestBase::SetUp();
  }

  GoogleString EncodedImageUrl(StringPiece image_name,
                               StringPiece ext,
                               StringPiece option_segment) {
    GoogleString image_url = Encode(kTestDomain, "ic", "0", image_name, ext);
    return AddOptionsToEncodedUrl(image_url, option_segment);
  }

  GoogleString EncodedCssUrl(StringPiece option_segment) {
    GoogleString css_link = Encode(kTestDomain, "cf", "0", kEmbedCss, "css");
    return AddOptionsToEncodedUrl(css_link, option_segment);
  }

  GoogleString css_url_;
  GoogleString html_input_;
};

TEST_F(CssEmbeddedConfigTest, CacheExtend) {
  AddFilterAndSetup(RewriteOptions::kExtendCacheImages);
  GoogleString css_link = RewriteImageInCss("foo.png");
  EXPECT_STREQ(EncodedCssUrl("ei"), css_link);
  GoogleString image_url = ExtractImageFromCssFilename(css_link);
  EXPECT_STREQ(Encode(kTestDomain, "ce", "0", "foo.png", "png"),
               image_url);
  EXPECT_STREQ(kDummyContent, FetchImageFromCache(image_url));
}

TEST_F(CssEmbeddedConfigTest, RewriteJpeg) {
  options()->set_image_jpeg_recompress_quality(81);
  AddFilterAndSetup(RewriteOptions::kRecompressJpeg);
  AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString css_link = RewriteImageInCss(kPuzzleJpgFile);
  EXPECT_STREQ(EncodedCssUrl("rj+iq=81"), css_link);
  GoogleString image_url = ExtractImageFromCssFilename(css_link);
  EXPECT_STREQ(EncodedImageUrl(kPuzzleJpgFile, "jpg", "rj+iq=81"),
               image_url);
  EXPECT_GE(103704, FetchImageFromCache(image_url).size());
}

TEST_F(CssEmbeddedConfigTest, RewriteJpegProgressive) {
  options()->set_image_jpeg_recompress_quality(81);
  options()->EnableFilter(RewriteOptions::kConvertJpegToProgressive);
  AddFilterAndSetup(RewriteOptions::kRecompressJpeg);
  AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString css_link = RewriteImageInCss(kPuzzleJpgFile);
  EXPECT_STREQ(EncodedCssUrl("jp+rj+iq=81"), css_link);
  GoogleString image_url = ExtractImageFromCssFilename(css_link);
  EXPECT_STREQ(EncodedImageUrl(kPuzzleJpgFile, "jpg", "jp+rj+iq=81"),
               image_url);
  EXPECT_GE(100349, FetchImageFromCache(image_url).size());
}

TEST_F(CssEmbeddedConfigTest, InlineImageToCss) {
  options()->set_css_image_inline_max_bytes(2048);
  AddFilterAndSetup(RewriteOptions::kInlineImages);
  AddFileToMockFetcher(StrCat(kTestDomain, kCuppaPngFile), kCuppaPngFile,
                       kContentTypePng, 100);
  GoogleString css_link = RewriteImageInCss(kCuppaPngFile);
  EXPECT_STREQ(EncodedCssUrl("ii+cii=2048"), css_link);
  GoogleString image_url = ExtractImageFromCssFilename(css_link);
  EXPECT_TRUE(StringPiece(image_url).starts_with("data:image/png;base64,"));
}

TEST_F(CssEmbeddedConfigTest, InlineImageToCssSmallThresholdExtend) {
  options()->set_css_image_inline_max_bytes(5);  // prevents inlining cuppa.png
  options()->EnableFilter(RewriteOptions::kExtendCacheImages);
  AddFilterAndSetup(RewriteOptions::kInlineImages);
  AddFileToMockFetcher(StrCat(kTestDomain, kCuppaPngFile), kCuppaPngFile,
                       kContentTypePng, 100);
  GoogleString css_link = RewriteImageInCss(kCuppaPngFile);
  EXPECT_STREQ(EncodedCssUrl("ei+ii+cii=5"), css_link);
  GoogleString image_url = ExtractImageFromCssFilename(css_link);
  EXPECT_STREQ(Encode(kTestDomain, "ce", "0", kCuppaPngFile, "png"), image_url);
  EXPECT_EQ(1763, FetchImageFromCache(image_url).size());
}

TEST_F(CssEmbeddedConfigTest, InlineImageToCssSmallThresholdCompress) {
  options()->set_css_image_inline_max_bytes(5);  // prevents inlining
  options()->set_image_jpeg_recompress_quality(60);
  options()->EnableFilter(RewriteOptions::kRecompressJpeg);
  AddFilterAndSetup(RewriteOptions::kInlineImages);
  AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString css_link = RewriteImageInCss(kPuzzleJpgFile);
  EXPECT_STREQ(EncodedCssUrl("ii+rj+cii=5+iq=60"), css_link);
  GoogleString image_url = ExtractImageFromCssFilename(css_link);
  EXPECT_STREQ(EncodedImageUrl(kPuzzleJpgFile, "jpg", "rj+iq=60"), image_url);
  EXPECT_GE(67113, FetchImageFromCache(image_url).size());
}

TEST_F(CssEmbeddedConfigTest, InlineImageToCssSmallTranscode) {
  options()->set_image_webp_recompress_quality(60);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  AddFilterAndSetup(RewriteOptions::kInlineImages);
  if (RunningOnValgrind()) {  // Too slow under vg; must call Setup first.
    return;
  }
  SetCurrentUserAgent("webp");
  rewrite_driver()->SetUserAgent("webp");
  AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile), kPuzzleJpgFile,
                       kContentTypeJpeg, 100);
  GoogleString css_link = RewriteImageInCss(kPuzzleJpgFile);
  EXPECT_STREQ(EncodedCssUrl("jw+ii+iw=60"), css_link);
  GoogleString image_url = ExtractImageFromCssFilename(css_link);
  EXPECT_STREQ(EncodedImageUrl(kPuzzleJpgFile, "webp", "jw+iw=60"), image_url);
  EXPECT_GE(36350, FetchImageFromCache(image_url).size());
}

}  // namespace net_instaweb
