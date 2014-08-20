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

#include "net/instaweb/rewriter/public/css_summarizer_base.h"

#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

const char kExpectedResult[] =
    "OK/*{display:|OK/div{displa/rel=stylesheet|ParseOrCloseStyleTagError//"
    "rel=stylesheet|ParseOrCloseStyleTagError//rel=stylesheet|"
    "ParseOrCloseStyleTagError//rel=stylesheet|FetchError//rel=stylesheet|"
    "ResourceError/|";

// Extracts first 10 characters of minified form of every stylesheet.
class MinifyExcerptFilter : public CssSummarizerBase {
 public:
  explicit MinifyExcerptFilter(RewriteDriver* driver)
      : CssSummarizerBase(driver),
        render_summaries_in_place_(false),
        will_not_render_summaries_in_place_(false),
        include_base_(false) {}

  virtual const char* Name() const { return "Minify10"; }
  virtual const char* id() const { return "csr"; }

  virtual bool MustSummarize(HtmlElement* element) const {
    return (!element->FindAttribute(HtmlName::kPagespeedNoDefer));
  }

  virtual void Summarize(Css::Stylesheet* stylesheet,
                         GoogleString* out) const {
    StringWriter write_out(out);
    CssMinify::Stylesheet(*stylesheet, &write_out, driver()->message_handler());
    if (out->length() > 10) {
      out->resize(10);
    }
  }

  GoogleString EncodeState(SummaryState state) {
    switch (state) {
      case kSummaryOk:
        return "OK";
      case kSummaryStillPending:
        return "Pending";
      case kSummaryCssParseError:
        return "ParseOrCloseStyleTagError";
      case kSummaryResourceCreationFailed:
        return "ResourceError";
      case kSummaryInputUnavailable:
        return "FetchError";
      case kSummarySlotRemoved:
        return "SlotRemoved";
    }
  }

  virtual void RenderSummary(int pos,
                             HtmlElement* element,
                             HtmlCharactersNode* char_node,
                             bool* is_element_deleted) {
    if (!render_summaries_in_place_) {
      return;
    }

    const SummaryInfo& summary = GetSummaryForStyle(pos);

    if (char_node != NULL) {
      *char_node->mutable_contents() = summary.data;
    } else {
      // Replace link with style. Note: real one should also keep media,
      // test code does not have to.
      HtmlElement* style_element = driver()->NewElement(NULL, HtmlName::kStyle);
      driver()->InsertNodeBeforeNode(element, style_element);

      HtmlCharactersNode* content =
          driver()->NewCharactersNode(style_element, summary.data);
      driver()->AppendChild(style_element, content);
      EXPECT_TRUE(driver()->DeleteNode(element));
      *is_element_deleted = true;
    }
  }

  virtual void WillNotRenderSummary(int pos,
                                    HtmlElement* element,
                                    HtmlCharactersNode* char_node,
                                    bool* is_element_deleted) {
    // Note that these should not normally mutate the DOM, we only
    // get away with this because the tests we use this in don't really do
    // any flushing.
    if (!will_not_render_summaries_in_place_) {
      return;
    }

    const SummaryInfo& sum = GetSummaryForStyle(pos);
    GoogleString annotation = StrCat("WillNotRender:", IntegerToString(pos),
                                     " --- ", EncodeState(sum.state));
    driver()->InsertNodeBeforeNode(
        element, driver()->NewCommentNode(NULL, annotation));
  }

  virtual void SummariesDone() {
    result_.clear();
    for (int i = 0; i < NumStyles(); ++i) {
      const SummaryInfo& sum = GetSummaryForStyle(i);
      StrAppend(&result_, EncodeState(sum.state), "/", sum.data,
                (sum.is_inside_noscript ? "/noscr" : ""),
                (sum.rel.empty() ? "" : StrCat("/rel=", sum.rel)),
                (include_base_ ? StrCat("/base=", sum.base) : ""),
                "|");
    }
    InsertNodeAtBodyEnd(driver()->NewCommentNode(NULL, result_));
  }

  const GoogleString& result() { return result_; }

  // Whether we should note the RenderSummary calls in place.
  void set_render_summaries_in_place(bool x) {
    render_summaries_in_place_ = x;
  }

  // Whether we should note the WillNotRenderSummary calls in place.
  void set_will_not_render_summaries_in_place(bool x) {
    will_not_render_summaries_in_place_ = x;
  }

