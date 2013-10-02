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
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/critical_finder_support_util.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/mock_timer.h"

namespace net_instaweb {

namespace {

const char kRequestUrl[] = "http://www.example.com/";

class CriticalSelectorFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUpBeforeSelectorsFilter() {
    rewrite_driver()->AddFilters();
  }

  virtual void SetUpAfterSelectorsFilter() {
    server_context()->ComputeSignature(options());
  }

  virtual void SetUp() {
    RewriteTestBase::SetUp();

    // Disable flush early for tests (unless they turn it on).
    options()->set_enable_flush_early_critical_css(false);

    // Enable critical selector filter alone so that
    // testing isn't disrupted by beacon injection.
    SetUpBeforeSelectorsFilter();
    filter_ = new CriticalSelectorFilter(rewrite_driver());
    rewrite_driver()->AppendOwnedPreRenderFilter(filter_);
    SetUpAfterSelectorsFilter();
    // Setup pcache.
    pcache_ = rewrite_driver()->server_context()->page_property_cache();
    const PropertyCache::Cohort* beacon_cohort =
        SetupCohort(pcache_, RewriteDriver::kBeaconCohort);
    const PropertyCache::Cohort* dom_cohort =
        SetupCohort(pcache_, RewriteDriver::kDomCohort);
    server_context()->set_dom_cohort(dom_cohort);
    server_context()->set_beacon_cohort(beacon_cohort);
    server_context()->set_critical_selector_finder(
        new BeaconCriticalSelectorFinder(server_context()->beacon_cohort(),
                                         factory()->nonce_generator(),
                                         statistics()));
    ResetDriver();
    // Set up initial candidates for critical selector beacon
    candidates_.insert("div");
    candidates_.insert("*");
    candidates_.insert("span");
    // Write out some initial critical selectors for us to work with.
    StringSet selectors;
    selectors.insert("div");
    selectors.insert("*");
    WriteCriticalSelectorsToPropertyCache(selectors);

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

  void WriteCriticalSelectorsToPropertyCache(const StringSet& selectors) {
    factory()->mock_timer()->AdvanceMs(kMinBeaconIntervalMs);
    last_beacon_metadata_ =
        server_context()->critical_selector_finder()->
            PrepareForBeaconInsertion(candidates_, rewrite_driver());
    ASSERT_EQ(kBeaconWithNonce, last_beacon_metadata_.status);
    ResetDriver();
    server_context()->critical_selector_finder()->
        WriteCriticalSelectorsToPropertyCache(
            selectors, last_beacon_metadata_.nonce, rewrite_driver());
    page_->WriteCohort(server_context()->beacon_cohort());
  }

  GoogleString WrapForJsLoad(StringPiece orig_css) {
    return StrCat("<noscript class=\"psa_add_styles\">", orig_css,
                  "</noscript>");
  }

  GoogleString JsLoader() {
    return StrCat("<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
                  CriticalSelectorFilter::kAddStylesFunction,
                  CriticalSelectorFilter::kAddStylesInvocation,
                  "</script>");
  }

  GoogleString LoadRestOfCss(StringPiece orig_css) {
    return StrCat(WrapForJsLoad(orig_css), JsLoader());
  }

  GoogleString CssLinkHrefMedia(StringPiece url, StringPiece media) {
    return StrCat("<link rel=stylesheet href=", url,
                  " media=\"", media, "\">");
  }

  virtual bool AddHtmlTags() const { return false; }

  void ValidateRewriterLogging(RewriterHtmlApplication::Status html_status) {
    rewrite_driver()->log_record()->WriteLog();

    const LoggingInfo& logging_info =
        *rewrite_driver()->log_record()->logging_info();
    ASSERT_EQ(1, logging_info.rewriter_stats_size());
    const RewriterStats& rewriter_stats = logging_info.rewriter_stats(0);
    EXPECT_EQ("pr", rewriter_stats.id());
    EXPECT_EQ(html_status, rewriter_stats.html_status());
  }

  CriticalSelectorFilter* filter_;  // owned by the driver;
  PropertyCache* pcache_;
  PropertyPage* page_;
  StringSet candidates_;
  BeaconMetadata last_beacon_metadata_;
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
      "basic", html,
      StrCat("<head>", critical_css, "</head>",
             "<body><div>Stuff</div>",
             LoadRestOfCss(css), "</body>"));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE);
}

