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

// Author: matterbury@google.com (Matt Atterbury)

#include "net/instaweb/rewriter/public/css_inline_import_to_link_filter.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kCssFile[] = "assets/styles.css";
const char kCssTail[] = "styles.css";
const char kCssSubdir[] = "assets/";
const char kCssData[] = ".blue {color: blue; src: url(dummy.png);}";

class CssInlineImportToLinkFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
  }

  // Test general situations.
  void ValidateStyleToLink(const GoogleString& input_style,
                           const GoogleString& expected_style) {
    const GoogleString html_input =
        "<head>\n" +
        input_style +
        "</head>\n"
        "<body>Hello, world!</body>\n";

    // Rewrite the HTML page.
    ParseUrl("http://test.com/test.html", html_input);

    // Check the output HTML.
    const GoogleString expected_output =
        "<head>\n" +
        expected_style +
        "</head>\n"
        "<body>Hello, world!</body>\n";
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  }

  void ValidateStyleUnchanged(const GoogleString& import_equals_output) {
    ValidateStyleToLink(import_equals_output, import_equals_output);
  }
};

// Tests for converting styles to links.
TEST_F(CssInlineImportToLinkFilterTest, ConvertGoodStyle) {
  AddFilter(RewriteOptions::kInlineImportToLink);

  static const char kLink[] =
      "<link rel=\"stylesheet\" href=\"assets/styles.css\">";

  // These all get converted to the above link.
  ValidateStyleToLink("<style>@import url(assets/styles.css);</style>", kLink);
  ValidateStyleToLink("<style>@import url(\"assets/styles.css\");</style>",
                      kLink);
  ValidateStyleToLink("<style>\n\t@import \"assets/styles.css\"\t;\n\t</style>",
                      kLink);
  ValidateStyleToLink("<style>@import 'assets/styles.css';</style>", kLink);
  ValidateStyleToLink("<style>@import url( assets/styles.css);</style>", kLink);
  ValidateStyleToLink("<style>@import url('assets/styles.css');</style>",
                      kLink);
  ValidateStyleToLink("<style>@import url( 'assets/styles.css' );</style>",
                      kLink);

  // According to the latest DRAFT CSS spec this is invalid due to the missing
  // final semicolon, however according to the 2003 spec it is valid. Some
  // browsers seem to accept it and some don't, so we will accept it.
  ValidateStyleToLink("<style>@import url(assets/styles.css)</style>", kLink);
}

TEST_F(CssInlineImportToLinkFilterTest, ConvertStyleWithAttributes) {
  AddFilter(RewriteOptions::kInlineImportToLink);
  ValidateStyleToLink("<style type=\"text/css\">"
                      "@import url(assets/styles.css);</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\">");
  ValidateStyleToLink("<style type=\"text/css\" media=\"screen\">"
                      "@import url(assets/styles.css);</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\"screen\">");
}

TEST_F(CssInlineImportToLinkFilterTest, ConvertStyleWithSameMedia) {
  AddFilter(RewriteOptions::kInlineImportToLink);
  ValidateStyleToLink("<style>@import url(assets/styles.css) all</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " media=\"all\">");
  ValidateStyleToLink("<style type=\"text/css\">"
                      "@import url(assets/styles.css) all;</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\"all\">");
  ValidateStyleToLink("<style type=\"text/css\" media=\"screen\">"
                      "@import url(assets/styles.css) screen;</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\"screen\">");
  ValidateStyleToLink("<style type=\"text/css\" media=\"screen,printer\">"
                      "@import url(assets/styles.css) printer,screen;</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\"screen,printer\">");
  ValidateStyleToLink("<style type=\"text/css\" media=\" screen , printer \">"
                      "@import 'assets/styles.css' printer, screen ;</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\" screen , printer \">");
}

TEST_F(CssInlineImportToLinkFilterTest, ConvertStyleWithDifferentMedia) {
  AddFilter(RewriteOptions::kInlineImportToLink);
  ValidateStyleUnchanged("<style type=\"text/css\" media=\"screen\">"
                         "@import url(assets/styles.css) all;</style>");
  ValidateStyleUnchanged("<style type=\"text/css\" media=\"screen,printer\">"
                         "@import url(assets/styles.css) screen;</style>");
}