  // Whether we should include the base URL in the output string we compute.
  void set_include_base(bool x) {
    include_base_ = x;
  }

 private:
  GoogleString result_;
  bool render_summaries_in_place_;
  bool will_not_render_summaries_in_place_;
  bool include_base_;
};

class CssSummarizerBaseTest : public RewriteTestBase {
 public:
  CssSummarizerBaseTest()
      : head_(StrCat("<html>\n",
                     "<style>* {display: none; }</style>",
                     CssLinkHref("a.css"),  // ok
                     CssLinkHref("b.css"),  // parse error
                     CssLinkHref("c.css"),  // parse error due to bad URL
                     CssLinkHref("close_style_tag.css"),  // closing style tag
                     CssLinkHref("404.css"),  // fetch error
                     CssLinkHref("http://evil.com/d.css"))) { }
  virtual ~CssSummarizerBaseTest() { }

 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    rewrite_driver()->AddFilters();
    filter_ = new MinifyExcerptFilter(rewrite_driver());
    rewrite_driver()->AppendOwnedPreRenderFilter(filter_);
    server_context()->ComputeSignature(options());

    // Valid CSS.
    SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                  "div { display: block; }", 100);

    // Parse error.
    SetResponseWithDefaultHeaders("b.css", kContentTypeCss,
                                  "div { ", 100);
    SetResponseWithDefaultHeaders(
        "c.css", kContentTypeCss,
        ".z{background-image:url(\"</style>\");", 100);

    // Contents that include a closing style tag.
    SetResponseWithDefaultHeaders("close_style_tag.css",
                                  kContentTypeCss,
                                  ".x </style> {color: white }", 100);

    // Permit testing a 404.
    SetFetchFailOnUnexpected(false);

    // An inline div? evil indeed.
    SetResponseWithDefaultHeaders("http://evil.com/d.css", kContentTypeCss,
                                  "div { display: inline; }", 100);
  }

  void StartTest(StringPiece name, StringPiece pre_comment) {
    SetupWriter();
    GoogleString url = StrCat(kTestDomain, name);
    rewrite_driver()->StartParse(url);
    rewrite_driver()->ParseText(head_);
    rewrite_driver()->ParseText(pre_comment);
  }

  const GoogleString FinishTest(
      StringPiece pre_comment, StringPiece post_comment) {
    const GoogleString expected_html = StrCat(
        head_, pre_comment, "<!--", kExpectedResult, "-->", post_comment);
    rewrite_driver()->ParseText(post_comment);
    rewrite_driver()->FinishParse();
    return expected_html;
  }

  const GoogleString FullTest(
      StringPiece name, StringPiece pre_comment, StringPiece post_comment) {
    StartTest(name, pre_comment);
    return FinishTest(pre_comment, post_comment);
  }

  const GoogleString FlushTest(
      StringPiece name, StringPiece pre_flush,
      StringPiece pre_comment, StringPiece post_comment) {
    StartTest(name, pre_flush);
    rewrite_driver()->Flush();
    rewrite_driver()->ParseText(pre_comment);
    GoogleString full_pre_comment = StrCat(pre_flush, pre_comment);
    return FinishTest(full_pre_comment, post_comment);
  }

  void VerifyUnauthNotRendered(StringPiece summary_comment) {
    FullTest("will_not_render", "", "");
    EXPECT_STREQ(
        StrCat("<html>\n"
               "<style>* {display: none; }</style>",
               CssLinkHref("a.css"),
               StrCat("<!--WillNotRender:2 --- ParseOrCloseStyleTagError-->",
                       CssLinkHref("b.css"),
                      "<!--WillNotRender:3 --- ParseOrCloseStyleTagError-->",
                       CssLinkHref("c.css")),
               StrCat("<!--WillNotRender:4 --- ParseOrCloseStyleTagError-->",
                       CssLinkHref("close_style_tag.css"),
                      "<!--WillNotRender:5 --- FetchError-->",
                       CssLinkHref("404.css")),
               StrCat("<!--WillNotRender:6 --- ResourceError-->",
                       CssLinkHref("http://evil.com/d.css")),
               summary_comment,
               StrCat("<!--", kExpectedResult, "-->")),
        output_buffer_);
  }

  MinifyExcerptFilter* filter_;  // owned by the driver;
  const GoogleString head_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CssSummarizerBaseTest);
};