TEST_F(CriticalSelectorFilterTest, StylesInBody) {
  GoogleString css_a = StrCat(
      "<style>*,p {display: none; } span {display: inline; }</style>",
      CssLinkHref("a.css"));
  GoogleString css_b = CssLinkHref("b.css");

  GoogleString critical_css_a =
      "<style>*{display:none}</style>"  // from the inline
      "<style>div,*::first-letter{display:block}</style>";  // from a.css
  GoogleString critical_css_b =
      "<style>@media screen{*{margin:0px}}</style>";  // from b.css

  GoogleString html = StrCat(
      "<head></head><body>", css_a,
      "<div>Stuff</div>", css_b,
      "</body>");

  ValidateExpected(
      "style_in_body", html,
      StrCat("<head></head><body>", critical_css_a,
             "<div>Stuff</div>", critical_css_b,
             LoadRestOfCss(css_a + css_b), "</body>"));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE);
}

TEST_F(CriticalSelectorFilterTest, EmptyBlock) {
  // Do not insert empty <style> blocks. Our critical selector sets do not
  // talk about 'i' so this should do nothing..
  GoogleString css = "<style>i { font-style:italic; }</style>";

  GoogleString html = StrCat(
      "<head>",
      css,
      "</head>"
      "<body><div>Stuff</div></body>");

  ValidateExpected(
      "empty_block", html,
      StrCat("<head></head>"
             "<body><div>Stuff</div>",
             LoadRestOfCss(css), "</body>"));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE);
}

TEST_F(CriticalSelectorFilterTest, DisabledForIE) {
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kIe7UserAgent);
  GoogleString css = StrCat(
      "<style>*,p {display: none; } span {display: inline; }</style>",
      CssLinkHref("a.css"),
      CssLinkHref("b.css"));
  GoogleString html = StrCat(
      "<head>",
      css,
      "</head>"
      "<body><div>Stuff</div></body>");
  ValidateNoChanges("on_ie", html);
  ValidateRewriterLogging(RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);
}

TEST_F(CriticalSelectorFilterTest, NoScript) {
  GoogleString css1 =
      "<style>*,p {display: none; } span {display: inline; }</style>";
  GoogleString css2 =
      StrCat("<noscript>", CssLinkHref("a.css"), "</noscript>");
  GoogleString css3 =
      CssLinkHref("b.css");
  GoogleString css = StrCat(css1, css2, css3);

  GoogleString critical_css =
      "<style>*{display:none}</style>"  // from the inline
      "<noscript></noscript>"  // from a.css
      "<style>@media screen{*{margin:0px}}</style>";  // from b.css

  GoogleString html = StrCat(
      "<head>",
      css,
      "</head>"
      "<body><div>Stuff</div></body>");

  ValidateExpected(
      "noscript", html,
      StrCat("<head>", critical_css, "</head>"
             "<body><div>Stuff</div>",
             WrapForJsLoad(css1),
             css2,  // noscript, so not marked for JS load.
             WrapForJsLoad(css3),
             JsLoader(), "</body>"));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE);
}

TEST_F(CriticalSelectorFilterTest, Alternate) {
  GoogleString css = StrCat(
    "<link rel=\"alternate stylesheet\" href=\"a.css\">",
    CssLinkHref("b.css"));

  GoogleString critical_css =
    "<style>@media screen{*{margin:0px}}</style>";  // from b.css

  GoogleString html = StrCat(
      "<head>",
      css,
      "</head>"
      "<body><div>Stuff</div></body>");

  ValidateExpected(
      "alternate", html,
      StrCat("<head>", critical_css, "</head>"
             "<body><div>Stuff</div>",
             LoadRestOfCss(css), "</body>"));
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
             "<body><div>Stuff</div>",
             LoadRestOfCss(css), "</body>"));
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
      "non_screen_media", html,
      StrCat("<head>", critical_css, "</head>",
             "<body><div>Stuff</div>",
             LoadRestOfCss(css), "</body>"));
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
  WriteCriticalSelectorsToPropertyCache(selectors);

  // Note that calling ResetDriver() just resets the state in the
  // driver. Whatever has been written to the property & metadata caches so far
  // will persist. Upon rewriting, the property cache contents will be read and
  // the critical selector info in the driver will be repopulated.
  ResetDriver();
  ValidateExpected("with_div_span", StrCat(css, "<span>Foo</span>"),
                   StrCat("<style>", critical_css_div_span, "</style>",
                          "<span>Foo</span>", LoadRestOfCss(css)));

  // Now send enough beacons to eliminate support for div; only span should be
  // left.
  CriticalSelectorFinder* finder = server_context()->critical_selector_finder();
  for (int i = 0; i < finder->SupportInterval(); ++i) {
    WriteCriticalSelectorsToPropertyCache(selectors);
  }
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
      server_context()->beacon_cohort(),
      CriticalSelectorFinder::kCriticalSelectorsPropertyName);
  page_->WriteCohort(server_context()->beacon_cohort());

  ResetDriver();
  ValidateNoChanges("no_sel_info", StrCat(css, "<div>Foo</div>"));

  ValidateNoChanges("no_sel_info", StrCat(css, "<div>Foo</div>"));
  ValidateRewriterLogging(RewriterHtmlApplication::PROPERTY_CACHE_MISS);
}

