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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/public/critical_css_beacon_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kHtmlTemplate[] =
    "<html><head>%s</head><body><p>content</p>%s</body></html>";
const char kInlineStyle[] =
    "<style media='not print'>"
    "a{color:red}"
    "a:visited{color:green}"
    "p{color:green}"
    "</style>";
const char kStyleA[] =
    "div ul:hover>li{color:red}"
    ":hover{color:red}"
    ".sec h1#id{color:green}";
const char kStyleB[] =
    "a{color:green}"
    "p:hover{color:red}"
    "div ul > li{color:green}";
const char kStyleCorrupt[] =
    "span{color:";
const char kStyleEvil[] =
    "div{display:inline}";
const char kEvilUrl[] = "http://evil.com/d.css";

class CriticalCssBeaconFilterTest : public RewriteTestBase {
 public:
  CriticalCssBeaconFilterTest() { }
  virtual ~CriticalCssBeaconFilterTest() { }

 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->EnableFilter(RewriteOptions::kRewriteCss);
    options()->EnableFilter(RewriteOptions::kFlattenCssImports);
    options()->EnableFilter(RewriteOptions::kInlineImportToLink);
    // TODO(jmaessen): control filter by manipulating configuration options when
    // they exist.
    CriticalCssBeaconFilter::InitStats(statistics());
    filter_ = new CriticalCssBeaconFilter(rewrite_driver());
    rewrite_driver()->AppendOwnedPreRenderFilter(filter_);
    rewrite_driver()->AddFilters();

    SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                  kStyleA, 100);
    SetResponseWithDefaultHeaders("b.css", kContentTypeCss,
                                  kStyleB, 100);
    SetResponseWithDefaultHeaders("corrupt.css", kContentTypeCss,
                                  kStyleCorrupt, 100);
    SetResponseWithDefaultHeaders(kEvilUrl, kContentTypeCss,
                                  kStyleEvil, 100);
  }

  // Generate an optimized reference for the given leaf css.
  GoogleString CssLinkHrefOpt(StringPiece leaf) {
    return CssLinkHref(Encode(kTestDomain, "cf", "0", leaf, "css"));
  }

  CriticalCssBeaconFilter* filter_;  // Owned by rewrite_driver()
};

TEST_F(CriticalCssBeaconFilterTest, ExtractFromInlineStyle) {
  GoogleString input_html = StringPrintf(kHtmlTemplate, kInlineStyle, "");
  GoogleString output_html =
      StringPrintf(kHtmlTemplate, kInlineStyle, "<!-- a, p -->");
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, ExtractFromUnopt) {
  GoogleString css = CssLinkHref("a.css");
  GoogleString input_html = StringPrintf(
      kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, css.c_str(), "<!-- .sec h1#id, div ul > li -->");
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, ExtractFromOpt) {
  GoogleString css = StrCat(CssLinkHref("b.css"), kInlineStyle);
  GoogleString opt = StrCat(CssLinkHrefOpt("b.css"), kInlineStyle);
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, opt.c_str(), "<!-- a, div ul > li, p -->");
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, Unauthorized) {
  GoogleString css = StrCat(CssLinkHref(kEvilUrl), kInlineStyle);
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, css.c_str(), "<!-- a, p -->");
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, Missing) {
  SetFetchFailOnUnexpected(false);
  GoogleString css = StrCat(CssLinkHref("404.css"), kInlineStyle);
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, css.c_str(), "<!-- a, p -->");
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, Corrupt) {
  GoogleString css = StrCat(CssLinkHref("corrupt.css"), kInlineStyle);
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, css.c_str(), "<!-- a, p -->");
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, Everything) {
  SetFetchFailOnUnexpected(false);
  GoogleString css = StrCat(
      CssLinkHref("a.css"), CssLinkHref("404.css"), kInlineStyle,
      CssLinkHref(kEvilUrl), CssLinkHref("corrupt.css"), CssLinkHref("b.css"));
  GoogleString opt = StrCat(
      CssLinkHref("a.css"), CssLinkHref("404.css"), kInlineStyle,
      CssLinkHref(kEvilUrl), CssLinkHref("corrupt.css"),
      CssLinkHrefOpt("b.css"));
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, opt.c_str(),
      "<!-- .sec h1#id, a, div ul > li, p -->");
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

// Make sure we ignore non-screen resources.
// Test behavior in the face of up-to-date pCache data
// Test behavior in the face of extant stale pCache data

}  // namespace

}  // namespace net_instaweb
