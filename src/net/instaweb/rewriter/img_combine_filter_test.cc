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

#include "net/instaweb/rewriter/public/img_combine_filter.h"
#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"

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
    options_.EnableFilter(RewriteOptions::kSpriteImages);
    CssRewriteTestBase::SetUp();
  }
};

TEST_F(CssImageCombineTest, SpritesImages) {
  AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile),
                       StrCat(GTestSrcDir(), kTestData, kPuzzleJpgFile),
                       kContentTypeJpeg);
  AddFileToMockFetcher(StrCat(kTestDomain, kCuppaPngFile),
                       StrCat(GTestSrcDir(), kTestData, kCuppaPngFile),
                       kContentTypePng);
  AddFileToMockFetcher(StrCat(kTestDomain, kBikePngFile),
                       StrCat(GTestSrcDir(), kTestData, kBikePngFile),
                       kContentTypePng);

  const std::string before = StrCat(
      "<head><style>"
      "#div1 { background-image:url('", kCuppaPngFile, "');"
      "background-repeat:no-repeat;}"
      "#div2 { background:transparent url('", kBikePngFile, "') no-repeat}"
      "#div3 { background-image: url('", kPuzzleJpgFile, "');"
      "background-repeat:no-repeat;}"
      "</style></head>");
  // The JPEG will not be included in the sprite because we only handle PNGs.
  const std::string sprite = StrCat(kTestDomain, kCuppaPngFile, "+",
                                     kBikePngFile, ".pagespeed.is.0.png");
  const std::string after = StrCat(
      "<head><style>#div1{background-image:url(", sprite, ");"
      "background-repeat:no-repeat;"
      "background-position-y:0px!important;background-position-x:0px!important}"
      "#div2{background:transparent url(", sprite,
      ") no-repeat;background-position-y:-70px!important;"
      "background-position-x:0px!important}#div3{background-image:url(",
      kPuzzleJpgFile, ");background-repeat:no-repeat}</style></head>");

  ValidateExpected("sprites_images", before, after);
}

TEST_F(CssImageCombineTest, NoCrashUnknownType) {
  // Make sure we don't crash trying to sprite an image with an unknown mimetype
  AddFilter(RewriteOptions::kSpriteImages);
  AddFilter(RewriteOptions::kRewriteCss);

  ResponseHeaders response_headers;
  resource_manager_->SetDefaultHeaders(&kContentTypePng, &response_headers);
  response_headers.Replace(HttpAttributes::kContentType, "image/x-bewq");
  response_headers.ComputeCaching();
  mock_url_fetcher_.SetResponse(StrCat(kTestDomain, "bar.bewq"),
                                response_headers, "unused payload");
  InitResponseHeaders("foo.png", kContentTypePng, "unused payload", 100);

  const std::string before =
      "<head><style>"
      "#div1 { background-image:url('bar.bewq');"
      "background-repeat:no-repeat;}"
      "#div2 { background:transparent url('foo.png') no-repeat}"
      "</style></head>";

  ParseUrl(kTestDomain, before);
}

}  // namespace

}  // namespace net_instaweb
