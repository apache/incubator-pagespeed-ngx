/*
 * Copyright 2010 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the css filter

#include "net/instaweb/rewriter/public/css_tag_scanner.h"

#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/null_writer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

namespace {

const char kUrl[] = "http://www.myhost.com/static/mycss.css";
const char kPrint[] = "print";
const char kStylesheet[] = "stylesheet";
const char kAlternateStylesheet[] = "alternate stylesheet";

class CssTagScannerTest : public testing::Test {
 protected:
  CssTagScannerTest()
      : html_parse_(&message_handler_),
        link_(NULL), href_(NULL), media_(NULL) {
  }

  void SetUp() {
    link_ = html_parse_.NewElement(NULL, HtmlName::kLink);
    // Set up link_ to a reasonable (and legal) start state.
    html_parse_.AddAttribute(link_, HtmlName::kRel, "stylesheet");
    html_parse_.AddAttribute(link_, HtmlName::kHref, kUrl);
  }

  void CheckGurlResolve(const GoogleUrl& base, const char* relative_path,
                        const char* abs_path) {
    GoogleUrl resolved(base, relative_path);
    EXPECT_TRUE(resolved.IsWebValid());
    EXPECT_STREQ(resolved.Spec(), abs_path);
  }

  GoogleMessageHandler message_handler_;

 protected:
  HtmlParse html_parse_;
  HtmlElement* link_;
  HtmlElement::Attribute* href_;
  const char* media_;
  StringPieceVector nonstandard_attributes_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CssTagScannerTest);
};

// This test verifies that we understand how Resolve works.
TEST_F(CssTagScannerTest, TestGurl) {
  GoogleUrl base_slash("http://base/");
  EXPECT_TRUE(base_slash.IsWebValid());
  CheckGurlResolve(base_slash, "r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_slash, "/r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_slash, "../r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_slash, "./r/path.ext", "http://base/r/path.ext");

  GoogleUrl base_no_slash("http://base");
  EXPECT_TRUE(base_no_slash.IsWebValid());
  CheckGurlResolve(base_no_slash, "r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_no_slash, "/r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_no_slash, "../r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_no_slash, "./r/path.ext", "http://base/r/path.ext");
}

// This test makes sure we can identify a few different forms of CSS tags we've
// seen.
TEST_F(CssTagScannerTest, MinimalOK) {
  // We can parse css if it has only href= and rel=stylesheet attributes.
  EXPECT_TRUE(CssTagScanner::ParseCssElement(link_, &href_, &media_,
                                             &nonstandard_attributes_));
  EXPECT_STREQ("", media_);
  EXPECT_STREQ(kUrl, href_->DecodedValueOrNull());
  EXPECT_EQ(0, nonstandard_attributes_.size());
}

TEST_F(CssTagScannerTest, NonstandardAttributeOK) {
  // Add a nonstandard attribute.
  html_parse_.AddAttribute(link_, HtmlName::kOther, "value");
  EXPECT_TRUE(CssTagScanner::ParseCssElement(link_, &href_, &media_,
                                             &nonstandard_attributes_));
  EXPECT_STREQ("", media_);
  EXPECT_STREQ(kUrl, href_->DecodedValueOrNull());
  EXPECT_EQ(1, nonstandard_attributes_.size());
  EXPECT_EQ("other", nonstandard_attributes_[0]);
}

TEST_F(CssTagScannerTest, WithTypeOK) {
  // Type=text/css works
  html_parse_.AddAttribute(link_, HtmlName::kType, "text/css");
  EXPECT_TRUE(CssTagScanner::ParseCssElement(link_, &href_, &media_,
                                             &nonstandard_attributes_));
  EXPECT_STREQ("", media_);
  EXPECT_STREQ(kUrl, href_->DecodedValueOrNull());
  EXPECT_EQ(0, nonstandard_attributes_.size());
}

TEST_F(CssTagScannerTest, BadTypeFail) {
  // Types other than text/css don't work.
  html_parse_.AddAttribute(link_, HtmlName::kType, "text/plain");
  EXPECT_FALSE(CssTagScanner::ParseCssElement(link_, &href_, &media_));
}

TEST_F(CssTagScannerTest, WithMediaOK) {
  // Add a media attribute.  It should still pass, yielding media.
  html_parse_.AddAttribute(link_, HtmlName::kMedia, kPrint);
  EXPECT_TRUE(CssTagScanner::ParseCssElement(link_, &href_, &media_,
                                             &nonstandard_attributes_));
  EXPECT_STREQ(kPrint, media_);
  EXPECT_FALSE(css_util::CanMediaAffectScreen(media_));
  EXPECT_STREQ(kUrl, href_->DecodedValueOrNull());
  EXPECT_EQ(0, nonstandard_attributes_.size());
}

TEST_F(CssTagScannerTest, DoubledHrefFail) {
  // We used to just count href and rel attributes; if we double the href
  // attribute we ought to fail.  We *could* succeed if the urls match, but it's
  // not worth the bother.
  HtmlElement::Attribute* attr = link_->FindAttribute(HtmlName::kHref);
  link_->AddAttribute(*attr);
  EXPECT_FALSE(CssTagScanner::ParseCssElement(link_, &href_, &media_));
}

TEST_F(CssTagScannerTest, MissingRelFail) {
  // Removal of rel= attribute.
  link_->DeleteAttribute(HtmlName::kRel);
  EXPECT_FALSE(CssTagScanner::ParseCssElement(link_, &href_, &media_));
}

TEST_F(CssTagScannerTest, AlternateRelFail) {
  // rel="alternate stylesheet" should fail.
  link_->DeleteAttribute(HtmlName::kRel);
  html_parse_.AddAttribute(link_, HtmlName::kRel, kAlternateStylesheet);
  EXPECT_FALSE(CssTagScanner::ParseCssElement(link_, &href_, &media_));
}

TEST_F(CssTagScannerTest, MissingRelDoubledHrefFail) {
  // Removal of rel= attribute and doubling of href.  This used to succeed since
  // we just counted to 2.
  link_->DeleteAttribute(HtmlName::kRel);
  HtmlElement::Attribute* attr = link_->FindAttribute(HtmlName::kHref);
  link_->AddAttribute(*attr);
  EXPECT_FALSE(CssTagScanner::ParseCssElement(link_, &href_, &media_));
}

TEST_F(CssTagScannerTest, DoubledRelOK) {
  // Double the rel="stylesheet" and everything is OK.
  HtmlElement::Attribute* attr = link_->FindAttribute(HtmlName::kRel);
  link_->AddAttribute(*attr);
  EXPECT_TRUE(CssTagScanner::ParseCssElement(link_, &href_, &media_,
                                             &nonstandard_attributes_));
  EXPECT_STREQ("", media_);
  EXPECT_STREQ(kUrl, href_->DecodedValueOrNull());
  EXPECT_EQ(0, nonstandard_attributes_.size());
}

TEST_F(CssTagScannerTest, MissingHrefDoubledRelFailOK) {
  // Double the rel and remove the href, and we should reject rather than
  // counting to 2.
  link_->DeleteAttribute(HtmlName::kHref);
  HtmlElement::Attribute* attr = link_->FindAttribute(HtmlName::kRel);
  link_->AddAttribute(*attr);
  EXPECT_FALSE(CssTagScanner::ParseCssElement(link_, &href_, &media_));
}

TEST_F(CssTagScannerTest, RelCaseInsensitiveOK) {
  // The rel attribute is case-insensitive.
  link_->DeleteAttribute(HtmlName::kRel);
  html_parse_.AddAttribute(link_, HtmlName::kRel, "StyleSheet");

  EXPECT_TRUE(CssTagScanner::ParseCssElement(link_, &href_, &media_,
                                             &nonstandard_attributes_));
  EXPECT_STREQ("", media_);
  EXPECT_STREQ(kUrl, href_->DecodedValueOrNull());
  EXPECT_EQ(0, nonstandard_attributes_.size());
}

TEST_F(CssTagScannerTest, TestHasImport) {
  // Should work.
  EXPECT_TRUE(CssTagScanner::HasImport("@import", &message_handler_));
  EXPECT_TRUE(CssTagScanner::HasImport("@Import", &message_handler_));
  EXPECT_TRUE(CssTagScanner::HasImport(
      "@charset 'iso-8859-1';\n"
      "@import url('http://foo.com');\n", &message_handler_));
  EXPECT_TRUE(CssTagScanner::HasImport(
      "@charset 'iso-8859-1';\n"
      "@iMPorT url('http://foo.com');\n", &message_handler_));

  // Should fail.
  EXPECT_FALSE(CssTagScanner::HasImport("", &message_handler_));
  EXPECT_FALSE(CssTagScanner::HasImport("@impor", &message_handler_));
  EXPECT_FALSE(CssTagScanner::HasImport(
      "@charset 'iso-8859-1';\n"
      "@impor", &message_handler_));
  // Make sure we aren't overflowing the buffer.
  GoogleString import_string = "@import";
  StringPiece truncated_import(import_string.data(), import_string.size() - 1);
  EXPECT_FALSE(CssTagScanner::HasImport(truncated_import, &message_handler_));

  // False positives.
  EXPECT_TRUE(CssTagScanner::HasImport(
      "@charset 'iso-8859-1';\n"
      "@importinvalid url('http://foo.com');\n", &message_handler_));
  EXPECT_TRUE(CssTagScanner::HasImport(
      "@charset 'iso-8859-1';\n"
      "/* @import url('http://foo.com'); */\n", &message_handler_));
  EXPECT_TRUE(CssTagScanner::HasImport(
      "@charset 'iso-8859-1';\n"
      "a { color: pink; }\n"
      "/* @import after rulesets is invalid */\n"
      "@import url('http://foo.com');\n", &message_handler_));
}

