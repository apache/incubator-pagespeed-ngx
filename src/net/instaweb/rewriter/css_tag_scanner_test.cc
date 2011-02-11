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

#include <string>
#include "base/basictypes.h"
#include "net/instaweb/htmlparse/public/html_parse.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"

#include "net/instaweb/util/public/google_url.h"

namespace net_instaweb {

class CssTagScannerTest : public testing::Test {
 protected:
  CssTagScannerTest() : writer_(&output_buffer_) { }

  void Check(const StringPiece& input, const StringPiece& expected) {
    ASSERT_TRUE(CssTagScanner::AbsolutifyUrls(
        input,  "http://base/dir/styles.css", &writer_, &message_handler_));
    EXPECT_EQ(expected, output_buffer_);
  }

  void CheckNoChange(const StringPiece& value) {
    Check(value, value);
  }

  void CheckGurlResolve(const GURL& base, const char* relative_path,
                        const char* abs_path) {
    GURL resolved = base.Resolve(relative_path);
    EXPECT_TRUE(resolved.is_valid());
    EXPECT_EQ(std::string(abs_path), resolved.spec());
  }

  std::string output_buffer_;
  StringWriter writer_;
  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CssTagScannerTest);
};

TEST_F(CssTagScannerTest, TestAbsolutifyUrlsEmpty) {
  CheckNoChange("");
}

TEST_F(CssTagScannerTest, TestAbsolutifyUrlsNoMatch) {
  CheckNoChange("hello");
}

TEST_F(CssTagScannerTest, TestAbsolutifyUrlsAbsolute) {
  const char css_with_abs_path[] = "a url(http://other_base/image.png) b";
  CheckNoChange(css_with_abs_path);
}

TEST_F(CssTagScannerTest, TestAbsolutifyUrlsAbsoluteSQuote) {
  const char css_with_abs_path[] = "a url('http://other_base/image.png') b";
  CheckNoChange(css_with_abs_path);
}

TEST_F(CssTagScannerTest, TestAbsolutifyUrlsAbsoluteDQuote) {
  CheckNoChange("a url(\"http://other_base/image.png\") b");
}

TEST_F(CssTagScannerTest, TestAbsolutifyUrlsRelative) {
  Check("a url(subdir/image.png) b",
        "a url(http://base/dir/subdir/image.png) b");
}

TEST_F(CssTagScannerTest, TestAbsolutifyUrlsRelativeSQuote) {
  Check("a url('subdir/image.png') b",
        "a url('http://base/dir/subdir/image.png') b");
}

// Testcase for Issue 60.
TEST_F(CssTagScannerTest, TestAbsolutifyUrlsRelativeSQuoteSpaced) {
  Check("a url( 'subdir/image.png' ) b",
        "a url('http://base/dir/subdir/image.png') b");
}

TEST_F(CssTagScannerTest, TestAbsolutifyUrlsRelativeDQuote) {
  Check("a url(\"subdir/image.png\") b",
        "a url(\"http://base/dir/subdir/image.png\") b");
}

TEST_F(CssTagScannerTest, TestAbsolutifyUrls2Relative1Abs) {
  Check("a url(s/1.png) b url(2.png) c url(http://a/3.png) d",
        "a url(http://base/dir/s/1.png) b url(http://base/dir/2.png) c "
        "url(http://a/3.png) d");
}

// This test verifies that we understand how GURL::Resolve works.
TEST_F(CssTagScannerTest, TestGurl) {
  GURL base_slash("http://base/");
  EXPECT_TRUE(base_slash.is_valid());
  CheckGurlResolve(base_slash, "r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_slash, "/r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_slash, "../r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_slash, "./r/path.ext", "http://base/r/path.ext");

  GURL base_no_slash("http://base");
  EXPECT_TRUE(base_no_slash.is_valid());
  CheckGurlResolve(base_no_slash, "r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_no_slash, "/r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_no_slash, "../r/path.ext", "http://base/r/path.ext");
  CheckGurlResolve(base_no_slash, "./r/path.ext", "http://base/r/path.ext");
}

// This test makes sure we can identify a few different forms of CSS tags we've
// seen.
TEST_F(CssTagScannerTest, TestFull) {
  HtmlParse html_parse(&message_handler_);
  HtmlElement* link = html_parse.NewElement(NULL, HtmlName::kLink);
  const char kUrl[] = "http://www.myhost.com/static/mycss.css";
  const char kPrint[] = "print";
  html_parse.AddAttribute(link, HtmlName::kRel, "stylesheet");
  html_parse.AddAttribute(link, HtmlName::kHref, kUrl);
  HtmlElement::Attribute* href = NULL;
  const char* media = NULL;
  CssTagScanner scanner(&html_parse);

  // We can parse css even lacking a 'type' attribute.  Default to text/css.
  EXPECT_TRUE(scanner.ParseCssElement(link, &href, &media));
  EXPECT_EQ("", std::string(media));
  EXPECT_EQ(kUrl, std::string(href->value()));

  // Add an unexpected attribute.  Now we don't know what to do with it.
  link->AddAttribute(html_parse.MakeName("other"), "value", "\"");
  EXPECT_FALSE(scanner.ParseCssElement(link, &href, &media));

  // Mutate it to the correct attribute.
  HtmlElement::Attribute* attr = link->FindAttribute(HtmlName::kOther);
  ASSERT_TRUE(attr != NULL);
  html_parse.SetAttributeName(attr, HtmlName::kType);
  attr->SetValue("text/css");
  EXPECT_TRUE(scanner.ParseCssElement(link, &href, &media));
  EXPECT_EQ("", std::string(media));
  EXPECT_EQ(kUrl, std::string(href->value()));

  // Add a media attribute.  It should still pass, yielding media.
  html_parse.AddAttribute(link, HtmlName::kMedia, kPrint);
  EXPECT_TRUE(scanner.ParseCssElement(link, &href, &media));
  EXPECT_EQ(kPrint, std::string(media));
  EXPECT_EQ(kUrl, std::string(href->value()));

  // TODO(jmarantz): test removal of 'rel' and 'href' attributes
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
  std::string import_string = "@import";
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

}  // namespace net_instaweb
