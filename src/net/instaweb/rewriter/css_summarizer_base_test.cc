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

#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

namespace net_instaweb {

namespace {

const char kExpectedResult[] =
    "OK/*{display:|OK/div{displa|ParseError/|FetchError/|ResourceError/|";

// Extracts first 10 characters of minified form of every stylesheet.
class MinifyExcerptFilter : public CssSummarizerBase {
 public:
  explicit MinifyExcerptFilter(RewriteDriver* driver)
      : CssSummarizerBase(driver) {}

  virtual const char* Name() const { return "Minify10"; }
  virtual const char* id() const { return "csr"; }

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
    }
  };

  virtual void SummariesDone() {
    result_.clear();
    for (int i = 0; i < NumStyles(); ++i) {
      const SummaryInfo& sum = GetSummaryForStyle(i);
      StrAppend(&result_, EncodeState(sum.state), "/", sum.data, "|");
    }
    InjectSummaryData(driver()->NewCommentNode(NULL, result_));
  }

  const GoogleString& result() { return result_; }

 private:
  GoogleString result_;
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
      FullTest("basic", "<body> <p>some content</p> ", "</body>\n</html>");
  EXPECT_STREQ(expected, output_buffer_);
  EXPECT_STREQ(kExpectedResult, filter_->result());
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

}  // namespace

}  // namespace net_instaweb