TEST_F(CssTagScannerTest, IsStylesheetOrAlternate) {
  EXPECT_TRUE(CssTagScanner::IsStylesheetOrAlternate("stylesheet"));
  EXPECT_TRUE(CssTagScanner::IsStylesheetOrAlternate("canonical stylesheet"));
  EXPECT_TRUE(CssTagScanner::IsStylesheetOrAlternate(" stylesheet"));
  EXPECT_TRUE(CssTagScanner::IsStylesheetOrAlternate(" styleSheet"));
  EXPECT_TRUE(CssTagScanner::IsStylesheetOrAlternate("alternate stylesheet"));
  EXPECT_TRUE(CssTagScanner::IsStylesheetOrAlternate("stylesheet alternate"));
  EXPECT_TRUE(
    CssTagScanner::IsStylesheetOrAlternate("stylesheet alternate canonical"));
  EXPECT_TRUE(
    CssTagScanner::IsStylesheetOrAlternate("StyleshEet alternAte canoNical "));
  EXPECT_FALSE(CssTagScanner::IsStylesheetOrAlternate("alternate"));
  EXPECT_FALSE(CssTagScanner::IsStylesheetOrAlternate("prev"));
  EXPECT_FALSE(CssTagScanner::IsStylesheetOrAlternate(""));
}

