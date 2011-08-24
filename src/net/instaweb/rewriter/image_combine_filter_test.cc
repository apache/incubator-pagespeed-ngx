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

// Author: sligocki@google.com (Shawn Ligocki)
#include <algorithm>

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kCuppaPngFile[] = "Cuppa.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";

// Image spriting tests.
class CssImageCombineTest : public CssRewriteTestBase {
 protected:
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(RewriteOptions::kSpriteImages);
    CssRewriteTestBase::SetUp();
    AddFileToMockFetcher(StrCat(kTestDomain, kBikePngFile), kBikePngFile,
                         kContentTypePng, 100);
    AddFileToMockFetcher(StrCat(kTestDomain, kCuppaPngFile), kCuppaPngFile,
                         kContentTypePng, 100);
    AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile), kPuzzleJpgFile,
                         kContentTypeJpeg, 100);
  }
  void TestSpriting(const char* bikePosition, const char* expectedPosition,
                    bool should_sprite) {
    const GoogleString sprite_string = StrCat(kTestDomain, kCuppaPngFile, "+",
                                              kBikePngFile,
                                              ".pagespeed.is.0.png");
    const char* sprite = sprite_string.c_str();
    // The JPEG will not be included in the sprite because we only handle PNGs.
    const char* html = "<head><style>"
        "#div1{background-image:url(%s);"
        "background-position:0px 0px;width:10px;height:10px}"
        "#div2{background:transparent url(%s);"
        "background-position:%s;width:10px;height:10px}"
        "#div3{background-image:url(%s);width:10px;height:10px}"
        "</style></head>";
    GoogleString before = StringPrintf(
        html, kCuppaPngFile, kBikePngFile, bikePosition, kPuzzleJpgFile);
    GoogleString after = StringPrintf(
        html, sprite, sprite, expectedPosition, kPuzzleJpgFile);

    ValidateExpected("sprites_images", before, should_sprite ? after : before);

    // Try it again, this time using the background shorthand with a couple
    // different orderings
    const char* html2 = "<head><style>"
        "#div1{background:0px 0px url(%s) no-repeat transparent scroll;"
        "width:10px;height:10px}"
        "#div2{background:url(%s) %s repeat fixed;width:10px;height:10px}"
        "#div3{background-image:url(%s);width:10px;height:10px}"
        "</style></head>";

    before = StringPrintf(
        html2, kCuppaPngFile, kBikePngFile, bikePosition, kPuzzleJpgFile);
    after = StringPrintf(
        html2, sprite, sprite, expectedPosition, kPuzzleJpgFile);

    ValidateExpected("sprites_images", before, should_sprite ? after : before);
  }
};

TEST_P(CssImageCombineTest, SpritesImages) {
  CSS_XFAIL_SYNC();
  TestSpriting("0px 0px", "0px -70px", true);
  TestSpriting("left top", "0px -70px", true);
  TestSpriting("top 10px", "10px -70px", true);
  TestSpriting("-5px 5px", "-5px -65px", true);
  TestSpriting("center top", "unused", false);
}

TEST_P(CssImageCombineTest, SpritesMultiple) {
  CSS_XFAIL_SYNC();
  const char* html = "<head><style>"
      "#div1{background:url(%s) 0px 0px;width:10px;height:10px}"
      "#div2{background:url(%s) 0px %dpx;width:%dpx;height:10px}"
      "#div3{background:url(%s) 0px %dpx;width:10px;height:10px}"
      "</style></head>";
  GoogleString before, after, sprite;
  // With the same image present 3 times, there should be no sprite.
  before = StringPrintf(html, kBikePngFile, kBikePngFile, 0, 10,
                        kBikePngFile, 0);
  ValidateExpected("no_sprite_3_bikes", before, before);

  // With 2 of the same and 1 different, there should be a sprite without
  // duplication.
  before = StringPrintf(html, kBikePngFile, kBikePngFile, 0, 10,
                        kCuppaPngFile, 0);
  sprite = StrCat(kTestDomain, kBikePngFile, "+", kCuppaPngFile,
                  ".pagespeed.is.0.png").c_str();
  after = StringPrintf(html, sprite.c_str(),
                       sprite.c_str(), 0, 10, sprite.c_str(), -100);
  ValidateExpected("sprite_2_bikes_1_cuppa", before, after);

  // If the second occurrence of the image is unspriteable (e.g. if the div is
  // larger than the image), then don't sprite anything.
  before = StringPrintf(html, kBikePngFile, kBikePngFile, 0, 999,
                        kCuppaPngFile, 0);
  ValidateExpected("sprite_none_dimmensions", before, before);
}

