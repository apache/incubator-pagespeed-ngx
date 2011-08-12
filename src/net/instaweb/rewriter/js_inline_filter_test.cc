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

class JsInlineFilterTest : public ResourceManagerTestBase,
                           public ::testing::WithParamInterface<bool> {
 public:
  JsInlineFilterTest() : filters_added_(false) {}

 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    SetAsynchronousRewrites(GetParam());
  }

  void TestInlineJavascript(const GoogleString& html_url,
                            const GoogleString& js_url,
                            const GoogleString& js_original_inline_body,
                            const GoogleString& js_outline_body,
                            bool expect_inline) {
    TestInlineJavascriptGeneral(
        html_url,
        "",  // don't use a doctype for these tests
        js_url,
        js_url,
        js_original_inline_body,
        js_outline_body,
        js_outline_body,  // expect ouline body to be inlined verbatim
        expect_inline);
  }

  void TestInlineJavascriptXhtml(const GoogleString& html_url,
                                 const GoogleString& js_url,
                                 const GoogleString& js_outline_body,
                                 bool expect_inline) {
    TestInlineJavascriptGeneral(
        html_url,
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" "
        "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">",
        js_url,
        js_url,
        "",  // use an empty original inline body for these tests
        js_outline_body,
        // Expect outline body to get surrounded by a CDATA block:
        "//<![CDATA[\n" + js_outline_body + "\n//]]>",
        expect_inline);
  }

  void TestInlineJavascriptGeneral(const GoogleString& html_url,
                                   const GoogleString& doctype,
                                   const GoogleString& js_url,
                                   const GoogleString& js_out_url,
                                   const GoogleString& js_original_inline_body,
                                   const GoogleString& js_outline_body,
                                   const GoogleString& js_expected_inline_body,
                                   bool expect_inline) {
    if (!filters_added_) {
      AddFilter(RewriteOptions::kInlineJavascript);
      filters_added_ = true;
    }

    // Specify the input and expected output.
    if (!doctype.empty()) {
      SetDoctype(doctype);
    }

    const char kHtmlTemplate[] =
        "<head>\n"
        "  <script src=\"%s\">%s</script>\n"
        "</head>\n"
        "<body>Hello, world!</body>\n";

    const GoogleString html_input =
        StringPrintf(kHtmlTemplate, js_url.c_str(),
                     js_original_inline_body.c_str());

    const GoogleString outline_html_output =
          StringPrintf(kHtmlTemplate, js_out_url.c_str(),
                    js_original_inline_body.c_str());

    const GoogleString expected_output =
        (!expect_inline ? outline_html_output :
         "<head>\n"
         "  <script>" + js_expected_inline_body + "</script>\n"
         "</head>\n"
         "<body>Hello, world!</body>\n");

    // Put original Javascript file into our fetcher.
    ResponseHeaders default_js_header;
    SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_js_header);
    SetFetchResponse(js_url, default_js_header, js_outline_body);

    // Rewrite the HTML page.
    ValidateExpectedUrl(html_url, html_input, expected_output);
  }

 private:
  bool filters_added_;
};

TEST_P(JsInlineFilterTest, DoInlineJavascriptSimple) {
  // Simple case:
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       "function id(x) { return x; }\n",
                       true);
}

TEST_P(JsInlineFilterTest, DoInlineJavascriptWhitespace) {
  // Whitespace between <script> and </script>:
  TestInlineJavascript("http://www.example.com/index2.html",
                       "http://www.example.com/script2.js",
                       "\n    \n  ",
                       "function id(x) { return x; }\n",
                       true);
}

TEST_P(JsInlineFilterTest, DoNotInlineJavascriptDifferentDomain) {
  // Different domains:
  TestInlineJavascript("http://www.example.net/index.html",
                       "http://scripts.example.org/script.js",
                       "",
                       "function id(x) { return x; }\n",
                       false);
}

TEST_P(JsInlineFilterTest, DoNotInlineJavascriptInlineContents) {
  // Inline contents:
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "{\"json\": true}",
                       "function id(x) { return x; }\n",
                       false);
}

TEST_P(JsInlineFilterTest, DoNotInlineJavascriptTooBig) {
  // Javascript too long:
  const int64 length = 2 * RewriteOptions::kDefaultJsInlineMaxBytes;
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       ("function longstr() { return '" +
                        GoogleString(length, 'z') + "'; }\n"),
                       false);
}

TEST_P(JsInlineFilterTest, DoNotInlineJavascriptWithCloseTag) {
  // External script contains "</script>":
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       "function close() { return '</script>'; }\n",
                       false);
}

TEST_P(JsInlineFilterTest, DoInlineJavascriptXhtml) {
  // Simple case:
  TestInlineJavascriptXhtml("http://www.example.com/index.html",
                            "http://www.example.com/script.js",
                            "function id(x) { return x; }\n",
                            true);
}

TEST_P(JsInlineFilterTest, DoNotInlineJavascriptXhtmlWithCdataEnd) {
  // External script contains "]]>":
  TestInlineJavascriptXhtml("http://www.example.com/index.html",
                            "http://www.example.com/script.js",
                            "function end(x) { return ']]>'; }\n",
                            false);
}

