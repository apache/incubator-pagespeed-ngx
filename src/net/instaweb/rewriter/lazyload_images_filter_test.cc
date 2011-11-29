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

// Author: nikhilmadan@google.com (Nikhil madan)

#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class LazyloadImagesFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    SetAsynchronousRewrites(true);
    AddFilter(RewriteOptions::kLazyloadImages);
  }

  GoogleString GenerateRewrittenImageTag(
      const StringPiece& tag,
      const StringPiece& url,
      const StringPiece& additional_attributes) {
    return StrCat("<", tag, " ", additional_attributes,
                  "pagespeed_lazy_src=\"", url, "\" onload=\"",
                  StrCat(LazyloadImagesFilter::kImageOnloadCode, "\" src=\"",
                         LazyloadImagesFilter::kDefaultInlineImage, "\"/>"));
  }
};

TEST_F(LazyloadImagesFilterTest, SingleHead) {
  ValidateExpected("lazyload_images",
      "<head></head>"
      "<body>"
      "<img src=\"1.jpg\" />"
      "<img src=\"2.jpg\" height=\"300\" width=\"123\" />"
      "<input type=\"image\" src=\"12.jpg\" />"
      "<input src=\"12.jpg\" />"
      "<img src=\"1.jpg\" onload=\"blah();\" />"
      "</body>",
      StrCat("<head><script type=\"text/javascript\">",
             LazyloadImagesFilter::kImageLazyloadCode,
             "\npagespeed.lazyLoadInit();\n",
             "</script></head>"
             "<body>", GenerateRewrittenImageTag("img", "1.jpg", ""),
             GenerateRewrittenImageTag("img", "2.jpg",
                                       "height=\"300\" width=\"123\" "),
             GenerateRewrittenImageTag("input", "12.jpg", "type=\"image\" "),
             "<input src=\"12.jpg\"/>"
             "<img src=\"1.jpg\" onload=\"blah();\"/>"
             "</body>"));
}

TEST_F(LazyloadImagesFilterTest, NoHeadTag) {
  GoogleString input_html = "<body>"
      "<img src=\"1.jpg\"/>"
      "</body>";
  // No change in the html if script is not inserted in the head.
  ValidateExpected("lazyload_images", input_html, input_html);
}

TEST_F(LazyloadImagesFilterTest, MultipleHeadTags) {
  ValidateExpected("lazyload_images",
      "<head></head>"
      "<head></head>"
      "<body>"
      "</body>",
      StrCat("<head><script type=\"text/javascript\">",
             LazyloadImagesFilter::kImageLazyloadCode,
             "\npagespeed.lazyLoadInit();\n",
             "</script></head>"
             "<head></head><body></body>"));
}

}  // namespace net_instaweb
