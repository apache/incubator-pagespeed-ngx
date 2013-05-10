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
    "OK/*{display:|OK/div{displa/rel=stylesheet|ParseError//rel=stylesheet|"
    "FetchError//rel=stylesheet|ResourceError/|";

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
    CssMinify::Stylesheet(*stylesheet, &write_out, driver_->message_handler());
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
        return "ParseError";
      case kSummaryResourceCreationFailed:
        return "ResourceError";
      case kSummaryInputUnavailable:
        return "FetchError";
      case kSummarySlotRemoved:
        return "SlotRemoved";
    }
  };

  virtual void RenderSummary(int pos,
                             HtmlElement* element,
                             HtmlCharactersNode* char_node) {
    if (!render_summaries_in_place_) {
      return;
    }

    const SummaryInfo& summary = GetSummaryForStyle(pos);

    if (char_node != NULL) {
      *char_node->mutable_contents() = summary.data;
    } else {
      // Replace link with style. Note: real one should also keep media,
      // test code does not have to.
      HtmlElement* style_element = driver_->NewElement(NULL, HtmlName::kStyle);
      driver_->InsertNodeBeforeNode(element, style_element);

      HtmlCharactersNode* content =
          driver_->NewCharactersNode(style_element, summary.data);
      driver_->AppendChild(style_element, content);
      EXPECT_TRUE(driver_->DeleteNode(element));
    }
  }

  virtual void WillNotRenderSummary(int pos,
                                    HtmlElement* element,
                                    HtmlCharactersNode* char_node) {
    // Note that these should not normally mutate the DOM, we only
    // get away with this because the tests we use this in don't really do
    // any flushing.
    if (!will_not_render_summaries_in_place_) {
      return;
    }

    const SummaryInfo& sum = GetSummaryForStyle(pos);
    GoogleString annotation = StrCat("WillNotRender:", IntegerToString(pos),
                                     " --- ", EncodeState(sum.state));
    driver_->InsertNodeBeforeNode(
        element, driver_->NewCommentNode(NULL, annotation));
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

TEST_F(CssSummarizerBaseTest, BasicOperationWhitespace) {
  GoogleString expected =
      FullTest("whitespace", "<body> <p>some content</p> ", "</body>\n</html>");
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
  FullTest("will_not_render", "", "");
  EXPECT_STREQ(StrCat("<html>\n",
                      "<style>* {display: none; }</style>",
                      CssLinkHref("a.css"),
                      StrCat("<!--WillNotRender:2 --- ParseError-->",
                             CssLinkHref("b.css")),
                      StrCat("<!--WillNotRender:3 --- FetchError-->",
                             CssLinkHref("404.css")),
                      StrCat("<!--WillNotRender:4 --- ResourceError-->",
                             CssLinkHref("http://evil.com/d.css")),
                      StrCat("<!--", kExpectedResult, "-->")),
               output_buffer_);
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
        "<link rel=stylesheet href='b.css' pagespeed_no_defer>"
        "<link rel=stylesheet href='a.css'>");
  EXPECT_STREQ("<html>\n"
               "<style>*{backgrou</style>"
               "<style pagespeed_no_defer>div {display:none;}</style>"
               "<link rel=stylesheet href='b.css' pagespeed_no_defer>"
               "<style>div{displa</style>\n"
               "<!--OK/*{backgrou|OK/div{displa/rel=stylesheet|--></html>",
               output_buffer_);
}

TEST_F(CssSummarizerBaseTest, NoBody) {
  GoogleString expected =
      FullTest("no_body", "some content without body tag\n", "</html>");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
}

TEST_F(CssSummarizerBaseTest, TwoBodies) {
  GoogleString expected =
      FullTest("two_bodies",
               "<body>First body</body><body>Second body", "</body></html>");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
}

TEST_F(CssSummarizerBaseTest, StuffAfterBody) {
  GoogleString expected =
      FullTest("stuff_after_body",
               "<body>Howdy!</body><p>extra stuff</p>", "</html>");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
}

TEST_F(CssSummarizerBaseTest, StuffAfterHtml) {
  GoogleString expected =
      FullTest("stuff_after_html",
               "<body>Howdy!</body></html>extra stuff", "");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
}

TEST_F(CssSummarizerBaseTest, FlushAfterBody) {
  // Even though we flush between </body> and </html>, !IsRewritable(</html>)
  // (since the inital <html> was in a different flush window) so we inject at
  // end of document.
  GoogleString expected =
      FlushTest("flush_after_body",
                "<body> some content </body>", "</html>", "");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
}

TEST_F(CssSummarizerBaseTest, FlushDuringBody) {
  // As above we end up inserting at end.
  GoogleString expected =
      FlushTest("flush_during_body",
                "<body> partial", " content </body></html>", "");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
}

TEST_F(CssSummarizerBaseTest, FlushBeforeBody) {
  // Here we can insert at end of body.
  GoogleString expected =
      FlushTest("flush_before_body",
                "", "<body> post-flush content ", "</body></html>");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
}

TEST_F(CssSummarizerBaseTest, FlushAtEnd) {
  // This causes us to append to the end of document after the flush.
  GoogleString expected =
      FlushTest("flush_at_end",
                "<body>pre-flush content</body></html>", "", "");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
}

TEST_F(CssSummarizerBaseTest, EnclosedBody) {
  GoogleString expected =
      FullTest("enclosed_body",
               "<noscript><body>no script body</body></noscript>", "</html>");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
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
  GoogleString combined_url = Encode(kTestDomain, "cc", "0",
                                     MultiUrl("a.css", "a2.css"), "css");

  Parse("with_combine", StrCat(CssLinkHref("a.css"), CssLinkHref("a2.css")));
  EXPECT_EQ(StrCat("<html>\n", CssLinkHref(combined_url),
                   "\n<!--OK/div{displa/rel=stylesheet|"
                   "SlotRemoved//rel=stylesheet|--></html>"),
            output_buffer_);
}

TEST_F(CssSummarizerBaseWithCombinerFilterTest, BaseAcrossPaths) {
  // Make sure base is updated if a previous filter moves a resource across
  // directories.
  filter_->set_include_base(true);
  SetResponseWithDefaultHeaders("b/a2.css", kContentTypeCss,
                                 "span { display: inline; }", 100);
  GoogleString combined_url =
      StrCat(kTestDomain, "b,_a2.css+a.css.pagespeed.cc.0.css");

  Parse("base_accross_paths",
        StrCat(CssLinkHref("b/a2.css"), CssLinkHref("a.css")));
  EXPECT_EQ(
      StrCat("<html>\n", CssLinkHref(combined_url), "\n",
             StrCat("<!--OK/span{displ/rel=stylesheet/base=", combined_url),
             StrCat("|SlotRemoved//rel=stylesheet/base=", kTestDomain, "a.css"),
             "|--></html>"),
      output_buffer_);
}

}  // namespace

}  // namespace net_instaweb