TEST_F(CssTagScannerTest, IsAlternateStylesheet) {
  EXPECT_FALSE(CssTagScanner::IsAlternateStylesheet("stylesheet"));
  EXPECT_FALSE(CssTagScanner::IsAlternateStylesheet("canonical stylesheet"));
  EXPECT_FALSE(CssTagScanner::IsAlternateStylesheet(" stylesheet"));
  EXPECT_FALSE(CssTagScanner::IsAlternateStylesheet(" styleSheet"));
  EXPECT_TRUE(CssTagScanner::IsAlternateStylesheet("alternate stylesheet"));
  EXPECT_TRUE(CssTagScanner::IsAlternateStylesheet("stylesheet alternate"));
  EXPECT_TRUE(
    CssTagScanner::IsAlternateStylesheet("stylesheet alternate canonical"));
  EXPECT_TRUE(
    CssTagScanner::IsAlternateStylesheet("StyleshEet alternAte canoNical "));
  EXPECT_FALSE(CssTagScanner::IsAlternateStylesheet("alternate"));
  EXPECT_FALSE(CssTagScanner::IsAlternateStylesheet("prev"));
  EXPECT_FALSE(CssTagScanner::IsAlternateStylesheet(""));
}

class RewriteDomainTransformerTest : public RewriteTestBase {
 public:
  RewriteDomainTransformerTest()
      : old_base_url_("http://old-base.com/"),
        new_base_url_("http://new-base.com/") {
  }

  GoogleString Transform(const StringPiece& input) {
    GoogleString output_buffer;
    StringWriter output_writer(&output_buffer);
    RewriteDomainTransformer transformer(&old_base_url_, &new_base_url_,
                                         server_context(), options(),
                                         message_handler());
    EXPECT_TRUE(CssTagScanner::TransformUrls(
        input, &output_writer, &transformer, message_handler()));
    return output_buffer;
  }