TEST_F(CriticalSelectorFilterTest, ResolveUrlsProperly) {
  SetResponseWithDefaultHeaders("dir/c.css", kContentTypeCss,
                                "* { background-image: url(d.png); }", 100);
  ValidateExpected("rel_path", CssLinkHref("dir/c.css"),
                   StrCat("<style>*{background-image:url(dir/d.png)}</style>",
                          LoadRestOfCss(CssLinkHref("dir/c.css"))));
}

TEST_F(CriticalSelectorFilterTest, DoNotLazyLoadIfNothingRewritten) {
  // Make sure we don't do the whole 'lazy load rest of CSS' schpiel if we
  // did not end up changing the main CSS.
  SetupWaitFetcher();
  ValidateNoChanges("not_loaded", StrCat(CssLinkHref("a.css"),
                                         CssLinkHref("b.css")));
  CallFetcherCallbacks();
  // Skip ValidateRewriterLogging because fetcher interferes with WriteLog.
}

TEST_F(CriticalSelectorFilterTest, InlineAndAddStyleForFlushingEarly) {
  SetResponseWithDefaultHeaders("noscript.css", kContentTypeCss,
                                "noscript { border: 1px solid red; }", 100);

  GoogleString css = StrCat(
      "<style>*,p {display: none; } span {display: inline; }</style>",
      CssLinkHref("a.css"),
      "<noscript>", CssLinkHref("noscript.css"), "</noscript>",
      CssLinkHref("b.css"));

  GoogleString critical_css =
      "<style>*{display:none}</style>"  // from the inline
      "<style data-pagespeed-flush-style=\"0\">"
          "div,*::first-letter{display:block}</style>"  // from a.css
      "<noscript></noscript>"  // from noscript.css
      "<style data-pagespeed-flush-style=\"0\">"
          "@media screen{*{margin:0px}}</style>";  // from b.css

  GoogleString input_html = StrCat(
      "<head>",
      css,
      "</head>"
      "<body><div>Stuff</div></body>");

  options()->ClearSignatureForTesting();
  options()->set_enable_flush_early_critical_css(true);
  options()->ComputeSignature();
  rewrite_driver()->set_flushing_early(true);
  // The "flushing_early" driver expects the critical CSS without the
  // full CSS at the bottom (no LoadRestOfCss).  This allows the flush
  // early flow to collect the CSS to flush.  The filter output is not
  // sent to the end user. (The "flushed_early" driver filters the
  // content that is sent to the user.)
  ValidateExpected(
      "flushing_early", input_html,
      StrCat("<head>", critical_css, "</head>",
             "<body><div>Stuff</div></body>"));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE);
}

TEST_F(CriticalSelectorFilterTest, InlineFlushEarly) {
  SetResponseWithDefaultHeaders("noscript.css", kContentTypeCss,
                                "noscript { border: 1px solid red; }", 100);

  GoogleString css_script = StrCat(
      "<style>*,p {display: none; } span {display: inline; }</style>",
      CssLinkHref("a.css"),
      CssLinkHrefMedia("b.css", "print,screen"));
  GoogleString css_noscript = StrCat(
      "<noscript>", CssLinkHref("noscript.css"), "</noscript>");

  GoogleString a_url = "http://test.com/a.css";
  GoogleString b_url = "http://test.com/b.css";
  GoogleString a_style_id =
      rewrite_driver()->server_context()->hasher()->Hash(a_url);
  GoogleString b_style_id =
      rewrite_driver()->server_context()->hasher()->Hash(b_url);
  GoogleString critical_css = StrCat(
      "<style>*{display:none}</style>"  // from the inline
      "<script id=\"psa_flush_style_early\" pagespeed_no_defer=\"\""
      " type=\"text/javascript\">",
      CriticalSelectorFilter::kApplyFlushEarlyCss,
      "</script>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      StringPrintf(CriticalSelectorFilter::kInvokeFlushEarlyCssTemplate,
                   a_style_id.c_str(), "" /* media */),
      "</script>"  // from a.css
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      StringPrintf(CriticalSelectorFilter::kInvokeFlushEarlyCssTemplate,
                   b_style_id.c_str(), "screen" /* media */),
      "</script>"  // from b.css
      "<noscript></noscript>");  // from noscript.css

  GoogleString html = StrCat(
      "<head>",
      css_script,
      css_noscript,
      "</head>"
      "<body><div>Stuff</div></body>");

  options()->ClearSignatureForTesting();
  options()->set_enable_flush_early_critical_css(true);
  options()->ComputeSignature();

  rewrite_driver()->flush_early_info()->set_resource_html(
      CssLinkHref("\"" + a_url + "\"") +
      CssLinkHref("\"" + b_url + "\""));

  rewrite_driver()->set_flushed_early(true);

  ValidateExpected(
      "flushed_early", html,
      StrCat("<head>", critical_css, "</head>",
             "<body><div>Stuff</div>",
             WrapForJsLoad(css_script),
             css_noscript,
             JsLoader(),
             "</body>"));
  ValidateRewriterLogging(RewriterHtmlApplication::ACTIVE);
}