TEST_F(CssSummarizerBaseTest, BasicOperation) {
  GoogleString expected =
      FullTest("basic", "<body> <p>some content</p> ", "</body></html>");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());

  // Re-test to make sure we behave OK with the result cached.
  expected = FullTest("basic", "<body> <p>some content</p> ", "</body></html>");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
}

TEST_F(CssSummarizerBaseTest, RenderSummary) {
  filter_->set_render_summaries_in_place(true);
  Parse("link", StrCat(CssLinkHref("a.css"),
                       "<style>* { background: blue; }</style>"));
  EXPECT_STREQ("<html>\n<style>div{displa</style><style>*{backgrou</style>\n"
               "<!--OK/div{displa/rel=stylesheet|"
                   "OK/*{backgrou|--></html>", output_buffer_);
}

TEST_F(CssSummarizerBaseTest, WillNotRenderSummary) {
  filter_->set_will_not_render_summaries_in_place(true);
  VerifyUnauthNotRendered(/* summary_comment= */ "");
}

TEST_F(CssSummarizerBaseTest, WillNotRenderSummaryWithUnauthEnabled) {
  filter_->set_will_not_render_summaries_in_place(true);
  options()->ClearSignatureForTesting();
  options()->AddInlineUnauthorizedResourceType(semantic_type::kStylesheet);
  server_context()->ComputeSignature(options());
  VerifyUnauthNotRendered(/* summary_comment= */ "");
}

TEST_F(CssSummarizerBaseTest, WillNotRenderSummaryWithDebug) {
  filter_->set_will_not_render_summaries_in_place(true);
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kDebug);
  server_context()->ComputeSignature(options());
  const char kDebugSummary[] =
      "<!--Summary computation status for Minify10\n"
      "Resource 0 http://test.com/will_not_render:2: Computed OK\n"
      "Resource 1 http://test.com/a.css: Computed OK\n"
      "Resource 2 http://test.com/b.css: "
      "Unrecoverable CSS parse error or resource contains closing style tag\n"
      "Resource 3 http://test.com/c.css: "
      "Unrecoverable CSS parse error or resource contains closing style tag\n"
      "Resource 4 http://test.com/close_style_tag.css: "
      "Unrecoverable CSS parse error or resource contains closing style tag\n"
      "Resource 5 http://test.com/404.css: "
      "Fetch failed or resource not publicly cacheable\n"
      "Resource 6 http://evil.com/d.css: Cannot create resource: either its "
      "domain is unauthorized and InlineUnauthorizedResources is not enabled, "
      "or it cannot be fetched (check the server logs)\n"
      "-->";
  VerifyUnauthNotRendered(kDebugSummary);
}

TEST_F(CssSummarizerBaseTest, WillNotRenderSummaryWait) {
  filter_->set_will_not_render_summaries_in_place(true);
  SetupWaitFetcher();
  Parse("link", CssLinkHref("a.css"));
  EXPECT_STREQ(StrCat("<html>\n",
                      "<!--WillNotRender:0 --- Pending-->",
                      CssLinkHref("a.css"),
                      "\n</html>"),
               output_buffer_);
  CallFetcherCallbacks();
}

TEST_F(CssSummarizerBaseTest, Base) {
  filter_->set_include_base(true);
  GoogleString css =
      StrCat(CssLinkHref("a.css"), "<style>*{display:block;}</style>");
  Parse("base", css);
  EXPECT_STREQ(
      StrCat("<html>\n", css, "\n",
             StrCat("<!--OK/div{displa/rel=stylesheet/base=",
                    kTestDomain, "a.css"),
             StrCat("|OK/*{display:/base=", kTestDomain, "base.html|-->"),
             "</html>"),
      output_buffer_);
}

TEST_F(CssSummarizerBaseTest, AlternateHandling) {
  // CssSummarizerBase itself handles alternate stylesheets, just keeps
  // the rel around inside the SummaryInfo
  Parse("alternate", "<link rel=\"stylesheet alternate\" href=\"a.css\">");
  EXPECT_STREQ("OK/div{displa/rel=stylesheet alternate|", filter_->result());
}

