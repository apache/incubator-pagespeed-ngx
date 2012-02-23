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
#include "net/instaweb/rewriter/public/javascript_url_manager.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class LazyloadImagesFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
  }

  virtual void InitLazyloadImagesFilter(bool debug) {
    if (debug) {
      options()->EnableFilter(RewriteOptions::kDebug);
    }
    lazyload_images_filter_.reset(
        new LazyloadImagesFilter(rewrite_driver()));
    rewrite_driver()->AddFilter(lazyload_images_filter_.get());
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

  scoped_ptr<LazyloadImagesFilter> lazyload_images_filter_;
};

TEST_F(LazyloadImagesFilterTest, SingleHead) {
  InitLazyloadImagesFilter(false);
  StringPiece lazyload_js_code =
      resource_manager()->javascript_url_manager()->GetJsSnippet(
          JavascriptUrlManager::kLazyloadImagesJs, options());
  ValidateExpected("lazyload_images",
      "<head></head>"
      "<body>"
      "<img src=\"1.jpg\" />"
      "<img src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhE\"/>"
      "<img src=\"2.jpg\" height=\"300\" width=\"123\" />"
      "<input type=\"image\" src=\"12.jpg\" />"
      "<input src=\"12.jpg\" />"
      "<img src=\"1.jpg\" onload=\"blah();\" />"
      "</body>",
      StrCat("<head><script type=\"text/javascript\">",
             lazyload_js_code,
             "\npagespeed.lazyLoadInit(false);\n"
             "</script></head><body>",
             GenerateRewrittenImageTag("img", "1.jpg", ""),
             "<img src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhE\"/>",
             GenerateRewrittenImageTag("img", "2.jpg",
                                       "height=\"300\" width=\"123\" "),
             GenerateRewrittenImageTag("input", "12.jpg", "type=\"image\" "),
             "<input src=\"12.jpg\"/>"
             "<img src=\"1.jpg\" onload=\"blah();\"/></body>"));
}

TEST_F(LazyloadImagesFilterTest, SingleHeadLoadOnOnload) {
  InitLazyloadImagesFilter(false);
  StringPiece lazyload_js_code =
      resource_manager()->javascript_url_manager()->GetJsSnippet(
          JavascriptUrlManager::kLazyloadImagesJs, options());
  options()->ClearSignatureForTesting();
  options()->set_lazyload_images_after_onload(true);
  resource_manager()->ComputeSignature(options());
  ValidateExpected("lazyload_images",
      "<head></head>"
      "<body>"
      "</body>",
      StrCat("<head><script type=\"text/javascript\">",
             lazyload_js_code,
             "\npagespeed.lazyLoadInit(true);\n",
             "</script></head>"
             "<body></body>"));
}

TEST_F(LazyloadImagesFilterTest, NoHeadTag) {
  InitLazyloadImagesFilter(false);
  GoogleString input_html = "<body>"
      "<img src=\"1.jpg\"/>"
      "</body>";
  // No change in the html if script is not inserted in the head.
  ValidateNoChanges("lazyload_images", input_html);
}

TEST_F(LazyloadImagesFilterTest, MultipleHeadTags) {
  InitLazyloadImagesFilter(false);
  StringPiece lazyload_js_code =
      resource_manager()->javascript_url_manager()->GetJsSnippet(
          JavascriptUrlManager::kLazyloadImagesJs, options());
  ValidateExpected("lazyload_images",
      "<head></head>"
      "<head></head>"
      "<body>"
      "</body>",
      StrCat("<head><script type=\"text/javascript\">",
             lazyload_js_code,
             "\npagespeed.lazyLoadInit(false);\n",
             "</script></head>"
             "<head></head><body></body>"));
}

TEST_F(LazyloadImagesFilterTest, LazyloadScriptOptimized) {
  InitLazyloadImagesFilter(false);
  Parse("optimized",
        "<head></head><body><img src=\"1.jpg\"></body>");
  EXPECT_EQ(GoogleString::npos, output_buffer_.find("/*"))
      << "There should be no comments in the optimized code";
}

TEST_F(LazyloadImagesFilterTest, LazyloadScriptDebug) {
  InitLazyloadImagesFilter(true);
  Parse("debug",
        "<head></head><img src=\"1.jpg\"></body>");
  EXPECT_NE(GoogleString::npos, output_buffer_.find("/*"))
      << "There should still be some comments in the debug code";
}

}  // namespace net_instaweb
