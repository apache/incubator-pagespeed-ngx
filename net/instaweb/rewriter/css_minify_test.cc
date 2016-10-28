/*
 * Copyright 2011 Google Inc.
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_minify.h"

#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "webutil/css/parser.h"


namespace net_instaweb {

namespace {

class CssMinifyTest : public ::testing::Test {
 protected:
  CssMinifyTest() {}
  virtual ~CssMinifyTest() {}

  void RewriteCss(const StringPiece& in_text, GoogleString* out_text) {
    out_text->clear();

    // Parse CSS.
    Css::Parser parser(in_text);
    parser.set_preservation_mode(true);
    parser.set_quirks_mode(false);
    scoped_ptr<Css::Stylesheet> stylesheet(parser.ParseRawStylesheet());

    if (parser.errors_seen_mask() != Css::Parser::kNoError) {
      in_text.CopyToString(out_text);
      return;
    }

    // Re-serialize
    StringWriter out_writer(out_text);
    EXPECT_TRUE(CssMinify::Stylesheet(*stylesheet, &out_writer, &handler_));
  }

  GoogleMessageHandler handler_;
};


TEST_F(CssMinifyTest, RewriteCssIncompleteUnicode) {
  // Test that a css string with an incomplete unicode character doesn't hang.
  // This string should not get minified either due to the error in it.
  static const unsigned char kCssStringData[] = {
      64, 109, 101, 100, 105, 97,  32,  40, 106,
      97, 120, 45,  119, 105, 100, 116, 104, 58,
      32, 88,  200, 194, 143, 135, 41,  32, 0};
  const char* css_string(reinterpret_cast<const char*>(kCssStringData));
  // This string is `echo "QG1lZGlhIChqYXgtd2lkdGg6IFjIwo+HKSA=" |base64 -d`
  GoogleString rewritten;
  RewriteCss(css_string, &rewritten);
  EXPECT_STREQ(rewritten, css_string) << "Should not hang";
}

TEST_F(CssMinifyTest, MinifyStylesheeetCollectingUrls) {
  const char kCss[] =
      ".a {\n"
      "  background-color: darkgreen;\n"
      "  background-image: url(foo.png);\n"
      "}";
  GoogleString minified;
  StringWriter writer(&minified);
  StringVector urls;
  CssMinify minify(&writer, &handler_);
  minify.set_url_collector(&urls);
  EXPECT_TRUE(minify.ParseStylesheet(kCss));
  ASSERT_EQ(1, urls.size());
  EXPECT_STREQ("foo.png", urls[0]);
  EXPECT_STREQ(".a{background-color:#006400;background-image:url(foo.png)}",
               minified);
}

TEST_F(CssMinifyTest, MinifyImportStylesheeetCollectingUrls) {
  const char kCss[] = "@import   'foo.png';";
  GoogleString minified;
  StringWriter writer(&minified);
  StringVector urls;
  CssMinify minify(&writer, &handler_);
  minify.set_url_collector(&urls);
  EXPECT_TRUE(minify.ParseStylesheet(kCss));
  ASSERT_EQ(1, urls.size());
  EXPECT_STREQ("foo.png", urls[0]);
  EXPECT_STREQ("@import url(foo.png);", minified);
}

TEST_F(CssMinifyTest, MinifyInvalid) {
  const char kCss[] = "{";
  GoogleString minified;
  StringWriter writer(&minified);
  CssMinify minify(&writer, &handler_);
  EXPECT_FALSE(minify.ParseStylesheet(kCss));
}

TEST_F(CssMinifyTest, DoNotFixBadColorsOrUnits) {
  const char kCss[] =
      ".a {\n"
      "  width: 10;\n"
      "  height: 20px;;\n"
      "  background-color: 0f0f0f;\n"
      "  foreground-color: #f0f0f0;\n"
      "}";
  GoogleString minified;
  StringWriter writer(&minified);
  CssMinify minify(&writer, &handler_);
  EXPECT_TRUE(minify.ParseStylesheet(kCss));

  // Note that we don't "fix" the '10' by appending a 'px', or the
  // background-color by adding a '#'.  In quirks-mode we would add the #.
  EXPECT_STREQ(
      ".a{width:10;height:20px;"
      "background-color: 0f0f0f;"  // Missing # not added, space is not removed.
      "foreground-color:#f0f0f0}",
      minified);
}

TEST_F(CssMinifyTest, RemoveZeroLengthButNotTimeOrPercentSuffix) {
  const char kCss[] =
      ".a {\n"
      "  width: 0px;\n"
      "  height: 0%;\n"
      "  -moz-transition-delay: 0s, 0s;\n"
      "}";
  GoogleString minified;
  StringWriter writer(&minified);
  CssMinify minify(&writer, &handler_);
  EXPECT_TRUE(minify.ParseStylesheet(kCss));
  // TODO(jmarantz): this CSS is not well minified.  We should strip
  // the spaces around the comma.
  EXPECT_STREQ(".a{width:0;height:0%;-moz-transition-delay:0s , 0s}", minified);
}

TEST_F(CssMinifyTest, ParsingAndMinifyingBackgroundAndFont) {
  const char kCss[] =
      ".a {\n"
      "  font:normal 16px Foo, sans-serif;\n"
      "}\n"
      "body {\n"
      "  background: #fff;\n"
      "}";
  Css::Parser parser(kCss);
  std::unique_ptr<Css::Stylesheet> stylesheet(parser.ParseStylesheet());
  GoogleString minified;
  StringWriter writer(&minified);

  EXPECT_TRUE(CssMinify::Stylesheet(*stylesheet, &writer, &handler_));
  // TODO(peleyal): We are adding more data than required. Should be:
  // ".a{font:normal 16px Foo,sans-serif}"
  // "body{background:#fff}"
  EXPECT_STREQ(
      ".a{font:16px Foo,sans-serif;font-style:normal;font-variant:normal;"
      "font-weight:normal;font-size:16px;line-height:normal;"
      "font-family:Foo,sans-serif}"
      "body{background:#fff;background-color:#fff;background-image:none;"
      "background-repeat:repeat;background-attachment:scroll;"
      "background-position-x:0%;background-position-y:0%}",
      minified);
}

TEST_F(CssMinifyTest, ParsingAndMinifingViewportUnits) {
  const char kCss[] =
      ".a {\n"
      "  margin-top: 70vh;\n"
      "  margin-bottom: 20vw;\n"
      "}\n";

  Css::Parser parser(kCss);
  std::unique_ptr<Css::Stylesheet> stylesheet(parser.ParseStylesheet());
  GoogleString minified;
  StringWriter writer(&minified);

  EXPECT_TRUE(CssMinify::Stylesheet(*stylesheet, &writer, &handler_));
  EXPECT_STREQ(".a{margin-top:70vh;margin-bottom:20vw}", minified);
}

TEST_F(CssMinifyTest, StraySingleQuote1) {
  static const char kCss[] =
      ".view_all a{\n"
      "  display: block;\n"
      "  'width: 100%;\n"
      "  padding: 5px 0 1px 0"
      "}";

  Css::Parser parser(kCss);
  std::unique_ptr<Css::Stylesheet> stylesheet(parser.ParseStylesheet());
  GoogleString minified;
  StringWriter writer(&minified);

  EXPECT_TRUE(CssMinify::Stylesheet(*stylesheet, &writer, &handler_));
  // There are two bits of error recovery happening here:
  // 1) error recovery for the unclosed 'width string eats all the way until
  //    the end of line.
  // 2) error recovery for the declaration starting with 'width eats all the way
  //    until the next semicolon or a closing } (skipping matching ones before)
  // --- which is after the padding declaration, since the first semicolon is
  // just a part of the 'width... string
  EXPECT_STREQ(".view_all a{display:block}", minified);
}

TEST_F(CssMinifyTest, StraySingleQuote2) {
  static const char kCss[] =
      ".view_all a{\n"
      "  display: block;\n"
      "  'width: 100%;\n"
      "  padding: 5px 0 1px 0;"
      "}";

  GoogleString minified;
  StringWriter writer(&minified);
  CssMinify minify(&writer, &handler_);
  StringVector urls;
  minify.set_url_collector(&urls);
  ASSERT_TRUE(minify.ParseStylesheet(kCss));
  EXPECT_STREQ(".view_all a{display:block;'width: 100%;\n"
               "  padding: 5px 0 1px 0}",
               minified);
}

TEST_F(CssMinifyTest, StraySingleQuote3) {
  // Non-permissive mode, should drop anything on the 'width line till \n,
  // and then continue recovery until the next semicolon.
  static const char kCss[] =
      ".view_all a{\n"
      "  display: block;\n"
      "  'width: 100%; border:1px solid red;\n"
      "  padding: 5px 0 1px 0;"
      "  margin: 1px;"
      "}";

  Css::Parser parser(kCss);
  std::unique_ptr<Css::Stylesheet> stylesheet(parser.ParseStylesheet());
  GoogleString minified;
  StringWriter writer(&minified);

  EXPECT_TRUE(CssMinify::Stylesheet(*stylesheet, &writer, &handler_));
  EXPECT_STREQ(".view_all a{display:block;margin:1px}", minified);
}

}  // namespace
}  // namespace net_instaweb
