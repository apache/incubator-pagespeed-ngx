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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/critical_selector_filter.h"

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/critical_selectors.pb.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kTestCohort[] = "test_cohort";
const char kRequestUrl[] = "http://www.example.com/";

class CriticalSelectorFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    filter_ = new CriticalSelectorFilter(rewrite_driver());
    rewrite_driver()->AppendOwnedPreRenderFilter(filter_);
    server_context()->ComputeSignature(options());
    server_context()->set_critical_selector_finder(
        new CriticalSelectorFinder(kTestCohort, statistics()));

    // Setup pcache.
    pcache_ = rewrite_driver()->server_context()->page_property_cache();
    SetupCohort(pcache_, kTestCohort);
    SetupCohort(pcache_, RewriteDriver::kDomCohort);
    ResetDriver();

    // Write out some initial critical selectors for us to work with.
    CriticalSelectorSet selectors;
    selectors.add_critical_selectors("div");
    selectors.add_critical_selectors("*");
    server_context()->critical_selector_finder()->
        WriteCriticalSelectorsToPropertyCache(selectors, rewrite_driver());
    page_->WriteCohort(pcache_->GetCohort(kTestCohort));

    // Some weird but valid CSS.
    SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                  "div,span,*::first-letter { display: block; }"
                                  "p { display: inline; }", 100);
    SetResponseWithDefaultHeaders("b.css", kContentTypeCss,
                                  "@media screen,print { * { margin: 0px; } }",
                                  100);
  }

  void ResetDriver() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    page_ = NewMockPage(kRequestUrl);
    rewrite_driver()->set_property_page(page_);
    pcache_->Read(page_);
  }

  virtual bool AddHtmlTags() const { return false; }

  CriticalSelectorFilter* filter_;  // owned by the driver;
  PropertyCache* pcache_;
  PropertyPage* page_;
};

TEST_F(CriticalSelectorFilterTest, BasicOperation) {
  GoogleString css = StrCat(
      "<style>*,p {display: none; } span {display: inline; }</style>",
      CssLinkHref("a.css"),
      CssLinkHref("b.css"));

  GoogleString critical_css =
      "*{display:none}"  // from the inline
      "div,*::first-letter{display:block}"  // from a.css
      "@media screen{*{margin:0px}}";  // from b.css

  GoogleString html = StrCat(
      "<head>",
      css,
      "</head>"
      "<body><div>Stuff</div></body>");

  // First run just collects the result into pcache
  ValidateNoChanges("foo", html);

  ResetDriver();
  ValidateExpected(
      "foo", html,
      StrCat("<head><style>", critical_css, "</style></head>",
              "<body><div>Stuff</div></body>",
              "<noscript id=\"psa_add_styles\">", css, "</noscript>",
              StrCat("<script type=\"text/javascript\">//<![CDATA[\n",
                     CriticalSelectorFilter::kAddStylesScript,
                     "\n//]]></script>")));
}

}  // namespace

}  // namespace net_instaweb