  // Test for rewriting CSS delivered in chunks --- the chunks are provided
  // in the NULL-terminated array pieces, and the return value includes what was
  // produced out and what was retained for reparse for each chunk.
  GoogleString TransformStreaming(const char* pieces[]) {
    RewriteDomainTransformer transformer(&old_base_url_, &new_base_url_,
                                         server_context(), options(),
                                         message_handler());
    CssTagScanner scanner(&transformer, message_handler());
    GoogleString result;

    for (int c = 0; pieces[c]; ++c) {
      const char* piece = pieces[c];
      bool last_piece = (pieces[c + 1] == NULL);

      GoogleString output_piece;
      StringWriter output_writer(&output_piece);
      EXPECT_TRUE(scanner.TransformUrlsStreaming(
          piece,
          last_piece ? CssTagScanner::kInputIncludesEnd :
                       CssTagScanner::kInputDoesNotIncludeEnd,
          &output_writer));
      StrAppend(&result, "portion=", output_piece,
                ", retain=", scanner.RetainedForReparse(), "|");
    }
    return result;
  }

 protected:
  GoogleUrl old_base_url_;
  GoogleUrl new_base_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RewriteDomainTransformerTest);
};

TEST_F(RewriteDomainTransformerTest, Empty) {
  EXPECT_STREQ("", Transform(""));
}

TEST_F(RewriteDomainTransformerTest, NoMatch) {
  EXPECT_STREQ("hello", Transform("hello"));
}

TEST_F(RewriteDomainTransformerTest, Absolute) {
  const char css_with_abs_path[] = "a url(http://other_base/image.png) b";
  EXPECT_STREQ(css_with_abs_path, Transform(css_with_abs_path));
}

TEST_F(RewriteDomainTransformerTest, AbsoluteSQuote) {
  const char css_with_abs_path[] = "a url('http://other_base/image.png') b";
  EXPECT_STREQ(css_with_abs_path, Transform(css_with_abs_path));
}

TEST_F(RewriteDomainTransformerTest, AbsoluteDQuote) {
  const char css_with_abs_path[] = "a url(\"http://other_base/image.png\") b";
  EXPECT_STREQ(css_with_abs_path, Transform(css_with_abs_path));
}

TEST_F(RewriteDomainTransformerTest, Relative) {
  EXPECT_STREQ("a url(http://old-base.com/subdir/image.png) b",
               Transform("a url(subdir/image.png) b"));
}

TEST_F(RewriteDomainTransformerTest, RelativeSQuote) {
  EXPECT_STREQ("a url('http://old-base.com/subdir/image.png') b",
               Transform("a url('subdir/image.png') b"));
}

TEST_F(RewriteDomainTransformerTest, EscapeSQuote) {
  EXPECT_STREQ("a url('http://old-base.com/subdir/imag\\'e.png') b",
               Transform("a url('subdir/imag\\'e.png') b"));
}

// Testcase for Issue 60.
TEST_F(RewriteDomainTransformerTest, RelativeSQuoteSpaced) {
  EXPECT_STREQ("a url('http://old-base.com/subdir/image.png') b",
               Transform("a url( 'subdir/image.png' ) b"));
}

TEST_F(RewriteDomainTransformerTest, RelativeDQuote) {
  EXPECT_STREQ("a url(\"http://old-base.com/subdir/image.png\") b",
               Transform("a url(\"subdir/image.png\") b"));
}

TEST_F(RewriteDomainTransformerTest, EscapeDQuote) {
  EXPECT_STREQ("a url(\"http://old-base.com/subdir/%22image.png\") b",
               Transform("a url(\"subdir/\\\"image.png\") b"));
}

TEST_F(RewriteDomainTransformerTest, 2Relative1Abs) {
  const char input[] = "a url(s/1.png) b url(2.png) c url(http://a/3.png) d";
  const char expected[] = "a url(http://old-base.com/s/1.png) b "
      "url(http://old-base.com/2.png) c url(http://a/3.png) d";
  EXPECT_STREQ(expected, Transform(input));
}

TEST_F(RewriteDomainTransformerTest, StringLineCont) {
  // Make sure we understand escaping of new lines inside string --
  // url('foo\                           (ignore this, avoids -Werror=comment)
  // bar') stuff
  //  is interpretted the same as
  // url('foobar') stuff
  const char kExpected[] = "url('http://old-base.com/foobar') stuff";
  EXPECT_STREQ(kExpected, Transform("url('foo\\\nbar') stuff"));

  // There are actually 4 possible new lines for this, per the spec:
  // nl     \n|\r\n|\r|\f
  // Test the other 3
  EXPECT_STREQ(kExpected, Transform("url('foo\\\rbar') stuff"));
  EXPECT_STREQ(kExpected, Transform("url('foo\\\r\nbar') stuff"));
  EXPECT_STREQ(kExpected, Transform("url('foo\\\fbar') stuff"));
}

