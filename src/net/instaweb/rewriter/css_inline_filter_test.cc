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

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/charset_util.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

class CssInlineFilterTest : public ResourceManagerTestBase {
 public:
  CssInlineFilterTest() : filters_added_(false) {}

 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
  }

  void TestInlineCssWithOutputUrl(
                     const GoogleString& html_url,
                     const GoogleString& head_extras,
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
        head_extras,
        "  <link rel=\"stylesheet\" href=\"%s\"",
        (other_attrs.empty() ? "" : " " + other_attrs) + ">\n",
        "</head>\n"
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
         StrCat("<head>\n",
                head_extras,
                StrCat("  <style",
                       (other_attrs.empty() ? "" : " " + other_attrs),
                       ">"),
                css_rewritten_body, "</style>\n"
                "</head>\n"
                "<body>Hello, world!</body>\n"));
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  }

  void TestInlineCss(const GoogleString& html_url,
                     const GoogleString& css_url,
                     const GoogleString& other_attrs,
                     const GoogleString& css_original_body,
                     bool expect_inline,
                     const GoogleString& css_rewritten_body) {
    TestInlineCssWithOutputUrl(
        html_url, "", css_url, css_url, other_attrs, css_original_body,
        expect_inline, css_rewritten_body);
  }

 private:
  bool filters_added_;
};

TEST_F(CssInlineFilterTest, InlineCssSimple) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
}

TEST_F(CssInlineFilterTest, InlineCss404) {
  // Test to make sure that a missing input is handled well.
  SetFetchResponse404("404.css");
  ValidateNoChanges("404", "<link rel=stylesheet href='404.css'>");

  // Second time, to make sure caching doesn't break it.
  ValidateNoChanges("404", "<link rel=stylesheet href='404.css'>");
}

TEST_F(CssInlineFilterTest, InlineCssCached) {
  // Doing it twice should be safe, too.
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
}

TEST_F(CssInlineFilterTest, InlineCssRewriteUrls1) {
  // CSS with a relative URL that needs to be changed:
  const GoogleString css1 =
      "BODY { background-image: url('bg.png'); }\n";
  const GoogleString css2 =
      "BODY { background-image: url('foo/bar/bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/foo/bar/baz.css",
                "", css1, true, css2);
}

TEST_F(CssInlineFilterTest, InlineCssRewriteUrls2) {
  // CSS with a relative URL, this time with ".." in it:
  const GoogleString css1 =
      "BODY { background-image: url('../quux/bg.png'); }\n";
  const GoogleString css2 =
      "BODY { background-image: url('foo/quux/bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/foo/bar/baz.css",
                "", css1, true, css2);
}

TEST_F(CssInlineFilterTest, NoRewriteUrlsSameDir) {
  const GoogleString css = "BODY { background-image: url('bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/baz.css",
                "", css, true, css);
}

TEST_F(CssInlineFilterTest, ShardSubresources) {
  UseMd5Hasher();
  DomainLawyer* lawyer = options()->domain_lawyer();
  lawyer->AddShard("www.example.com", "shard1.com,shard2.com",
                   &message_handler_);

  const GoogleString css_in =
      ".p1 { background-image: url('b1.png'); }"
      ".p2 { background-image: url('b2.png'); }";
  const GoogleString css_out =
      ".p1 { background-image: url('http://shard2.com/b1.png'); }"
      ".p2 { background-image: url('http://shard1.com/b2.png'); }";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/baz.css",
                "", css_in, true, css_out);
}

TEST_F(CssInlineFilterTest, DoNotInlineCssWithMediaNotScreen) {
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

TEST_F(CssInlineFilterTest, DoInlineCssWithMediaScreen) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "media=\"print, audio ,, ,sCrEeN \"", css, true, css);
}

TEST_F(CssInlineFilterTest, InlineCssWithUndecodableMedia) {
  // Ensure that our test string really is not decodable, to cater for it
  // becoming decodable in the future.
  const char kNotDecodable[] = "not\240decodable";  // ' ' with high bit set.
  RewriteDriver* driver = rewrite_driver();
  HtmlElement* element = driver->NewElement(NULL, HtmlName::kStyle);
  driver->AddEscapedAttribute(element, HtmlName::kMedia, kNotDecodable);
  HtmlElement::Attribute* attr = element->FindAttribute(HtmlName::kMedia);
  EXPECT_TRUE(NULL == attr->DecodedValueOrNull());

  const GoogleString css = "BODY { color: red; }\n";
  GoogleString media;

  // Now do the actual test that we don't inline the CSS with an undecodable
  // media type (and not screen or all as well).
  media = StrCat("media=\"", kNotDecodable, "\"");
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                media, css, false, "");

  // And now test that we DO inline the CSS with an undecodable media type
  // if ther's also an instance of "screen" in the media attribute.
  media = StrCat("media=\"", kNotDecodable, ",screen\"");
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                media, css, true, css);
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
  // Note: This only fails because we haven't authorized www.example.org
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.org/styles.css",
                "", "BODY { color: red; }\n", false, "");
}

TEST_F(CssInlineFilterTest, CorrectlyInlineCssWithImports) {
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/dir/styles.css", "",
                "@import \"foo.css\"; BODY { color: red; }\n", true,
                "@import \"dir/foo.css\"; BODY { color: red; }\n");
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

