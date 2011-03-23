/*
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/rewriter/public/css_image_rewriter.h"

#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include <string>

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kCuppaPngFile[] = "Cuppa.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";

const char kImageData[] = "Invalid PNG but it does not matter for this test";

class CssImageRewriterTest : public ResourceManagerTestBase {
};

TEST_F(CssImageRewriterTest, CacheExtendsImages) {
  AddFilter(RewriteOptions::kExtendCache);
  AddFilter(RewriteOptions::kRewriteCss);
  InitResponseHeaders("foo.png", kContentTypePng, kImageData, 100);
  InitResponseHeaders("bar.png", kContentTypePng, kImageData, 100);
  InitResponseHeaders("baz.png", kContentTypePng, kImageData, 100);

  static const char before[] =
      "<head><style>"
      "body {\n"
      "  background-image: url(foo.png);\n"
      "  list-style-image: url('bar.png');\n"
      "}\n"
      ".titlebar p.cfoo, #end p {\n"
      "  background: url(\"baz.png\");\n"
      "  list-style: url('foo.png');\n"
      "}\n"
      ".other {\n"
      "  background-image:url(data:image/;base64,T0sgYjAxZGJhYTZmM2Y1NTYyMQ==);"
      "  -proprietary-background-property: url(foo.png);\n"
      "}\n"
      "</style></head>";
  static const char after[] =
      "<head><style>"
      "body{background-image:url(http://test.com/foo.png.pagespeed.ce.0.png);"
      "list-style-image:url(http://test.com/bar.png.pagespeed.ce.0.png)}"
      ".titlebar p.cfoo,#end p{"
      "background:url(http://test.com/baz.png.pagespeed.ce.0.png);"
      "list-style:url(http://test.com/foo.png.pagespeed.ce.0.png)}"
      ".other{"  // data: URLs and unknown properties are not rewritten.
      "background-image:url(data:image/;base64\\,T0sgYjAxZGJhYTZmM2Y1NTYyMQ==);"
      "-proprietary-background-property:url(foo.png)}"
      "</style></head>";

  ValidateExpected("cache_extends_images", before, after);
}

TEST_F(CssImageRewriterTest, SpritesImages) {
  AddFilter(RewriteOptions::kSpriteImages);
  AddFilter(RewriteOptions::kRewriteCss);
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

}  // namespace

}  // namespace net_instaweb