TEST_F(RewriteDomainTransformerTest, ContAndUnterminated) {
  // We had a logic error in how we handled an unterminated string
  // which also had line continuations after the break point.
  // Note that what we parse here is foo as URL, which gets mapped,
  // and the rest is preserved, including the newline, to get the original
  // error recovery behavior.
  EXPECT_STREQ("@import \"http://old-base.com/foo\nbar\\\nbaz",
               Transform("@import \"foo\nbar\\\nbaz"));
}

TEST_F(RewriteDomainTransformerTest, StringUnterminated) {
  // Properly extend URLs that occur in unclosed string literals;
  // but don't alter the quote mismatch. Notice that the
  // quote didn't get escaped.
  EXPECT_STREQ("@import 'http://old-base.com/foo\n\"bar stuff",
               Transform("@import 'foo\n\"bar stuff"));

  // Try with a different newline separator, too.
  EXPECT_STREQ("@import 'http://old-base.com/foo\f\"bar stuff",
               Transform("@import 'foo\f\"bar stuff"));
}

TEST_F(RewriteDomainTransformerTest, StringMultineTerminated) {
  // Multiline string. This testcase used to show that having a close
  // quote matters, but it doesn't --- unescaped newline closes the string.
  EXPECT_STREQ("@import 'http://old-base.com/foo\nbar' stuff",
               Transform("@import 'foo\nbar' stuff"));
}

TEST_F(RewriteDomainTransformerTest, UrlProperClose) {
  // Note: the \) in the output is due to some unneeded escaping done;
  // it'd be fine if it were missing.
  EXPECT_STREQ("url('http://old-base.com/foo\\).bar')",
               Transform("url('foo).bar')"));
}

TEST_F(RewriteDomainTransformerTest, UrlUnquoted) {
  // Unquoted URLs can't have space in them, either.
  // The important thing here is that transformed version doesn't get %20.
  EXPECT_STREQ("url(http://old-base.com/foo bar)",
               Transform("url(/foo bar)"));
}

TEST_F(RewriteDomainTransformerTest, LotsOfWhitespace) {
  // Make sure we do sane thing with trailing whitespace in unquoted url()
  EXPECT_STREQ("url(http://old-base.com/foo)",
               Transform("url(/foo \t  \f     )"));

  // Leading, too.
  EXPECT_STREQ("url(http://old-base.com/foo)",
               Transform("url(  \r\n  /foo \t  \f     )"));
}

TEST_F(RewriteDomainTransformerTest, DontUnescapeTooMuch) {
  // Demonstrate that our escaping doesn't cause us to produce improperly
  // closed URLs in output.
  EXPECT_STREQ("url(http://old-base.com/\\)stuff)",
               Transform("url(/\\)stuff)"));

  EXPECT_STREQ("url(\"http://old-base.com/%22stuff\")",
               Transform("url(\"/\\\"stuff\")"));
}

TEST_F(RewriteDomainTransformerTest, ImportUrl) {
  EXPECT_STREQ(
      "a @import url(http://old-base.com/style.css) div { display: block; }",
      Transform("a @import url(style.css) div { display: block; }"));
}

TEST_F(RewriteDomainTransformerTest, ImportUrlQuote) {
  EXPECT_STREQ(
      "a @import url('http://old-base.com/style.css') div { display: block; }",
      Transform("a @import url('style.css') div { display: block; }"));
}

TEST_F(RewriteDomainTransformerTest, ImportUrlQuoteNoCloseParen) {
  // Despite what CSS2.1 specifies, in practice browsers don't seem to
  // recover consistently from an unclosed url(; so we don't either.
  const char kInput[] = "a @import url('style.css' div { display: block; }";
  EXPECT_STREQ(kInput, Transform(kInput));
}

TEST_F(RewriteDomainTransformerTest, ImportSQuote) {
  EXPECT_STREQ(
      "a @import 'http://old-base.com/style.css' div { display: block; }",
      Transform("a @import 'style.css' div { display: block; }"));
}

TEST_F(RewriteDomainTransformerTest, ImportDQuote) {
  EXPECT_STREQ(
      "a @import \"http://old-base.com/style.css\" div { display: block; }",
      Transform("a @import \t \"style.css\" div { display: block; }"));
}

TEST_F(RewriteDomainTransformerTest, ImportSQuoteDQuote) {
  EXPECT_STREQ(
      "a @import 'http://old-base.com/style.css'\"screen\";",
      Transform("a @import 'style.css'\"screen\";"));
}

