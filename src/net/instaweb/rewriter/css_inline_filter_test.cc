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
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

class CssInlineFilterTest : public ResourceManagerTestBase,
                            public ::testing::WithParamInterface<bool> {
 public:
  CssInlineFilterTest() : filters_added_(false) {}

 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    SetAsynchronousRewrites(GetParam());
  }

  void TestInlineCssWithOutputUrl(
                     const GoogleString& html_url,
                     const GoogleString& css_url,
                     const GoogleString& css_out_url,
                     const GoogleString& other_attrs,
                     const GoogleString& css_original_body,
                     bool expect_inline,
                     const GoogleString& css_rewritten_body) {
    if (!filters_added_) {
      AddFilter(RewriteOptions::kInlineCss);
      filters_added_ = true;
    }

    GoogleString html_template = StrCat(
        "<head>\n",
        "  <link rel=\"stylesheet\" href=\"%s\"",
        (other_attrs.empty() ? "" : " " + other_attrs) + ">\n",
        "</head>\n",
        "<body>Hello, world!</body>\n");

    const GoogleString html_input =
        StringPrintf(html_template.c_str(), css_url.c_str());

    const GoogleString outline_html_output =
        StringPrintf(html_template.c_str(), css_out_url.c_str());

    // Put original CSS file into our fetcher.
    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(css_url, default_css_header, css_original_body);

    // Rewrite the HTML page.
    ParseUrl(html_url, html_input);

    const GoogleString expected_output =
        (!expect_inline ? outline_html_output :
         "<head>\n"
         "  <style>" + css_rewritten_body + "</style>\n"
         "</head>\n"
         "<body>Hello, world!</body>\n");
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  }

  void TestInlineCss(const GoogleString& html_url,
                     const GoogleString& css_url,
                     const GoogleString& other_attrs,
                     const GoogleString& css_original_body,
                     bool expect_inline,
                     const GoogleString& css_rewritten_body) {
    TestInlineCssWithOutputUrl(
        html_url, css_url, css_url, other_attrs, css_original_body,
        expect_inline, css_rewritten_body);
  }

 private:
  bool filters_added_;
};

TEST_P(CssInlineFilterTest, InlineCssSimple) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
}

TEST_P(CssInlineFilterTest, InlineCss404) {
  // Test to make sure that a missing input is handled well.
  SetFetchResponse404("404.css");
  ValidateNoChanges("404", "<link rel=stylesheet href='404.css'>");

  // Second time, to make sure caching doesn't break it.
  ValidateNoChanges("404", "<link rel=stylesheet href='404.css'>");
}

TEST_P(CssInlineFilterTest, InlineCssCached) {
  // Doing it twice should be safe, too.
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
}

TEST_P(CssInlineFilterTest, InlineCssAbsolutifyUrls1) {
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

TEST_P(CssInlineFilterTest, InlineCssAbsolutifyUrls2) {
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

TEST_P(CssInlineFilterTest, NoAbsolutifyUrlsSameDir) {
  const GoogleString css = "BODY { background-image: url('bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/baz.css",
                "", css, true, css);
}

TEST_P(CssInlineFilterTest, DoNotInlineCssWithMediaAttr) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "media=\"print\"", css, false, "");
}

TEST_P(CssInlineFilterTest, DoInlineCssWithMediaAll) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "media=\"all\"", css, true, css);
}

TEST_P(CssInlineFilterTest, DoNotInlineCssTooBig) {
  // CSS too large to inline:
  const int64 length = 2 * RewriteOptions::kDefaultCssInlineMaxBytes;
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css", "",
                ("BODY { background-image: url('" +
                 GoogleString(length, 'z') + ".png'); }\n"),
                false, "");
}

TEST_P(CssInlineFilterTest, DoNotInlineCssDifferentDomain) {
  // TODO(mdsteele): Is switching domains in fact an issue for CSS?
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.org/styles.css",
                "", "BODY { color: red; }\n", false, "");
}

TEST_P(CssInlineFilterTest, DoNotInlineCssWithImports) {
  // TODO(mdsteele): Is switching domains in fact an issue for CSS?
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", "@import \"foo.css\", BODY { color: red; }\n", false, "");
}

// http://code.google.com/p/modpagespeed/issues/detail?q=css&id=252
TEST_P(CssInlineFilterTest, ClaimsXhtmlButHasUnclosedLink) {
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

TEST_P(CssInlineFilterTest, InlineCombined) {
  // Make sure we interact with CSS combiner properly, including in cached
  // case.
  options()->EnableFilter(RewriteOptions::kInlineCss);
  options()->EnableFilter(RewriteOptions::kCombineCss);
  rewrite_driver()->AddFilters();

  const char kCssUrl[] = "a.css";
  const char kCss[] = "div {display:block;}";

  InitResponseHeaders(kCssUrl, kContentTypeCss, kCss, 3000);

  GoogleString html_input =
      StrCat("<link rel=stylesheet href=\"", kCssUrl, "\">",
             "<link rel=stylesheet href=\"", kCssUrl, "\">");
  GoogleString html_output= StrCat("<style>", kCss, kCss, "</style>");

  ValidateExpected("inline_combined", html_input, html_output);
  ValidateExpected("inline_combined", html_input, html_output);
}

TEST_P(CssInlineFilterTest, InlineMinimizeInteraction) {
  // There was a bug in async mode where we would accidentally prevent
  // minification results from rendering when inlining was not to be done.
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_css_inline_max_bytes(4);

  TestInlineCssWithOutputUrl(
      StrCat(kTestDomain, "minimize_but_not_inline.html"),
      StrCat(kTestDomain, "a.css"),
      StrCat(kTestDomain, "a.css.pagespeed.cf.0.css"),
      "", /* no other attributes*/
      "div{display: none;}",
      false,
      "div{display: none}");
}

// We test with asynchronous_rewrites() == GetParam() as both true and false.
INSTANTIATE_TEST_CASE_P(CssInlineFilterTestInstance,
                        CssInlineFilterTest,
                        ::testing::Bool());


}  // namespace

}  // namespace net_instaweb