// Try the last test from SpritesMultiple with a cold cache.
TEST_P(CssImageCombineTest, NoSpritesMultiple) {
  CSS_XFAIL_SYNC();
  const char* html = "<head><style>"
      "#div1{background:url(%s) 0px 0px;width:10px;height:10px}"
      "#div2{background:url(%s) 0px %dpx;width:%dpx;height:10px}"
      "#div3{background:url(%s) 0px %dpx;width:10px;height:10px}"
      "</style></head>";
  GoogleString text;
  // If the second occurence of the image is unspriteable (e.g. if the div is
  // larger than the image), then don't sprite anything.
  text = StringPrintf(html, kBikePngFile, kBikePngFile, 0, 999,
                        kCuppaPngFile, 0);
  ValidateExpected("no_sprite", text, text);

}

TEST_P(CssImageCombineTest, NoCrashUnknownType) {
  CSS_XFAIL_SYNC();
  // Make sure we don't crash trying to sprite an image with an unknown mimetype
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypePng, &response_headers);
  response_headers.Replace(HttpAttributes::kContentType, "image/x-bewq");
  response_headers.ComputeCaching();
  SetFetchResponse(StrCat(kTestDomain, "bar.bewq"),
                   response_headers, "unused payload");
  InitResponseHeaders("foo.png", kContentTypePng, "unused payload", 100);

  const GoogleString before =
      "<head><style>"
      "#div1 { background-image:url('bar.bewq');"
      "width:10px;height:10px}"
      "#div2 { background:transparent url('foo.png');width:10px;height:10px}"
      "</style></head>";

  ParseUrl(kTestDomain, before);
}

TEST_P(CssImageCombineTest, SpritesImagesExternal) {
  CSS_XFAIL_SYNC();
  SetupWaitFetcher();

  const GoogleString beforeCss = StrCat(" "  // extra whitespace allows rewrite
      "#div1{background-image:url(", kCuppaPngFile, ");"
      "width:10px;height:10px}"
      "#div2{background:transparent url(", kBikePngFile,
                                        ");width:10px;height:10px}");
  GoogleString cssUrl(kTestDomain);
  cssUrl += "style.css";
  // At first try, not even the CSS gets loaded, so nothing gets
  // changed at all.
  ValidateRewriteExternalCss(
      "wip", beforeCss, beforeCss, kNoOtherContexts | kNoClearFetcher |
      kExpectNoChange | kExpectSuccess);

  // Allow the images to load
  CallFetcherCallbacks();

  // On the second run, we get spriting.
  const GoogleString sprite = StrCat(kTestDomain, kCuppaPngFile, "+",
                                     kBikePngFile,
                                     ".pagespeed.is.0.png");
  const GoogleString spriteCss = StrCat(
      "#div1{background-image:url(", sprite, ");"
      "width:10px;height:10px;"
      "background-position:0px 0px}"
      "#div2{background:transparent url(", sprite,
      ");width:10px;height:10px;background-position:0px -70px}");
  ValidateRewriteExternalCss(
      "wip", beforeCss, spriteCss, kNoOtherContexts | kNoClearFetcher |
      kExpectChange | kExpectSuccess | kNoStatCheck);
}