class CriticalSelectorWithRewriteCssFilterTest
    : public CriticalSelectorFilterTest {
 protected:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kRewriteCss);
    CriticalSelectorFilterTest::SetUp();
  }
};

TEST_F(CriticalSelectorWithRewriteCssFilterTest, ProperlyUsedOptimized) {
  // Make sure that the lazy loading code for rest of CSS actually uses
  // optimized resources.
  GoogleString css = StrCat(
      CssLinkHref("a.css"),
      CssLinkHref("b.css"));

  GoogleString critical_css =
      "<style>div,*::first-letter{display:block}</style>"  // from a.css
      "<style>@media screen{*{margin:0px}}</style>";  // from b.css

  GoogleString optimized_css = StrCat(
      CssLinkHref(Encode("", "cf", "0", "a.css", "css")),
      CssLinkHref(Encode("", "cf", "0", "b.css", "css")));
  GoogleString html = StrCat(
      "<head>",
      css,
      "</head>"
      "<body><div>Stuff</div></body>");

  ValidateExpected("with_rewrite_css",
                   html,
                   StrCat("<head>", critical_css, "</head>"
                          "<body><div>Stuff</div>",
                          LoadRestOfCss(optimized_css), "</body>"));
}

class CriticalSelectorWithCombinerFilterTest
    : public CriticalSelectorFilterTest {
 protected:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kCombineCss);
    CriticalSelectorFilterTest::SetUp();
  }
};

TEST_F(CriticalSelectorWithCombinerFilterTest, Interaction) {
  GoogleString css = StrCat(
      CssLinkHref("a.css"),
      CssLinkHref("b.css"));

  // Only one <style> element since combine_css ran before us.
  GoogleString critical_css =
      "<style>div,*::first-letter{display:block}"  // from a.css
      "@media screen{*{margin:0px}}</style>";  // from b.css

  GoogleString combined_url = Encode("", "cc", "0",
                                     MultiUrl("a.css", "b.css"), "css");

  ValidateExpected("with_combiner",
                   css,
                   StrCat(critical_css,
                          LoadRestOfCss(CssLinkHref(combined_url))));
}

TEST_F(CriticalSelectorWithCombinerFilterTest, ResolveWhenCombineAcrossPaths) {
  // Make sure we get proper URL resolution when doing combine-across-paths.
  SetResponseWithDefaultHeaders("dir/a.css", kContentTypeCss,
                                "* { background-image: url(/dir/d.png); }",
                                100);
  GoogleString css = StrCat(
      CssLinkHref("dir/a.css"),
      CssLinkHref("b.css"));

    // Only one <style> element since combine_css ran before us.
  GoogleString critical_css =
      "<style>*{background-image:url(dir/d.png)}"  // from dir/a.css
      "@media screen{*{margin:0px}}</style>";  // from b.css

  GoogleString combined_url = "dir,_a.css+b.css.pagespeed.cc.0.css";

  ValidateExpected("with_combiner_rel",
                   css,
                   StrCat(critical_css,
                          LoadRestOfCss(CssLinkHref(combined_url))));
}

class CriticalSelectorWithInlineCssFilterTest
    : public CriticalSelectorFilterTest {
 protected:
  virtual void SetUpBeforeSelectorsFilter() {}

  // Add the inline css filter after the critical selector filter so
  // it matches the order that is in RewriteDriver.
  virtual void SetUpAfterSelectorsFilter() {
    options()->EnableFilter(RewriteOptions::kInlineCss);
    rewrite_driver()->AddFilters();
  }
};

TEST_F(CriticalSelectorWithInlineCssFilterTest, AvoidTryingToInlineTwice) {
  // Verify that the critical selector filter stops further processing of
  // the rewrite contexts.
  GoogleString css = StrCat(
      CssLinkHref("a.css"),
      CssLinkHref("b.css"));
  GoogleString critical_css =
      "<style>div,*::first-letter{display:block}</style>"  // from a.css
      "<style>@media screen{*{margin:0px}}</style>";  // from b.css

  GoogleString input_html = StrCat(
      "<head>", css, "</head>"
      "<body><div>Stuff</div></body>");
  GoogleString expected_html = StrCat(
      "<head>", critical_css, "</head>"
      "<body><div>Stuff</div>", LoadRestOfCss(css), "</body>");
  ValidateExpected("with_inline_css", input_html, expected_html);
}

}  // namespace

}  // namespace net_instaweb
