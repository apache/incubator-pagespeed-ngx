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

#include "net/instaweb/rewriter/public/critical_css_beacon_filter.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
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

namespace net_instaweb {

namespace {

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
    "@media screen { p:hover{color:red} }"
    "@media print { span{color:green} }"
    "div ul > li{color:green}";

// The styles above produce the following beacon initialization selector lists.
const char kSelectorsInline[] = "\"a\",\"p\"";
const char kSelectorsA[] = "\".sec h1#id\",\"div ul > li\"";
const char kSelectorsB[] = "\"a\",\"div ul > li\",\"p\"";
const char kSelectorsInlineAB[] = "\".sec h1#id\",\"a\",\"div ul > li\",\"p\"";

// The following styles do not add selectors to the beacon initialization.
const char kInlinePrint[] =
    "<style media='print'>"
    "span{color:red}"
    "</style>";
const char kStyleCorrupt[] =
    "span{color:";
const char kStyleEmpty[] =
    "/* This has no selectors */";
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
    rewrite_driver()->set_property_page(NewMockPage(kTestDomain));
    // Set up pcache for page.
    const PropertyCache::Cohort* cohort =
        SetupCohort(page_property_cache(), RewriteDriver::kBeaconCohort);
    server_context()->set_beacon_cohort(cohort);
    page_property_cache()->Read(rewrite_driver()->property_page());
    // Set up and register a beacon finder.
    CriticalSelectorFinder* finder = new BeaconCriticalSelectorFinder(
        server_context()->beacon_cohort(), factory()->nonce_generator(),
        statistics());
    server_context()->set_critical_selector_finder(finder);
    // Set up contents of CSS files.
    SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                  kStyleA, 100);
    SetResponseWithDefaultHeaders("b.css", kContentTypeCss,
                                  kStyleB, 100);
    SetResponseWithDefaultHeaders("corrupt.css", kContentTypeCss,
                                  kStyleCorrupt, 100);
    SetResponseWithDefaultHeaders("empty.css", kContentTypeCss,
                                  kStyleEmpty, 100);
    SetResponseWithDefaultHeaders(kEvilUrl, kContentTypeCss,
                                  kStyleEvil, 100);
  }

  // Return a css_filter optimized url.
  GoogleString UrlOpt(StringPiece url) {
    return Encode("", RewriteOptions::kCssFilterId, "0", url, "css");
  }

  // Return a link tag with a css_filter optimized url.
  GoogleString CssLinkHrefOpt(StringPiece url) {
    return CssLinkHref(UrlOpt(url));
  }

  GoogleString InputHtml(StringPiece head) {
    return StrCat("<head>", head, "</head><body><p>content</p></body>");
  }

  GoogleString BeaconHtml(StringPiece head, StringPiece selectors) {
    GoogleString html = StrCat(
        "<head>", head, "</head><body><p>content</p>"
        "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
        server_context()->static_asset_manager()->GetAsset(
            StaticAssetManager::kCriticalCssBeaconJs, options()),
        "pagespeed.selectors=[", selectors, "];");
    StrAppend(&html,
              "pagespeed.criticalCssBeaconInit('",
              options()->beacon_url().http, "','", kTestDomain,
              "','0','", ExpectedNonce(), "',pagespeed.selectors);"
              "</script></body>");
    return html;
  }

  GoogleString SelectorsOnlyHtml(StringPiece head, StringPiece selectors) {
    return StrCat(
        "<head>", head, "</head><body><p>content</p>"
        "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
        CriticalCssBeaconFilter::kInitializePageSpeedJs,
        "pagespeed.selectors=[", selectors, "];"
        "</script></body>");
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

 private:
  DISALLOW_COPY_AND_ASSIGN(CriticalCssBeaconFilterTest);
};

TEST_F(CriticalCssBeaconFilterTest, ExtractFromInlineStyle) {
  ValidateExpectedUrl(
      kTestDomain,
      InputHtml(kInlineStyle),
      BeaconHtml(kInlineStyle, kSelectorsInline));
}

TEST_F(CriticalCssBeaconFilterTest, DisabledForIE) {
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kIe7UserAgent);
  ValidateNoChanges(kTestDomain, InputHtml(kInlineStyle));
}

