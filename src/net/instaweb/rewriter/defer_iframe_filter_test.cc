/*
 * Copyright 2012 Google Inc.
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

// Author: pulkitg@google.com (Pulkit Goyal)

#include "net/instaweb/rewriter/public/defer_iframe_filter.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

#include "testing/base/public/gunit.h"

namespace net_instaweb {

class DeferIframeFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    rewrite_driver_->AddOwnedPostRenderFilter(
        new DeferIframeFilter(rewrite_driver_));
  }

  GoogleString GeneratePagespeedIframeTag(const StringPiece& src) {
    return StrCat("<pagespeed_iframe src=\"", src, "\">"
        "<script type=\"text/javascript\">"
        "\npagespeed.deferIframe.convertToIframe();"
        "</script></pagespeed_iframe>");
  }
};

TEST_F(DeferIframeFilterTest, TestDeferIframe) {
  StringPiece defer_iframe_js_code =
      resource_manager()->static_javascript_manager()->GetJsSnippet(
          StaticJavascriptManager::kDeferIframe, options());
  GoogleString input_html = "<head></head>"
      "<body>"
      "<iframe src=\"http://test.com/1.html\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body><script type=\"text/javascript\">",
      defer_iframe_js_code, "pagespeed.deferIframeInit();</script>",
      GeneratePagespeedIframeTag("http://test.com/1.html"), "</body>");
  ValidateExpected("defer_iframe", input_html, output_html);
}

TEST_F(DeferIframeFilterTest, TestNoIframePresent) {
  GoogleString input_html = "<head></head>"
      "<body>"
      "<img src=\"http://test.com/1.jpeg\"/>"
      "</body>";
  ValidateExpected("defer_iframe", input_html, input_html);
}

TEST_F(DeferIframeFilterTest, TestMultipleIframePresent) {
  StringPiece defer_iframe_js_code =
      resource_manager()->static_javascript_manager()->GetJsSnippet(
          StaticJavascriptManager::kDeferIframe, options());
  GoogleString input_html = "<head></head>"
      "<body>"
      "<iframe src=\"http://test.com/1.html\"/>"
      "<iframe src=\"http://test.com/2.html\"/>"
      "</body>";
  GoogleString output_html = StrCat(
      "<head></head><body><script type=\"text/javascript\">",
      defer_iframe_js_code, "pagespeed.deferIframeInit();</script>",
      GeneratePagespeedIframeTag("http://test.com/1.html"),
      GeneratePagespeedIframeTag("http://test.com/2.html"), "</body>");
  ValidateExpected("defer_iframe", input_html, output_html);
}

}  // namespace net_instaweb