TEST_F(CssInlineImportToLinkFilterTest, DoNotConvertBadStyle) {
  AddFilter(RewriteOptions::kInlineImportToLink);
  // These all are problematic in some way so are not changed at all.
  ValidateStyleUnchanged("<style/>");
  ValidateStyleUnchanged("<style></style>");
  ValidateStyleUnchanged("<style>@import assets/styles.css;</style>");
  ValidateStyleUnchanged("<style>@import url (assets/styles.css);</style>");
  ValidateStyleUnchanged("<style>@ import url(assets/styles.css)</style>");
  ValidateStyleUnchanged("<style>*border: 0px</style>");
  ValidateStyleUnchanged("<style>@import \"mystyle.css\" all;\n"
                         "@import url(\"mystyle.css\" );\n</style>");
  ValidateStyleUnchanged("<style>@charset \"ISO-8859-1\";\n"
                         "@import \"mystyle.css\" all;</style>");
  ValidateStyleUnchanged("<style><p/>@import url(assets/styles.css)</style>");
  ValidateStyleUnchanged("<style>@import url(assets/styles.css);<p/</style>");
  ValidateStyleUnchanged("<style><![CDATA[@import url(assets/styles.css);]]\n");
  ValidateStyleUnchanged("<style>@import url(assets/styles.css);\n"
                         "<![CDATA[\njunky junk junk!\n]]\\></style>");
  ValidateStyleUnchanged("<style><![CDATA[\njunky junk junk!\n]]\\>\n"
                         "@import url(assets/styles.css);</style>");
  ValidateStyleUnchanged("<style>@import url(assets/styles.css);"
                         "<!-- comment --></style>");
  ValidateStyleUnchanged("<style><!-- comment -->"
                         "@import url(assets/styles.css);</style>");
  ValidateStyleUnchanged("<style href='x'>@import url(styles.css);</style>");
  ValidateStyleUnchanged("<style rel='x'>@import url(styles.css);</style>");
  ValidateStyleUnchanged("<style type=\"text/javascript\">"
                         "@import url(assets/styles.css);</style>");

  // Note: this test fails because Css::Parser parses <style/> as a media type
  // (and converts it to 'style') and since the real style element has no media
  // type we end up with a link with media type of 'style'. I don't know if
  // this is correct behavior so I am leaving it out but commenting it.
  // ValidateStyleUnchanged("<style>@import url(styles.css)<style/></style>");
}

class CssInlineImportToLinkFilterTestNoTags
    : public CssInlineImportToLinkFilterTest {
 public:
  virtual bool AddHtmlTags() const { return false; }
};

TEST_F(CssInlineImportToLinkFilterTestNoTags, UnclosedStyleGetsConverted) {
  options()->EnableFilter(RewriteOptions::kInlineImportToLink);
  rewrite_driver()->AddFilters();
  ValidateExpected("unclosed_style",
                   "<style>@import url(assets/styles.css)",
                   "<link rel=\"stylesheet\" href=\"assets/styles.css\">");
}

TEST_F(CssInlineImportToLinkFilterTest, ConvertThenCacheExtend) {
  options()->EnableFilter(RewriteOptions::kInlineImportToLink);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();
  InitResponseHeaders(kCssFile, kContentTypeCss, kCssData, 100);  // 100ms

  ValidateExpected("script_to_link_then_cache_extend",
                   StrCat("<style>@import url(", kCssFile, ");</style>"),
                   StrCat("<link rel=\"stylesheet\" href=\"",
                          Encode(StrCat(kTestDomain, kCssSubdir), "ce", "0",
                                 kCssTail, "css"),
                          "\">"));
}

TEST_F(CssInlineImportToLinkFilterTest, DontConvertButCacheExtend) {
  options()->EnableFilter(RewriteOptions::kInlineImportToLink);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();
  InitResponseHeaders(kCssFile, kContentTypeCss, kCssData, 100);  // 100ms

  const GoogleString kStyleElement = StrCat("<style>@import url(",
                                            kCssFile, ");\n",
                                            "body { color: red; }\n",
                                            "</style>");

  ValidateNoChanges("dont_touch_script_but_cache_extend", kStyleElement);
}

}  // namespace

}  // namespace net_instaweb