TEST_F(CriticalCssBeaconFilterTest, ExtractFromUnopt) {
  ValidateExpectedUrl(
      kTestDomain,
      InputHtml(CssLinkHref("a.css")),
      BeaconHtml(CssLinkHref("a.css"), kSelectorsA));
}

TEST_F(CriticalCssBeaconFilterTest, ExtractFromOpt) {
  GoogleString input_html = InputHtml(
      StrCat(CssLinkHref("b.css"), kInlineStyle));
  GoogleString expected_html = BeaconHtml(
      StrCat(CssLinkHrefOpt("b.css"), kInlineStyle), kSelectorsB);
  ValidateExpectedUrl(kTestDomain, input_html, expected_html);
}

TEST_F(CriticalCssBeaconFilterTest, DontExtractFromNoScript) {
  GoogleString input_html = InputHtml(
      StrCat(CssLinkHref("a.css"),
             "<noscript>", CssLinkHref("b.css"), "</noscript>"));
  GoogleString expected_html = BeaconHtml(
      StrCat(CssLinkHref("a.css"),
             "<noscript>", CssLinkHrefOpt("b.css"), "</noscript>"),
      kSelectorsA);
  ValidateExpectedUrl(kTestDomain, input_html, expected_html);
}

TEST_F(CriticalCssBeaconFilterTest, DontExtractFromAlternate) {
  GoogleString input_html = InputHtml(
      StrCat(CssLinkHref("a.css"),
             "<link rel=\"alternate stylesheet\" href=b.css>"));
  GoogleString expected_html = BeaconHtml(
      StrCat(CssLinkHref("a.css"),
             "<link rel=\"alternate stylesheet\" href=", UrlOpt("b.css"), ">"),
      kSelectorsA);
  ValidateExpectedUrl(kTestDomain, input_html, expected_html);
}

TEST_F(CriticalCssBeaconFilterTest, Unauthorized) {
  GoogleString css = StrCat(CssLinkHref(kEvilUrl), kInlineStyle);
  ValidateExpectedUrl(
      kTestDomain, InputHtml(css), BeaconHtml(css, kSelectorsInline));
}

TEST_F(CriticalCssBeaconFilterTest, Missing) {
  SetFetchFailOnUnexpected(false);
  GoogleString css = StrCat(CssLinkHref("404.css"), kInlineStyle);
  ValidateExpectedUrl(
      kTestDomain, InputHtml(css), BeaconHtml(css, kSelectorsInline));
}

TEST_F(CriticalCssBeaconFilterTest, Corrupt) {
  GoogleString css = StrCat(CssLinkHref("corrupt.css"), kInlineStyle);
  ValidateExpectedUrl(
      kTestDomain, InputHtml(css), BeaconHtml(css, kSelectorsInline));
}

TEST_F(CriticalCssBeaconFilterTest, EmptyCssIgnored) {
  // This failed when SummariesDone called SplitStringPieceToVector()
  // with "omit_empty_strings" set to false. The beacon selector list
  // looked like the following: [,".sec h1#id","a","div ul > li","p"].
  // That caused the beacon JavaScript to take the length of 'undefined'.
  GoogleString input_html = InputHtml(
      StrCat(CssLinkHref("a.css"), kInlineStyle, CssLinkHref("empty.css")));
  GoogleString expected_html = BeaconHtml(
      StrCat(CssLinkHref("a.css"), kInlineStyle, CssLinkHrefOpt("empty.css")),
      kSelectorsInlineAB);
  ValidateExpectedUrl(kTestDomain, input_html, expected_html);
}

TEST_F(CriticalCssBeaconFilterTest, EmptyCssDoesNotTriggerBeaconCode) {
  GoogleString input_html = InputHtml(CssLinkHref("empty.css"));
  GoogleString expected_html = InputHtml(CssLinkHrefOpt("empty.css"));
  ValidateExpectedUrl(kTestDomain, input_html, expected_html);
}

TEST_F(CriticalCssBeaconFilterTest, NonScreenMediaInline) {
  ValidateNoChanges("non-screen-inline", InputHtml(kInlinePrint));
}

TEST_F(CriticalCssBeaconFilterTest, NonScreenMediaExternal) {
  ValidateNoChanges(
      "non-screen-external",
      InputHtml("<link rel=stylesheet href='a.css' media='print'>"));
}