TEST_P(CssImageCombineTest, SpritesOkAfter404) {
  // Make sure the handling of a 404 is correct, and doesn't interrupt spriting
  // (nor check fail, as it used to before).
  CSS_XFAIL_SYNC();

  AddFileToMockFetcher(StrCat(kTestDomain, "bike2.png"), kBikePngFile,
                       kContentTypePng, 100);
  AddFileToMockFetcher(StrCat(kTestDomain, "bike3.png"), kBikePngFile,
                       kContentTypePng, 100);
  SetFetchResponse404("404.png");

  const char kHtmlTemplate[] = "<head><style>"
      "#div1{background:url(%s);width:10px;height:10px}"
      "#div2{background:url(%s);width:10px;height:10px}"
      "#div3{background:url(%s);width:10px;height:10px}"
      "#div4{background:url(%s);width:10px;height:10px}"
      "#div5{background:url(%s);width:10px;height:10px}"
      "</style></head>";

  GoogleString html = StringPrintf(kHtmlTemplate,
                                   kBikePngFile,
                                   kCuppaPngFile,
                                   "404.png",
                                   "bike2.png",
                                   "bike3.png");
  Parse("sprite_with_404", html);  // Parse
  EXPECT_NE(GoogleString::npos,
            output_buffer_.find(
                StrCat(kBikePngFile, "+", kCuppaPngFile,
                       "+bike2.png+bike3.png.pagespeed.is.0.png")));
}

TEST_P(CssImageCombineTest, SpritesMultiSite) {
  // Make sure we do something sensible when we're forced to split into multiple
  // partitions due to different host names -- at lest when it doesn't require
  // us to keep track of multiple partitions intelligently.
  CSS_XFAIL_SYNC();

  const char kAltDomain[] = "http://images.example.com/";
  DomainLawyer* lawyer = options()->domain_lawyer();
  lawyer->AddDomain(kAltDomain, message_handler());

  AddFileToMockFetcher(StrCat(kAltDomain, kBikePngFile), kBikePngFile,
                        kContentTypePng, 100);
  AddFileToMockFetcher(StrCat(kAltDomain, kCuppaPngFile), kCuppaPngFile,
                        kContentTypePng, 100);

  const char kHtmlTemplate[] = "<head><style>"
      "#div1{background:url(%s);width:10px;height:10px}"
      "#div2{background:url(%s);width:10px;height:10px}"
      "#div3{background:url(%s);width:10px;height:10px}"
      "#div4{background:url(%s);width:10px;height:10px}"
      "</style></head>";

  GoogleString html = StringPrintf(kHtmlTemplate,
                                   StrCat(kTestDomain, kBikePngFile).c_str(),
                                   StrCat(kTestDomain, kCuppaPngFile).c_str(),
                                   StrCat(kAltDomain, kBikePngFile).c_str(),
                                   StrCat(kAltDomain, kCuppaPngFile).c_str());
  Parse("sprite_multi_site", html);
  EXPECT_NE(GoogleString::npos,
            output_buffer_.find(
                StrCat(kTestDomain, kBikePngFile, "+", kCuppaPngFile,
                       ".pagespeed.is.0.png")));

  EXPECT_NE(GoogleString::npos,
            output_buffer_.find(
                StrCat(kAltDomain, kBikePngFile, "+", kCuppaPngFile,
                       ".pagespeed.is.0.png")));
}

// TODO(nforman): Add a testcase that synthesizes a spriting situation where
// the total size of the constructed segment (not including the domain or
// .pagespeed.* parts) is larger than RewriteOptions::kDefaultMaxUrlSegmentSize
// (1024).
TEST_P(CssImageCombineTest, ServeFiles) {
  CSS_XFAIL_SYNC();
  GoogleString sprite_str = StrCat(kTestDomain, kCuppaPngFile, "+",
                                    kBikePngFile, ".pagespeed.is.0.png");
  GoogleString output;
  EXPECT_EQ(true, ServeResourceUrl(sprite_str, &output));
  ServeResourceFromManyContexts(sprite_str, output);
}