TEST_F(CssSummarizerBaseTest, NoScriptHandling) {
  Parse("ns", StrCat(CssLinkHref("a.css"),
                     "<noscript>", CssLinkHref("a.css"), "</noscript>"));
  EXPECT_STREQ("OK/div{displa/rel=stylesheet|"
                   "OK/div{displa/noscr/rel=stylesheet|",
               filter_->result());
}

TEST_F(CssSummarizerBaseTest, IgnoreNonSummarizable) {
  filter_->set_render_summaries_in_place(true);
  Parse("non-summarizable",
        "<style>* { background: blue; }</style>"
        "<style pagespeed_no_defer>div {display:none;}</style>"
        "<style scoped>p {display:none;}</style>"
        "<link rel=stylesheet href='b.css' pagespeed_no_defer>"
        "<link rel=stylesheet href='a.css'>");
  EXPECT_STREQ("<html>\n"
               "<style>*{backgrou</style>"
               "<style pagespeed_no_defer>div {display:none;}</style>"
               "<style scoped>p {display:none;}</style>"
               "<link rel=stylesheet href='b.css' pagespeed_no_defer>"
               "<style>div{displa</style>\n"
               "<!--OK/*{backgrou|OK/div{displa/rel=stylesheet|--></html>",
               output_buffer_);
}

class CssSummarizerBaseWithCombinerFilterTest : public CssSummarizerBaseTest {
 protected:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kCombineCss);
    CssSummarizerBaseTest::SetUp();
    SetHtmlMimetype();  // no <link />, just <link>
  }
};

TEST_F(CssSummarizerBaseWithCombinerFilterTest, Interaction) {
  SetResponseWithDefaultHeaders("a2.css", kContentTypeCss,
                                 "span { display: inline; }", 100);
  GoogleString combined_url = Encode("", "cc", "0",
                                     MultiUrl("a.css", "a2.css"), "css");

  Parse("with_combine", StrCat(CssLinkHref("a.css"), CssLinkHref("a2.css")));
  EXPECT_EQ(StrCat("<html>\n", CssLinkHref(combined_url),
                   "\n<!--OK/div{displa/rel=stylesheet|"
                   "SlotRemoved//rel=stylesheet|--></html>"),
            output_buffer_);
}

TEST_F(CssSummarizerBaseWithCombinerFilterTest, InteractionWithFlush) {
  // Make sure that SummariesDone is called once only, at the actual end of the
  // document, and not for every flush window.
  SetResponseWithDefaultHeaders("a2.css", kContentTypeCss,
                                 "span { display: inline; }", 100);
  GoogleString combined_url = Encode("", "cc", "0",
                                     MultiUrl("a.css", "a2.css"), "css");
  GoogleString css = StrCat(CssLinkHref("a.css"), CssLinkHref("a2.css"));

  SetupWriter();
  html_parse()->StartParse(StrCat(kTestDomain, "example.html"));
  html_parse()->ParseText(css);
  html_parse()->Flush();
  html_parse()->ParseText(css);
  html_parse()->FinishParse();

  // Should only see the comment once, since SummariesDone is supposed to be
  // called only at document end.
  EXPECT_EQ(StrCat(CssLinkHref(combined_url), CssLinkHref(combined_url),
                   StrCat("<!--",
                          "OK/div{displa/rel=stylesheet|",
                          "SlotRemoved//rel=stylesheet|",
                          "OK/div{displa/rel=stylesheet|",
                          "SlotRemoved//rel=stylesheet|",
                          "-->")),
            output_buffer_);
}

TEST_F(CssSummarizerBaseWithCombinerFilterTest, BaseAcrossPaths) {
  // Make sure base is updated if a previous filter moves a resource across
  // directories.
  filter_->set_include_base(true);
  SetResponseWithDefaultHeaders("b/a2.css", kContentTypeCss,
                                 "span { display: inline; }", 100);
  GoogleString combined_url = "b,_a2.css+a.css.pagespeed.cc.0.css";

  Parse("base_accross_paths",
        StrCat(CssLinkHref("b/a2.css"), CssLinkHref("a.css")));
  EXPECT_EQ(StrCat(
      "<html>\n", CssLinkHref(combined_url), "\n"
      "<!--OK/span{displ/rel=stylesheet/base=", kTestDomain, combined_url,
      "|SlotRemoved//rel=stylesheet/base=", kTestDomain, "a.css"
      "|--></html>"),
            output_buffer_);
}

}  // namespace

}  // namespace net_instaweb
