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

}  // namespace

}  // namespace net_instaweb