TEST_F(CssInlineFilterTest, InlineCombined) {
  // Make sure we interact with CSS combiner properly, including in cached
  // case.
  options()->EnableFilter(RewriteOptions::kInlineCss);
  options()->EnableFilter(RewriteOptions::kCombineCss);
  rewrite_driver()->AddFilters();

  const char kCssUrl[] = "a.css";
  const char kCss[] = "div {display:block;}";

  SetResponseWithDefaultHeaders(kCssUrl, kContentTypeCss, kCss, 3000);

  GoogleString html_input =
      StrCat("<link rel=stylesheet href=\"", kCssUrl, "\">",
             "<link rel=stylesheet href=\"", kCssUrl, "\">");
  GoogleString html_output= StrCat("<style>", kCss, kCss, "</style>");

  ValidateExpected("inline_combined", html_input, html_output);
  ValidateExpected("inline_combined", html_input, html_output);
}

TEST_F(CssInlineFilterTest, InlineMinimizeInteraction) {
  // There was a bug in async mode where we would accidentally prevent
  // minification results from rendering when inlining was not to be done.
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_css_inline_max_bytes(4);

  TestInlineCssWithOutputUrl(
      StrCat(kTestDomain, "minimize_but_not_inline.html"), "",
      StrCat(kTestDomain, "a.css"),
      Encode(kTestDomain, "cf", "0", "a.css", "css"),
      "", /* no other attributes*/
      "div{display: none;}",
      false,
      "div{display: none}");
}

TEST_F(CssInlineFilterTest, CharsetDetermination) {
  // Sigh. rewrite_filter.cc doesn't have its own unit test so we test this
  // method here since we're the only ones that use it.
  GoogleString x_css_url = "x.css";
  GoogleString y_css_url = "y.css";
  GoogleString z_css_url = "z.css";
  const char x_css_body[] = "BODY { color: red; }";
  const char y_css_body[] = "BODY { color: green; }";
  const char z_css_body[] = "BODY { color: blue; }";
  GoogleString y_bom_body = StrCat(kUtf8Bom, y_css_body);
  GoogleString z_bom_body = StrCat(kUtf8Bom, z_css_body);

  // x.css has no charset header nor a BOM.
  // y.css has no charset header but has a BOM.
  // z.css has a charset header and a BOM.
  ResponseHeaders default_header;
  SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_header);
  SetFetchResponse(StrCat(kTestDomain, x_css_url), default_header, x_css_body);
  SetFetchResponse(StrCat(kTestDomain, y_css_url), default_header, y_bom_body);
  default_header.MergeContentType("text/css; charset=iso-8859-1");
  SetFetchResponse(StrCat(kTestDomain, z_css_url), default_header, z_bom_body);

  ResourcePtr x_css_resource(CreateResource(kTestDomain, x_css_url));
  ResourcePtr y_css_resource(CreateResource(kTestDomain, y_css_url));
  ResourcePtr z_css_resource(CreateResource(kTestDomain, z_css_url));
  EXPECT_TRUE(ReadIfCached(x_css_resource));
  EXPECT_TRUE(ReadIfCached(y_css_resource));
  EXPECT_TRUE(ReadIfCached(z_css_resource));

  GoogleString result;
  const StringPiece kUsAsciiCharset("us-ascii");

  // Nothing set: charset should be empty.
  result = RewriteFilter::GetCharsetForStylesheet(x_css_resource.get(), "", "");
  EXPECT_TRUE(result.empty());

  // Only the containing charset is set.
  result = RewriteFilter::GetCharsetForStylesheet(x_css_resource.get(),
                                                  "", kUsAsciiCharset);
  EXPECT_STREQ(result, kUsAsciiCharset);

  // The containing charset is trumped by the element's charset attribute.
  result = RewriteFilter::GetCharsetForStylesheet(x_css_resource.get(),
                                                  "gb", kUsAsciiCharset);
  EXPECT_STREQ("gb", result);

  // The element's charset attribute is trumped by the resource's BOM.
  result = RewriteFilter::GetCharsetForStylesheet(y_css_resource.get(),
                                                  "gb", kUsAsciiCharset);
  EXPECT_STREQ("utf-8", result);

  // The resource's BOM is trumped by the resource's header.
  result = RewriteFilter::GetCharsetForStylesheet(z_css_resource.get(),
                                                  "gb", kUsAsciiCharset);
  EXPECT_STREQ("iso-8859-1", result);
}

TEST_F(CssInlineFilterTest, InlineWithCompatibleBom) {
  const GoogleString css = "BODY { color: red; }\n";
  const GoogleString css_with_bom = StrCat(kUtf8Bom, css);
  TestInlineCssWithOutputUrl("http://www.example.com/index.html",
                             "  <meta charset=\"UTF-8\">\n",
                             "http://www.example.com/styles.css",
                             "http://www.example.com/styles.css",
                             "", css_with_bom, true, css);
}

TEST_F(CssInlineFilterTest, DoNotInlineWithIncompatibleBom) {
  const GoogleString css = "BODY { color: red; }\n";
  const GoogleString css_with_bom = StrCat(kUtf8Bom, css);
  TestInlineCssWithOutputUrl("http://www.example.com/index.html",
                             "  <meta charset=\"ISO-8859-1\">\n",
                             "http://www.example.com/styles.css",
                             "http://www.example.com/styles.css",
                             "", css_with_bom, false, "");
}

}  // namespace

}  // namespace net_instaweb