TEST_P(CssImageCombineTest, CombineManyFiles) {
  CSS_XFAIL_SYNC();
  // Prepare an HTML fragment with too many image files to combine,
  // exceeding the char limit.
  const int kNumImages = 100;
  const int kImagesInCombination = 47;
  GoogleString html = "<head><style>";
  for (int i = 0; i < kNumImages; ++i) {
    GoogleString url = StringPrintf("%s%.02d%s", kTestDomain, i, kBikePngFile);
    AddFileToMockFetcher(url, kBikePngFile, kContentTypePng, 100);
    html.append(StringPrintf(
        "#div%d{background:url(%s) 0px 0px;width:10px;height:10px}",
        i, url.c_str()));
  }
  html.append("</style></head>");

  // We expect 3 combinations: 0-46, 47-93, 94-99
  StringVector combinations;
  int image_index = 0;
  while (image_index < kNumImages) {
    GoogleString combo = kTestDomain;
    int end_index = std::min(image_index + kImagesInCombination, kNumImages);
    while (image_index < end_index) {
      combo.append(StringPrintf("%.02d%s", image_index, kBikePngFile));
      combo.append("+");
      ++image_index;
    }
    combo.resize(combo.size() - 1);
    combo.append(".pagespeed.is.0.png");
    combinations.push_back(combo);
  }

  image_index = 0;
  int combo_index = 0;
  GoogleString result = "<head><style>";
  while (image_index < kNumImages) {
    result.append(StringPrintf(
        "#div%d{background:url(%s) 0px %dpx;width:10px;height:10px}",
        image_index, combinations[combo_index].c_str(),
        (image_index - (combo_index * kImagesInCombination)) * -100));
    ++image_index;
    if (image_index % kImagesInCombination == 0) {
      ++combo_index;
    }
  }
  result.append("</style></head>");

  ValidateExpected("manymanyimages", html, result);
}

// We test with asynchronous_rewrites() == GetParam() as both true and false.
INSTANTIATE_TEST_CASE_P(CssImageCombineTestInstance,
                        CssImageCombineTest,
                        ::testing::Bool());

class CssImageMultiFilterTest : public CssImageCombineTest {
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(RewriteOptions::kExtendCache);
    CssImageCombineTest::SetUp();
  }
};

TEST_P(CssImageMultiFilterTest, SpritesAndNonSprites) {
  CSS_XFAIL_SYNC();
  const char* html = "<head><style>"
      "#div1{background:url(%s) 0px 0px;width:10px;height:10px}"
      "#div2{background:url(%s) 0px %dpx;width:%dpx;height:10px}"
      "#div3{background:url(%s) 0px %dpx;width:10px;height:10px}"
      "</style></head>";
  GoogleString before, after, encoded, cuppa_encoded, sprite;
  // With the same image present 3 times, there should be no sprite.
  before = StringPrintf(html, kBikePngFile, kBikePngFile, 0, 10,
                        kBikePngFile, 0);
  encoded = Encode(kTestDomain, "ce", "0", kBikePngFile, "png");
  after = StringPrintf(html, encoded.c_str(), encoded.c_str(), 0, 10,
                       encoded.c_str(), 0);
  ValidateExpected("no_sprite_3_bikes", before, after);

  // With 2 of the same and 1 different, there should be a sprite without
  // duplication.
  before = StringPrintf(html, kBikePngFile, kBikePngFile, 0, 10,
                        kCuppaPngFile, 0);
  sprite = StrCat(kTestDomain, kBikePngFile, "+", kCuppaPngFile,
                  ".pagespeed.is.0.png").c_str();
  after = StringPrintf(html, sprite.c_str(),
                       sprite.c_str(), 0, 10, sprite.c_str(), -100);
  ValidateExpected("sprite_2_bikes_1_cuppa", before, after);

  // If the second occurrence of the image is unspriteable (e.g. if the div is
  // larger than the image), we shouldn't sprite any of them.
  before = StringPrintf(html, kBikePngFile, kBikePngFile, 0, 999,
                        kCuppaPngFile, 0);
  cuppa_encoded = Encode(kTestDomain, "ce", "0", kCuppaPngFile, "png");
  after = StringPrintf(html, encoded.c_str(), encoded.c_str(), 0, 999,
                       cuppa_encoded.c_str());
  ValidateExpected("sprite_none_dimmensions", before, after);
}

// We test with asynchronous_rewrites() == GetParam() as both true and false.
INSTANTIATE_TEST_CASE_P(CssImageMultiFilterTestInstance,
                        CssImageMultiFilterTest,
                        ::testing::Bool());

}  // namespace

}  // namespace net_instaweb