TEST_P(JsInlineFilterTest, CachedRewrite) {
  // Make sure we work fine when result is cached.
  const char kPageUrl[] = "http://www.example.com/index.html";
  const char kJsUrl[] = "http://www.example.com/script.js";
  const char kJs[] = "function id(x) { return x; }\n";
  const char kNothingInsideScript[] = "";
  TestInlineJavascript(kPageUrl, kJsUrl, kNothingInsideScript, kJs, true);
  TestInlineJavascript(kPageUrl, kJsUrl, kNothingInsideScript, kJs, true);
}

TEST_P(JsInlineFilterTest, CachedWithSuccesors) {
  // Regression test: in async case, at one point we had a problem with
  // slot rendering of a following cache extender trying to manipulate
  // the source attribute which the inliner deleted while using
  // cached filter results.
  options()->EnableFilter(RewriteOptions::kInlineJavascript);
  options()->EnableFilter(RewriteOptions::kExtendCache);
  rewrite_driver()->AddFilters();

  const char kJsUrl[] = "script.js";
  const char kJs[] = "function id(x) { return x; }\n";

  InitResponseHeaders(kJsUrl, kContentTypeJavascript, kJs, 3000);

  GoogleString html_input = StrCat("<script src=\"", kJsUrl, "\"></script>");
  GoogleString html_output= StrCat("<script>", kJs, "</script>");

  ValidateExpected("inline_with_succ", html_input, html_output);
  ValidateExpected("inline_with_succ", html_input, html_output);
}

TEST_P(JsInlineFilterTest, CachedWithPredecessors) {
  // Regression test for crash: trying to inline after combining would crash.
  // (Current state is not to inline after combining due to the
  //  <script> element with src= being new).
  options()->EnableFilter(RewriteOptions::kInlineJavascript);
  options()->EnableFilter(RewriteOptions::kCombineJavascript);
  rewrite_driver()->AddFilters();

  const char kJsUrl[] = "script.js";
  const char kJs[] = "function id(x) { return x; }\n";

  InitResponseHeaders(kJsUrl, kContentTypeJavascript, kJs, 3000);

  GoogleString html_input = StrCat("<script src=\"", kJsUrl, "\"></script>",
                                   "<script src=\"", kJsUrl, "\"></script>");

  Parse("inline_with_pred", html_input);
  Parse("inline_with_pred", html_input);
}

TEST_P(JsInlineFilterTest, InlineJs404) {
  // Test to make sure that a missing input is handled well.
  SetFetchResponse404("404.js");
  AddFilter(RewriteOptions::kInlineJavascript);
  ValidateNoChanges("404", "<script src='404.js'></script>");

  // Second time, to make sure caching doesn't break it.
  ValidateNoChanges("404", "<script src='404.js'></script>");
}

TEST_P(JsInlineFilterTest, InlineMinimizeInteraction) {
  // There was a bug in async mode where we would accidentally prevent
  // minification results from rendering when inlining was not to be done.
  options()->EnableFilter(RewriteOptions::kRewriteJavascript);
  options()->set_js_inline_max_bytes(4);

  TestInlineJavascriptGeneral(
      StrCat(kTestDomain, "minimize_but_not_inline.html"),
      "",  // No doctype
      StrCat(kTestDomain, "a.js"),
      StrCat(kTestDomain, "a.js.pagespeed.jm.0.js"),
      "",  // No inline body in,
      "var answer = 42; // const is non-standard",  // out-of-line body
      "",  // No inline body out,
      false);  // Not inlining
}

TEST_P(JsInlineFilterTest, FlushSplittingScriptTag) {
  options()->EnableFilter(RewriteOptions::kInlineJavascript);
  rewrite_driver()->AddFilters();
  SetupWriter();

  const char kJsUrl[] = "http://www.example.com/script.js";
  const char kJs[] = "function id(x) { return x; }\n";
  InitResponseHeaders(kJsUrl, kContentTypeJavascript, kJs, 3000);

  html_parse()->StartParse("http://www.example.com");
  html_parse()->ParseText("<div><script src=\"script.js\"> ");
  html_parse()->Flush();
  html_parse()->ParseText("</script> </div>");
  html_parse()->FinishParse();
  EXPECT_EQ("<div><script src=\"script.js\"> </script> </div>", output_buffer_);
}

TEST_P(JsInlineFilterTest, NoFlushSplittingScriptTag) {
  options()->EnableFilter(RewriteOptions::kInlineJavascript);
  rewrite_driver()->AddFilters();
  SetupWriter();

  const char kJsUrl[] = "http://www.example.com/script.js";
  const char kJs[] = "function id(x) { return x; }\n";
  InitResponseHeaders(kJsUrl, kContentTypeJavascript, kJs, 3000);

  html_parse()->StartParse("http://www.example.com");
  html_parse()->ParseText("<div><script src=\"script.js\">     ");
  html_parse()->ParseText("     </script> </div>");
  html_parse()->FinishParse();
  EXPECT_EQ("<div><script>function id(x) { return x; }\n</script> </div>",
            output_buffer_);
}

// We test with asynchronous_rewrites() == GetParam() as both true and false.
INSTANTIATE_TEST_CASE_P(JsInlineFilterTestInstance,
                        JsInlineFilterTest,
                        ::testing::Bool());

}  // namespace

}  // namespace net_instaweb
