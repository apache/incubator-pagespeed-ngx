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

// Author: mdsteele@google.com (Matthew D. Steele)

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

class CssInlineFilterTest : public ResourceManagerTestBase {
 protected:
  void TestInlineCss(const GoogleString& html_url,
                     const GoogleString& css_url,
                     const GoogleString& other_attrs,
                     const GoogleString& css_original_body,
                     bool expect_inline,
                     const GoogleString& css_rewritten_body) {
    AddFilter(RewriteOptions::kInlineCss);

    const GoogleString html_input =
        "<head>\n"
        "  <link rel=\"stylesheet\" href=\"" + css_url + "\"" +
        (other_attrs.empty() ? "" : " " + other_attrs) + ">\n"
        "</head>\n"
        "<body>Hello, world!</body>\n";

    // Put original CSS file into our fetcher.
    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(css_url, default_css_header, css_original_body);

    // Rewrite the HTML page.
    ParseUrl(html_url, html_input);

    const GoogleString expected_output =
        (!expect_inline ? html_input :
         "<head>\n"
         "  <style>" + css_rewritten_body + "</style>\n"
         "</head>\n"
         "<body>Hello, world!</body>\n");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  }
};

TEST_F(CssInlineFilterTest, InlineCssSimple) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
}

TEST_F(CssInlineFilterTest, InlineCssAbsolutifyUrls1) {
  // CSS with a relative URL that needs to be changed:
  const GoogleString css1 =
      "BODY { background-image: url('bg.png'); }\n";
  const GoogleString css2 =
      "BODY { background-image: "
      "url('http://www.example.com/foo/bar/bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/foo/bar/baz.css",
                "", css1, true, css2);
}

TEST_F(CssInlineFilterTest, InlineCssAbsolutifyUrls2) {
  // CSS with a relative URL, this time with ".." in it:
  const GoogleString css1 =
      "BODY { background-image: url('../quux/bg.png'); }\n";
  const GoogleString css2 =
      "BODY { background-image: "
      "url('http://www.example.com/foo/quux/bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/foo/bar/baz.css",
                "", css1, true, css2);
}

TEST_F(CssInlineFilterTest, NoAbsolutifyUrlsSameDir) {
  const GoogleString css = "BODY { background-image: url('bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/baz.css",
                "", css, true, css);
}

TEST_F(CssInlineFilterTest, DoNotInlineCssWithMediaAttr) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "media=\"print\"", css, false, "");
}

TEST_F(CssInlineFilterTest, DoInlineCssWithMediaAll) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "media=\"all\"", css, true, css);
}

TEST_F(CssInlineFilterTest, DoNotInlineCssTooBig) {
  // CSS too large to inline:
  const int64 length = 2 * RewriteOptions::kDefaultCssInlineMaxBytes;
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css", "",
                ("BODY { background-image: url('" +
                 GoogleString(length, 'z') + ".png'); }\n"),
                false, "");
}

TEST_F(CssInlineFilterTest, DoNotInlineCssDifferentDomain) {
  // TODO(mdsteele): Is switching domains in fact an issue for CSS?
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.org/styles.css",
                "", "BODY { color: red; }\n", false, "");
}

TEST_F(CssInlineFilterTest, DoNotInlineCssWithImports) {
  // TODO(mdsteele): Is switching domains in fact an issue for CSS?
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", "@import \"foo.css\", BODY { color: red; }\n", false, "");
}

// http://code.google.com/p/modpagespeed/issues/detail?q=css&id=252
TEST_F(CssInlineFilterTest, ClaimsXhtmlButHasUnclosedLink) {
  // XHTML text should not have unclosed links.  But if they do, like
  // in Issue 252, then we should leave them alone.
  static const char html_format[] =
      "<head>\n"
      "  %s\n"
      "  %s\n"
      "  <script type='text/javascript' src='c.js'></script>"     // 'in' <link>
      "</head>\n"
      "<body><div class=\"c1\"><div class=\"c2\"><p>\n"
      "  Yellow on Blue</p></div></div></body>";

  static const char unclosed_css[] =
      "  <link rel='stylesheet' href='a.css' type='text/css'>\n";  // unclosed
  static const char inlined_css[] = "  <style>.a {}</style>\n";

  // Put original CSS files into our fetcher.
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(StrCat(kTestDomain, "a.css"), default_css_header, ".a {}");
  AddFilter(RewriteOptions::kInlineCss);
  ValidateExpected("claims_xhtml_but_has_unclosed_links",
                   StringPrintf(html_format, kXhtmlDtd, unclosed_css),
                   StringPrintf(html_format, kXhtmlDtd, inlined_css));
}

}  // namespace

}  // namespace net_instaweb
