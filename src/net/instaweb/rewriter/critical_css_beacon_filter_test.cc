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
#include "net/instaweb/http/public/user_agent_matcher_test.h"
#include "net/instaweb/rewriter/public/critical_css_beacon_filter.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

namespace {

const char kHtmlTemplate[] =
    "<head>%s</head><body><p>content</p>%s</body>";
const char kInlineStyle[] =
    "<style media='not print'>"
    "a{color:red}"
    "a:visited{color:green}"
    "p{color:green}"
    "</style>";
const char kInlinePrint[] =
    "<style media='print'>"
    "span{color:red}"
    "</style>";
const char kStyleA[] =
    "div ul:hover>li{color:red}"
    ":hover{color:red}"
    ".sec h1#id{color:green}";
const char kStyleB[] =
    "a{color:green}"
    "@media screen { p:hover{color:red} }"
    "@media print { span{color:green} }"
    "div ul > li{color:green}";
const char kStyleCorrupt[] =
    "span{color:";
const char kStyleEvil[] =
    "div{display:inline}";
const char kEvilUrl[] = "http://evil.com/d.css";

// Common setup / result generation code for all tests
class CriticalCssBeaconFilterTestBase : public RewriteTestBase {
 public:
  CriticalCssBeaconFilterTestBase() { }
  virtual ~CriticalCssBeaconFilterTestBase() { }

 protected:
  // Set everything up except for filter configuration.
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetHtmlMimetype();  // Don't wrap scripts in <![CDATA[ ]]>
    factory()->set_use_beacon_results_in_filters(true);

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

  GoogleString BeaconScriptFor(StringPiece selectors) {
    StaticAssetManager* manager =
        rewrite_driver()->server_context()->static_asset_manager();
    GoogleString script = StrCat(
        "<script src=\"",
        manager->GetAssetUrl(
            StaticAssetManager::kCriticalCssBeaconJs, options()),
        "\"></script><script type=\"text/javascript\">");
    StrAppend(
        &script,
        "pagespeed.criticalCssBeaconInit('",
        options()->beacon_url().http, "','",
        kTestDomain, "','0',[", selectors, "]);</script>");
    return script;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CriticalCssBeaconFilterTestBase);
};

// Standard test setup enables filter via RewriteOptions.
class CriticalCssBeaconFilterTest : public CriticalCssBeaconFilterTestBase {
 public:
  CriticalCssBeaconFilterTest() { }
  virtual ~CriticalCssBeaconFilterTest() { }

 protected:
  virtual void SetUp() {
    CriticalCssBeaconFilterTestBase::SetUp();
    options()->EnableFilter(RewriteOptions::kPrioritizeCriticalCss);
    rewrite_driver()->AddFilters();
  }

  // Generate an optimized reference for the given leaf css.
  GoogleString CssLinkHrefOpt(StringPiece leaf) {
    return CssLinkHref(Encode(kTestDomain, "cf", "0", leaf, "css"));
  }