TEST_F(CriticalCssBeaconFilterTest, MixOfGoodAndBad) {
  // Make sure we don't see any strange interactions / missed connections.
  SetFetchFailOnUnexpected(false);
  GoogleString input_html = InputHtml(
      StrCat(CssLinkHref("a.css"), CssLinkHref("404.css"), kInlineStyle,
             CssLinkHref(kEvilUrl), CssLinkHref("corrupt.css"), kInlinePrint,
             CssLinkHref("b.css")));
  GoogleString expected_html = BeaconHtml(
      StrCat(CssLinkHref("a.css"), CssLinkHref("404.css"), kInlineStyle,
             CssLinkHref(kEvilUrl), CssLinkHref("corrupt.css"), kInlinePrint,
             CssLinkHrefOpt("b.css")),
      kSelectorsInlineAB);
  ValidateExpectedUrl(kTestDomain, input_html, expected_html);
}

TEST_F(CriticalCssBeaconFilterTest, EverythingThatParses) {
  GoogleString input_html = InputHtml(
      StrCat(CssLinkHref("a.css"), kInlineStyle, CssLinkHref("b.css")));
  GoogleString expected_html = BeaconHtml(
      StrCat(CssLinkHref("a.css"), kInlineStyle, CssLinkHrefOpt("b.css")),
      kSelectorsInlineAB);
  ValidateExpectedUrl(kTestDomain, input_html, expected_html);
}

TEST_F(CriticalCssBeaconFilterTest, FalseBeaconResultsGivesEmptyBeaconUrl) {
  factory()->set_use_beacon_results_in_filters(false);
  GoogleString input_html = InputHtml(
      StrCat(CssLinkHref("a.css"), kInlineStyle, CssLinkHref("b.css")));
  GoogleString expected_html = SelectorsOnlyHtml(
      StrCat(CssLinkHref("a.css"), kInlineStyle, CssLinkHrefOpt("b.css")),
      kSelectorsInlineAB);
  ValidateExpectedUrl(kTestDomain, input_html, expected_html);
}

// This class explicitly only includes the beacon filter and its prerequisites;
// this lets us test the presence of beacon results without the critical
// selector filter injecting a lot of stuff in the output.
class CriticalCssBeaconOnlyTest : public CriticalCssBeaconFilterTestBase {
 public:
  CriticalCssBeaconOnlyTest() {}
  virtual ~CriticalCssBeaconOnlyTest() {}

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

// Make sure we re-beacon if candidate data changes.
TEST_F(CriticalCssBeaconOnlyTest, ExtantPCache) {
  // Inject pcache entry.
  StringSet selectors;
  selectors.insert("div ul > li");
  selectors.insert("p");
  selectors.insert("span");  // Doesn't occur in our CSS
  CriticalSelectorFinder* finder = server_context()->critical_selector_finder();
  RewriteDriver* driver = rewrite_driver();
  BeaconMetadata metadata =
      finder->PrepareForBeaconInsertion(selectors, driver);
  ASSERT_TRUE(metadata.status == kBeaconWithNonce);
  EXPECT_EQ(ExpectedNonce(), metadata.nonce);
  finder->WriteCriticalSelectorsToPropertyCache(
      selectors, metadata.nonce, driver);
  // Force cohort to persist.
  rewrite_driver()->property_page()
      ->WriteCohort(server_context()->beacon_cohort());
  // Now do the test.

  GoogleString input_html = InputHtml(
      StrCat(CssLinkHref("a.css"), kInlineStyle, CssLinkHref("b.css")));
  GoogleString expected_html = BeaconHtml(
      StrCat(CssLinkHref("a.css"), kInlineStyle, CssLinkHrefOpt("b.css")),
      kSelectorsInlineAB);
  ValidateExpectedUrl(kTestDomain, input_html, expected_html);
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
  GoogleString input_html = InputHtml(
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")));
  GoogleString combined_url = Encode("", RewriteOptions::kCssCombinerId, "0",
                                     MultiUrl("a.css", "b.css"), "css");
  // css_filter applies after css_combine and adds to the url encoding.
  GoogleString expected_url = UrlOpt(combined_url);
  GoogleString expected_html = BeaconHtml(
      CssLinkHref(expected_url), kSelectorsInlineAB);
  ValidateExpectedUrl(kTestDomain, input_html, expected_html);
}

}  // namespace

}  // namespace net_instaweb