TEST_F(RewriteDomainTransformerTest, BrokenEscape) {
  // First one is unchanged due to our own limitations.
  EXPECT_STREQ(
      "@import 'foo/\\1234'; url(foo\\",
      Transform("@import 'foo/\\1234'; url(foo\\"));
}

TEST_F(RewriteDomainTransformerTest, StreamingUrlInterrupt) {
  const char* input[] = { "u",
                          "rl(",
                          "\"foo",
                          ".png\"",
                          ") bar u",
                          "x",
                          NULL };
  EXPECT_EQ("portion=, retain=u|"
            "portion=, retain=url(|"
            "portion=, retain=url(\"foo|"
            "portion=, retain=url(\"foo.png\"|"
            "portion=url(\"http://old-base.com/foo.png\") bar , retain=u|"
            "portion=ux, retain=|",
            TransformStreaming(input));
}

TEST_F(RewriteDomainTransformerTest, StreamingOtherAtRule) {
  // export has same length as import, so we can tell it's not import
  // when seeing it.
  const char* input[] = { "@export",
                          " \"foo.png\";",
                          NULL };
  EXPECT_EQ("portion=@export, retain=|"
            "portion= \"foo.png\";, retain=|",
            TransformStreaming(input));
}

TEST_F(RewriteDomainTransformerTest, StreamingUrlArgInterrupt) {
  const char* input[] = { "background-image:url(",
                          "foo.png",
                          ")",
                          NULL };
  EXPECT_EQ("portion=background-image:, retain=url(|"
            "portion=, retain=url(foo.png|"
            "portion=url(http://old-base.com/foo.png), retain=|",
            TransformStreaming(input));
}

TEST_F(RewriteDomainTransformerTest, StreamingImportInterrupt) {
  const char* input[] = { "@",
                          "imp",
                          "ort",
                          " ",
                          " \"foo.css",
                          "\";",
                          NULL };
  EXPECT_EQ("portion=, retain=@|"
            "portion=, retain=@imp|"
            "portion=, retain=@import|"
            "portion=, retain=@import |"
            "portion=, retain=@import  \"foo.css|"
            "portion=@import \"http://old-base.com/foo.css\";, retain=|",
            TransformStreaming(input));
}

TEST_F(RewriteDomainTransformerTest, StreamingEscape) {
  const char* input[] = { "background-image: url(\"foo\\",
                          "\"bar\")",
                          NULL };
  EXPECT_EQ("portion=background-image: , retain=url(\"foo\\|"
            "portion=url(\"http://old-base.com/foo%22bar\"), retain=|",
            TransformStreaming(input));
}

TEST_F(RewriteDomainTransformerTest, StreamingCharByChar) {
  // Just run through input char-by-char.
  const char input[] = "@import \"other.css\"; "
                      "ul { list-style-image:url(a.png); }";
  RewriteDomainTransformer transformer(&old_base_url_, &new_base_url_,
                                       server_context(), options(),
                                       message_handler());
  CssTagScanner scanner(&transformer, message_handler());
  GoogleString result;

  for (int i = 0; i < STATIC_STRLEN(input); ++i) {
    char character = input[i];
    bool last_piece = (i == (STATIC_STRLEN(input) - 1));

    GoogleString output_piece;
    StringWriter output_writer(&output_piece);
    EXPECT_TRUE(scanner.TransformUrlsStreaming(
        GoogleString(1, character),
        last_piece ? CssTagScanner::kInputIncludesEnd :
                     CssTagScanner::kInputDoesNotIncludeEnd,
        &output_writer));
    StrAppend(&result, output_piece, "|");
  }

  EXPECT_EQ("||||||||||||||||||@import \"http://old-base.com/other.css\"|;"
            "| ||||ul {| |l|i|s|t|-|s|t|y|l|e|-|i|m|a|g|e|:"
            "||||||||||url(http://old-base.com/a.png)|;| |}|",
            result);
}

class FailTransformer : public CssTagScanner::Transformer {
 public:
  FailTransformer() {}
  virtual ~FailTransformer() {}

  virtual TransformStatus Transform(GoogleString* str) {
    return kFailure;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FailTransformer);
};

TEST(FailTransformerTest, TransformUrlsFails) {
  NullWriter writer;
  NullMessageHandler handler;

  FailTransformer fail_transformer;
  EXPECT_FALSE(CssTagScanner::TransformUrls("url(foo)", &writer,
                                            &fail_transformer, &handler));
}

}  // namespace

}  // namespace net_instaweb
