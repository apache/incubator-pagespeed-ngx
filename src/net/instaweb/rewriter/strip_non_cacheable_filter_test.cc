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

// Author: rahulbansal@google.com (Rahul Bansal)

#include "net/instaweb/rewriter/public/strip_non_cacheable_filter.h"

#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.test.com";

const char kHtmlInput[] =
    "<html>"
    "<body>"
    "<noscript>This should get removed</noscript>"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"Item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item lots of classes here for testing\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
      "<div class=\"itema itemb others are ok\">"
        "<img src=\"image5\">"
      "</div>"
      "<div class=\"itemb before itema\">"
        "<img src=\"image6\">"
      "</div>"
      "<div class=\"itemb only\">"
        "<img src=\"image7\">"
      "</div>"
    "</body></html>";

const char kBlinkUrlHandler[] = "/psajs/blink.js";
const char kBlinkUrlGstatic[] = "http://www.gstatic.com/psa/static/1-blink.js";
const char kPsaHeadScriptNodesStart[] =
    "<script type=\"text/javascript\" pagespeed_no_defer=\"\" src=\"";

const char kPsaHeadScriptNodesEnd[] =
    "\"></script>"
    "<script type=\"text/javascript\" pagespeed_no_defer=\"\">pagespeed.deferInit();</script>";

}  // namespace


class StripNonCacheableFilterTest : public ResourceManagerTestBase {
 public:
  StripNonCacheableFilterTest() {}

  virtual ~StripNonCacheableFilterTest() {}

  virtual void SetUp() {
    delete options_;
    options_ = new RewriteOptions();
    options_->EnableFilter(RewriteOptions::kStripNonCacheable);

    // The following is to test with both option setting mechanisms for
    // non-cacheable panels.
    // TODO(sriharis): Remove this (keep only AddBlinkCacheableFamily) once
    // transition to new prioritize_visible_content options is complete.
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(), "StripNonCacheableOldOption") == 0) {
      options_->set_prioritize_visible_content_non_cacheable_elements(
          "/:class=\"item\",id=beforeItems,class=\"itema itemb\"");
    } else {
      options_->AddBlinkCacheableFamily(
          "/", RewriteOptions::kDefaultPrioritizeVisibleContentCacheTimeMs,
          "class= \"item \" , id\t =beforeItems \t , class=\"itema itemb\"");
    }

    SetUseManagedRewriteDrivers(true);
    ResourceManagerTestBase::SetUp();
  }

  virtual bool AddHtmlTags() const { return false; }

 protected:
  GoogleString GetExpectedOutput(const StringPiece blink_js) {
    GoogleString psa_head_script_nodes = StrCat(
        kPsaHeadScriptNodesStart, blink_js, kPsaHeadScriptNodesEnd);
    return StrCat(
        "<html><head>",
        psa_head_script_nodes,
        "</head><body>",
        BlinkUtil::kStartBodyMarker,
        "<div id=\"header\"> This is the header </div>"
        "<div id=\"container\" class>"
        "<!--GooglePanel begin panel-id-1.0--><!--GooglePanel end panel-id-1.0-->"
        "<!--GooglePanel begin panel-id-0.0--><!--GooglePanel end panel-id-0.0-->"
        "<!--GooglePanel begin panel-id-0.1-->"
        "<!--GooglePanel end panel-id-0.1-->"
        "<!--GooglePanel begin panel-id-2.0-->"
        "<!--GooglePanel end panel-id-2.0-->"
        "<!--GooglePanel begin panel-id-2.1-->"
        "<!--GooglePanel end panel-id-2.1-->"
        "<div class=\"itemb only\"><img src=\"image7\"></div>"
        "</body></html>");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StripNonCacheableFilterTest);
};

TEST_F(StripNonCacheableFilterTest, StripNonCacheableOldOption) {
  ValidateExpectedUrl(kRequestUrl, kHtmlInput,
                      GetExpectedOutput(kBlinkUrlHandler));
}

TEST_F(StripNonCacheableFilterTest, StripNonCacheable) {
  ValidateExpectedUrl(kRequestUrl, kHtmlInput,
                      GetExpectedOutput(kBlinkUrlHandler));
}

TEST_F(StripNonCacheableFilterTest, TestGstatic) {
  UrlNamer url_namer;
  StaticJavascriptManager js_manager(&url_namer, true, "1");
  resource_manager()->set_static_javascript_manager(&js_manager);
  ValidateExpectedUrl(kRequestUrl, kHtmlInput,
                      GetExpectedOutput(kBlinkUrlGstatic));
}

}  // namespace net_instaweb
