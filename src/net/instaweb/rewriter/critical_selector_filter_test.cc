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

const char kRequestUrl[] = "http://www.example.com/";

class CriticalSelectorFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    filter_ = new CriticalSelectorFilter(rewrite_driver());
    rewrite_driver()->AppendOwnedPreRenderFilter(filter_);
    server_context()->ComputeSignature(options());
    server_context()->set_critical_selector_finder(
        new CriticalSelectorFinder(RewriteDriver::kBeaconCohort, statistics()));

    // Setup pcache.
    pcache_ = rewrite_driver()->server_context()->page_property_cache();
    SetupCohort(pcache_, RewriteDriver::kBeaconCohort);
    SetupCohort(pcache_, RewriteDriver::kDomCohort);
    ResetDriver();

    // Write out some initial critical selectors for us to work with.
    StringSet selectors;
    selectors.insert("div");
    selectors.insert("*");
    server_context()->critical_selector_finder()->
        WriteCriticalSelectorsToPropertyCache(selectors, rewrite_driver());
    page_->WriteCohort(pcache_->GetCohort(RewriteDriver::kBeaconCohort));

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
    SetHtmlMimetype();  // Don't wrap scripts in <![CDATA[ ]]>
  }

  GoogleString LoadRestOfCss(StringPiece orig_css) {
    return StrCat("<noscript id=\"psa_add_styles\">", orig_css, "</noscript>",
                  StrCat("<script type=\"text/javascript\">",
                         CriticalSelectorFilter::kAddStylesScript,
                         "</script>"));
  }

  GoogleString CssLinkHrefMedia(StringPiece url, StringPiece media) {
    return StrCat("<link rel=stylesheet href=", url,
                  " media=\"", media, "\">");
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
      "<style>*{display:none}</style>"  // from the inline
      "<style>div,*::first-letter{display:block}</style>"  // from a.css
      "<style>@media screen{*{margin:0px}}</style>";  // from b.css

  GoogleString html = StrCat(
      "<head>",
      css,
      "</head>"
      "<body><div>Stuff</div></body>");

  ValidateExpected(
      "foo", html,
      StrCat("<head>", critical_css, "</head>",
             "<body><div>Stuff</div></body>",
             LoadRestOfCss(css)));
}

TEST_F(CriticalSelectorFilterTest, Media) {
  GoogleString css = StrCat(
      "<style media=screen,print>*,p {display: none; } "
          "span {display: inline; }</style>",
      CssLinkHrefMedia("a.css", "screen"),
      CssLinkHrefMedia("b.css", "screen and (color), aural"));

  GoogleString critical_css =
      "<style media=\"screen\">"
          "*{display:none}"  // from the inline
      "</style>"
      "<style media=\"screen\">"
          "div,*::first-letter{display:block}"  // from a.css
      "</style>"
      "<style media=\"screen and (color)\">"
          "@media screen{*{margin:0px}}"  // from b.css
      "</style>";

  GoogleString html = StrCat(
      "<head>",
      css,
      "</head>"
      "<body><div>Stuff</div></body>");

  ValidateExpected(
      "foo", html,
      StrCat("<head>", critical_css, "</head>",
             "<body><div>Stuff</div></body>",
             LoadRestOfCss(css)));
}

TEST_F(CriticalSelectorFilterTest, NonScreenMedia) {
  GoogleString css = StrCat(
      "<style media=print>*,p {display: none; } "
          "span {display: inline; }</style>",
      CssLinkHrefMedia("a.css", "screen"),
      CssLinkHrefMedia("b.css", "screen and (color), aural"));

  GoogleString critical_css =
      "<style media=\"screen\">"
          "div,*::first-letter{display:block}"  // from a.css
      "</style>"
      "<style media=\"screen and (color)\">"
          "@media screen{*{margin:0px}}"  // from b.css
      "</style>";

  GoogleString html = StrCat(
      "<head>",
      css,
      "</head>"
      "<body><div>Stuff</div></body>");

  ValidateExpected(
      "foo", html,
      StrCat("<head>", critical_css, "</head>",
             "<body><div>Stuff</div></body>",
             LoadRestOfCss(css)));
}