  GoogleString BeaconScriptFor(StringPiece selectors) {
    StaticAssetManager* manager =
        rewrite_driver()->server_context()->static_asset_manager();
    GoogleString script = StrCat(
        "<script src=\"",
        manager->GetAssetUrl(
            StaticAssetManager::kCriticalCssBeaconJs, options()),
        "\"></script><script type=\"text/javascript\">");
    StrAppend(
        &script,
        "pagespeed.criticalCssBeaconInit('",
        options()->beacon_url().http, "','",
        kTestDomain, "','0',[", selectors, "]);</script>");
    return script;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CriticalCssBeaconFilterTest);
};

TEST_F(CriticalCssBeaconFilterTest, ExtractFromInlineStyle) {
  GoogleString input_html = StringPrintf(kHtmlTemplate, kInlineStyle, "");
  GoogleString output_html =
      StringPrintf(kHtmlTemplate, kInlineStyle,
                   BeaconScriptFor("\"a\",\"p\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, DisabledForIE) {
  rewrite_driver()->SetUserAgent(UserAgentStrings::kIe7UserAgent);
  GoogleString input_html = StringPrintf(kHtmlTemplate, kInlineStyle, "");
  ValidateNoChanges(kTestDomain, input_html);
}

TEST_F(CriticalCssBeaconFilterTest, ExtractFromUnopt) {
  GoogleString css = CssLinkHref("a.css");
  GoogleString input_html = StringPrintf(
      kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, css.c_str(),
      BeaconScriptFor("\".sec h1#id\",\"div ul > li\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, ExtractFromOpt) {
  GoogleString css = StrCat(CssLinkHref("b.css"), kInlineStyle);
  GoogleString opt = StrCat(CssLinkHrefOpt("b.css"), kInlineStyle);
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, opt.c_str(),
      BeaconScriptFor("\"a\",\"div ul > li\",\"p\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, DontExtractFromNoScript) {
  GoogleString css = StrCat(CssLinkHref("a.css"),
                            "<noscript>", CssLinkHref("b.css"), "</noscript>");
  GoogleString opt = StrCat(
      CssLinkHref("a.css"),
      "<noscript>", CssLinkHrefOpt("b.css"), "</noscript>");
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  // Selectors only from a.css, since b.css in the noscript.
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, opt.c_str(),
      BeaconScriptFor("\".sec h1#id\",\"div ul > li\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, DontExtractFromAlternate) {
  GoogleString css = StrCat(CssLinkHref("a.css"),
                            "<link rel=\"alternate stylesheet\" href=b.css>");
  GoogleString opt = StrCat(
      CssLinkHref("a.css"),
      "<link rel=\"alternate stylesheet\" href=",
      Encode(kTestDomain, "cf", "0", "b.css", "css"),
      ">");
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  // Selectors only from a.css, since b.css is alternate.
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, opt.c_str(),
      BeaconScriptFor("\".sec h1#id\",\"div ul > li\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, Unauthorized) {
  GoogleString css = StrCat(CssLinkHref(kEvilUrl), kInlineStyle);
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, css.c_str(), BeaconScriptFor("\"a\",\"p\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, Missing) {
  SetFetchFailOnUnexpected(false);
  GoogleString css = StrCat(CssLinkHref("404.css"), kInlineStyle);
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, css.c_str(), BeaconScriptFor("\"a\",\"p\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, Corrupt) {
  GoogleString css = StrCat(CssLinkHref("corrupt.css"), kInlineStyle);
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, css.c_str(), BeaconScriptFor("\"a\",\"p\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, NonScreenMediaInline) {
  GoogleString html = StringPrintf(kHtmlTemplate, kInlinePrint, "");
  ValidateNoChanges("non-screen-inline", html);
}

TEST_F(CriticalCssBeaconFilterTest, NonScreenMediaExternal) {
  GoogleString css = "<link rel=stylesheet href='a.css' media='print'>";
  GoogleString html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  ValidateNoChanges("non-screen-external", html);
}

TEST_F(CriticalCssBeaconFilterTest, MixOfGoodAndBad) {
  // Make sure we don't see any strange interactions / missed connections.
  SetFetchFailOnUnexpected(false);
  GoogleString css = StrCat(
      CssLinkHref("a.css"), CssLinkHref("404.css"), kInlineStyle,
      CssLinkHref(kEvilUrl), CssLinkHref("corrupt.css"), kInlinePrint,
      CssLinkHref("b.css"));
  GoogleString opt = StrCat(
      CssLinkHref("a.css"), CssLinkHref("404.css"), kInlineStyle,
      CssLinkHref(kEvilUrl), CssLinkHref("corrupt.css"), kInlinePrint,
      CssLinkHrefOpt("b.css"));
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, opt.c_str(),
      BeaconScriptFor("\".sec h1#id\",\"a\",\"div ul > li\",\"p\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

TEST_F(CriticalCssBeaconFilterTest, EverythingThatParses) {
  GoogleString css = StrCat(
      CssLinkHref("a.css"), kInlineStyle, CssLinkHref("b.css"));
  GoogleString opt = StrCat(
      CssLinkHref("a.css"), kInlineStyle, CssLinkHrefOpt("b.css"));
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, opt.c_str(),
      BeaconScriptFor("\".sec h1#id\",\"a\",\"div ul > li\",\"p\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

// This class explicitly only includes the beacon filter and its prerequisites;
// this lets us test the presence of beacon results without the critical
// selector filter injecting a lot of stuff in the output.
class CriticalCssBeaconOnlyTest : public CriticalCssBeaconFilterTestBase {
 public:
  CriticalCssBeaconOnlyTest() { }
  virtual ~CriticalCssBeaconOnlyTest() { }

 protected:
  virtual void SetUp() {
    CriticalCssBeaconFilterTestBase::SetUp();
    // Need to set up filters that are normally auto-enabled by
    // kPrioritizeCriticalCss: we're switching on CriticalCssBeaconFilter by
    // hand so that we don't turn on CriticalSelectorFilter.
    options()->EnableFilter(RewriteOptions::kRewriteCss);
    options()->EnableFilter(RewriteOptions::kFlattenCssImports);
    options()->EnableFilter(RewriteOptions::kInlineImportToLink);
    CriticalCssBeaconFilter::InitStats(statistics());
    filter_ = new CriticalCssBeaconFilter(rewrite_driver());
    rewrite_driver()->AddFilters();
    rewrite_driver()->AppendOwnedPreRenderFilter(filter_);
  }

 private:
  CriticalCssBeaconFilter* filter_;  // Owned by rewrite_driver()

  DISALLOW_COPY_AND_ASSIGN(CriticalCssBeaconOnlyTest);
};

// Right now we never beacon if there's valid pcache data, even if that data
// corresponds to an earlier version of the page.
TEST_F(CriticalCssBeaconOnlyTest, ExtantPCache) {
  // Set up and register a beacon finder.
  CriticalSelectorFinder* finder =
      new CriticalSelectorFinder(RewriteDriver::kBeaconCohort, statistics());
  server_context()->set_critical_selector_finder(finder);
  // Set up pcache for page.
  SetupCohort(page_property_cache(), RewriteDriver::kBeaconCohort);
  PropertyPage* page = NewMockPage(kTestDomain);
  rewrite_driver()->set_property_page(page);
  page_property_cache()->Read(page);
  // Inject pcache entry.
  StringSet selectors;
  selectors.insert("div ul > li");
  selectors.insert("p");
  selectors.insert("span");  // Doesn't occur in our CSS
  finder->WriteCriticalSelectorsToPropertyCache(selectors, rewrite_driver());
  // Force cohort to persist.
  page->WriteCohort(
      page_property_cache()->GetCohort(RewriteDriver::kBeaconCohort));
  // Check injection
  EXPECT_TRUE(rewrite_driver()->CriticalSelectors() != NULL);
  // Now do the test.
  GoogleString css = StrCat(
      CssLinkHref("a.css"), kInlineStyle, CssLinkHref("b.css"));
  GoogleString opt = StrCat(
      CssLinkHref("a.css"), kInlineStyle, CssLinkHrefOpt("b.css"));
  GoogleString input_html = StringPrintf(kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(kHtmlTemplate, opt.c_str(), "");
  ValidateExpected("already_beaconed", input_html, output_html);
}

class CriticalCssBeaconWithCombinerFilterTest
    : public CriticalCssBeaconFilterTest {
 public:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kCombineCss);
    CriticalCssBeaconFilterTest::SetUp();
  }
};

TEST_F(CriticalCssBeaconWithCombinerFilterTest, Interaction) {
  // Make sure that beacon insertion interacts with combine CSS properly.
  GoogleString css = StrCat(CssLinkHref("a.css"), CssLinkHref("b.css"));
  GoogleString combined_css = CssLinkHref(
      Encode(kTestDomain, "cf", "0",
             Encode("", "cc", "0", MultiUrl("a.css", "b.css"), "css"), "css"));
  GoogleString input_html = StringPrintf(
      kHtmlTemplate, css.c_str(), "");
  GoogleString output_html = StringPrintf(
      kHtmlTemplate, combined_css.c_str(),
      BeaconScriptFor("\".sec h1#id\",\"a\",\"div ul > li\",\"p\"").c_str());
  ValidateExpectedUrl(kTestDomain, input_html, output_html);
}

}  // namespace

}  // namespace net_instaweb