TEST_F(CriticalSelectorFilterTest, SameCssDifferentSelectors) {
  // We should not reuse results for same CSS when selectors are different.
  GoogleString css = "<style>div,span { display: inline-block; }</style>";

  GoogleString critical_css_div = "div{display:inline-block}";
  GoogleString critical_css_span = "span{display:inline-block}";
  GoogleString critical_css_div_span = "div,span{display:inline-block}";

  // Check what we compute for a page with div.
  ValidateExpected("with_div", StrCat(css, "<div>Foo</div>"),
                   StrCat("<style>", critical_css_div, "</style>",
                          "<div>Foo</div>", LoadRestOfCss(css)));

  // Update the selector list with span. Because we are storing the last N
  // beacon entries, both div and span should now be in the critical set. We
  // also clear the property cache entry for our result, which is needed because
  // the test harness is not really keying the pcache by the URL like the real
  // system would.
  StringSet selectors;
  selectors.insert("span");
  CriticalSelectorFinder* finder = server_context()->critical_selector_finder();
  finder->WriteCriticalSelectorsToPropertyCache(selectors, rewrite_driver());
  page_->WriteCohort(pcache_->GetCohort(RewriteDriver::kBeaconCohort));
  page_->DeleteProperty(
      pcache_->GetCohort(RewriteDriver::kDomCohort),
      CriticalSelectorFilter::kSummarizedCssProperty);
  page_->WriteCohort(pcache_->GetCohort(RewriteDriver::kDomCohort));

  // Note that calling ResetDriver() just resets the state in the
  // driver. Whatever has been written to the property & metadata caches so far
  // will persist. Upon rewriting, the property cache contents will be read and
  // the critical selector info in the driver will be repopulated.
  ResetDriver();
  ValidateExpected("with_div_span", StrCat(css, "<span>Foo</span>"),
                   StrCat("<style>", critical_css_div_span, "</style>",
                          "<span>Foo</span>", LoadRestOfCss(css)));

  // Now send enough beacons to clear div from the critical selector set, so
  // only span should be left.
  for (int i = 0; i < finder->NumSetsToKeep(); ++i) {
    finder->WriteCriticalSelectorsToPropertyCache(selectors, rewrite_driver());
  }
  page_->WriteCohort(pcache_->GetCohort(RewriteDriver::kBeaconCohort));
  page_->DeleteProperty(
      pcache_->GetCohort(RewriteDriver::kDomCohort),
      CriticalSelectorFilter::kSummarizedCssProperty);
  page_->WriteCohort(pcache_->GetCohort(RewriteDriver::kDomCohort));
  ResetDriver();
  ValidateExpected("with_span", StrCat(css, "<span>Foo</span>"),
                   StrCat("<style>", critical_css_span, "</style>",
                          "<span>Foo</span>", LoadRestOfCss(css)));
}

TEST_F(CriticalSelectorFilterTest, RetainPseudoOnly) {
  // Make sure we handle things like :hover OK.
  GoogleString css = ":hover { border: 2px solid red; }";
  SetResponseWithDefaultHeaders("c.css", kContentTypeCss,
                                css, 100);
  ValidateExpected("hover", CssLinkHref("c.css"),
                   StrCat("<style>:hover{border:2px solid red}</style>",
                          LoadRestOfCss(CssLinkHref("c.css"))));
}

TEST_F(CriticalSelectorFilterTest, RetainUnparseable) {
  // Make sure we keep unparseable fragments around, particularly when
  // the problem is with the selector, as well as with the entire region.
  GoogleString css = "!huh! {background: white; } @huh { display: block; }";
  SetResponseWithDefaultHeaders("c.css", kContentTypeCss,
                                css, 100);
  ValidateExpected(
      "partly_unparseable", CssLinkHref("c.css"),
      StrCat("<style>!huh! {background:#fff}@huh { display: block; }</style>",
             LoadRestOfCss(CssLinkHref("c.css"))));
}

TEST_F(CriticalSelectorFilterTest, NoSelectorInfo) {
  // Particular CSS doesn't matter here, just want some.
  GoogleString css = "<style>div,span { display: inline-block; }</style>";

  // We shouldn't change things when there is no info on selectors available.
  page_->DeleteProperty(
      pcache_->GetCohort(RewriteDriver::kBeaconCohort),
      CriticalSelectorFinder::kCriticalSelectorsPropertyName);
  page_->WriteCohort(pcache_->GetCohort(RewriteDriver::kBeaconCohort));

  ResetDriver();
  ValidateNoChanges("no_sel_info", StrCat(css, "<div>Foo</div>"));

  ResetDriver();
  ValidateNoChanges("no_sel_info", StrCat(css, "<div>Foo</div>"));
}

TEST_F(CriticalSelectorFilterTest, ResolveUrlsProperly) {
  SetResponseWithDefaultHeaders("dir/c.css", kContentTypeCss,
                                "* { background-image: url(d.png); }", 100);
  ValidateExpected("rel_path", CssLinkHref("dir/c.css"),
                   StrCat("<style>*{background-image:url(dir/d.png)}</style>",
                          LoadRestOfCss(CssLinkHref("dir/c.css"))));
}

}  // namespace

}  // namespace net_instaweb
