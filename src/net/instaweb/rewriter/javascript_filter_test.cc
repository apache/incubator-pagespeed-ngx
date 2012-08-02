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

// Unit-test the javascript filter

#include "net/instaweb/rewriter/public/javascript_filter.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kHtmlFormat[] =
    "<script type='text/javascript' src='%s'></script>\n";

const char kCdataWrapper[] = "//<![CDATA[\n%s\n//]]>";
const char kCdataAltWrapper[] = "//<![CDATA[\r%s\r//]]>";

const char kInlineJs[] =
    "<script type='text/javascript'>%s</script>\n";

const char kJsData[] =
    "alert     (    'hello, world!'    ) "
    " /* removed */ <!-- removed --> "
    " // single-line-comment";
const char kJsMinData[] = "alert('hello, world!')";
const char kFilterId[] = "jm";
const char kOrigJsName[] = "hello.js";
const char kRewrittenJsName[] = "hello.js";

}  // namespace

namespace net_instaweb {

class JavascriptFilterTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    AddFilter(RewriteOptions::kRewriteJavascript);
    expected_rewritten_path_ = Encode(kTestDomain, kFilterId, "0",
                                      kRewrittenJsName, "js");

    blocks_minified_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kBlocksMinified);
    minification_failures_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kMinificationFailures);
    total_bytes_saved_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kTotalBytesSaved);
    total_original_bytes_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kTotalOriginalBytes);
    num_uses_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kMinifyUses);
  }

  void InitTest(int64 ttl) {
    SetResponseWithDefaultHeaders(kOrigJsName, kContentTypeJavascript,
                                  kJsData, ttl);
  }

  // Generate HTML loading 3 resources with the specified URLs
  GoogleString GenerateHtml(const char* a) {
    return StringPrintf(kHtmlFormat, a);
  }

  void TestCorruptUrl(const char* new_suffix) {
    // Do a normal rewrite test
    InitTest(100);
    ValidateExpected("no_ext_corruption",
                    GenerateHtml(kOrigJsName),
                    GenerateHtml(expected_rewritten_path_.c_str()));

    // Fetch messed up URL.
    ASSERT_TRUE(StringCaseEndsWith(expected_rewritten_path_, ".js"));
    GoogleString munged_url =
        ChangeSuffix(expected_rewritten_path_, false /* replace */,
                     ".js", new_suffix);

    GoogleString out;
    EXPECT_TRUE(FetchResourceUrl(munged_url, &out));

    // Rewrite again; should still get normal URL
    ValidateExpected("no_ext_corruption",
                    GenerateHtml(kOrigJsName),
                    GenerateHtml(expected_rewritten_path_.c_str()));
  }

  GoogleString expected_rewritten_path_;

  // Stats
  Variable* blocks_minified_;
  Variable* minification_failures_;
  Variable* total_bytes_saved_;
  Variable* total_original_bytes_;
  Variable* num_uses_;
};

TEST_F(JavascriptFilterTest, DoRewrite) {
  InitTest(100);
  ValidateExpected("do_rewrite",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));

  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(STATIC_STRLEN(kJsData) - STATIC_STRLEN(kJsMinData),
            total_bytes_saved_->Get());
  EXPECT_EQ(STATIC_STRLEN(kJsData), total_original_bytes_->Get());
  EXPECT_EQ(1, num_uses_->Get());
}

TEST_F(JavascriptFilterTest, RewriteAlreadyCachedProperly) {
  InitTest(100000000);  // cached for a long time to begin with
  // But we will rewrite because we can make the data smaller.
  ValidateExpected("rewrite_despite_being_cached_properly",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
}

TEST_F(JavascriptFilterTest, NoRewriteOriginUncacheable) {
  InitTest(0);  // origin not cacheable
  ValidateExpected("no_extend_origin_not_cacheable",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kOrigJsName));

  EXPECT_EQ(0, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(0, total_bytes_saved_->Get());
  EXPECT_EQ(0, total_original_bytes_->Get());
  EXPECT_EQ(0, num_uses_->Get());
}

TEST_F(JavascriptFilterTest, ServeFiles) {
  TestServeFiles(&kContentTypeJavascript, kFilterId, "js",
                 kOrigJsName, kJsData,
                 kRewrittenJsName, kJsMinData);

  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(STATIC_STRLEN(kJsData) - STATIC_STRLEN(kJsMinData),
            total_bytes_saved_->Get());
  EXPECT_EQ(STATIC_STRLEN(kJsData), total_original_bytes_->Get());
  // Note: We do not count any uses, because we did not write the URL into
  // an HTML file, just served it on request.
  EXPECT_EQ(0, num_uses_->Get());

  // Finally, serve from a completely separate server.
  ServeResourceFromManyContexts(expected_rewritten_path_, kJsMinData);
}

TEST_F(JavascriptFilterTest, InvalidInputMimetype) {
  // Make sure we can rewrite properly even when input has corrupt mimetype.
  ContentType not_java_script = kContentTypeJavascript;
  not_java_script.mime_type_ = "text/semicolon-inserted";
  const char* kNotJsFile = "script.notjs";

  SetResponseWithDefaultHeaders(kNotJsFile, not_java_script, kJsData, 100);
  ValidateExpected("wrong_mime",
                   GenerateHtml(kNotJsFile),
                   GenerateHtml(Encode(kTestDomain, "jm", "0",
                                       kNotJsFile, "js").c_str()));
}

TEST_F(JavascriptFilterTest, RewriteJs404) {
  // Test to make sure that a missing input is handled well.
  SetFetchResponse404("404.js");
  ValidateNoChanges("404", "<script src='404.js'></script>");
  EXPECT_EQ(0, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(0, num_uses_->Get());

  // Second time, to make sure caching doesn't break it.
  ValidateNoChanges("404", "<script src='404.js'></script>");
  EXPECT_EQ(0, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(0, num_uses_->Get());
}

// Make sure bad requests do not corrupt our extension.
TEST_F(JavascriptFilterTest, NoExtensionCorruption) {
  TestCorruptUrl(".js%22");
}

TEST_F(JavascriptFilterTest, NoQueryCorruption) {
  TestCorruptUrl(".js?query");
}

TEST_F(JavascriptFilterTest, NoWrongExtCorruption) {
  TestCorruptUrl(".html");
}

TEST_F(JavascriptFilterTest, InlineJavascript) {
  // Test minification of a simple inline script
  InitTest(100);
  ValidateExpected("inline javascript",
                   StringPrintf(kInlineJs, kJsData),
                   StringPrintf(kInlineJs, kJsMinData));

  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(STATIC_STRLEN(kJsData) - STATIC_STRLEN(kJsMinData),
            total_bytes_saved_->Get());
  EXPECT_EQ(STATIC_STRLEN(kJsData), total_original_bytes_->Get());
  EXPECT_EQ(1, num_uses_->Get());
}

TEST_F(JavascriptFilterTest, StripInlineWhitespace) {
  // Make sure we strip inline whitespace when minifying external scripts.
  InitTest(100);
  ValidateExpected(
      "StripInlineWhitespace",
      StrCat("<script src='", kOrigJsName, "'>   \t\n   </script>"),
      StrCat("<script src='",
             Encode(kTestDomain, "jm", "0", kOrigJsName, "js"),
             "'></script>"));
}

TEST_F(JavascriptFilterTest, RetainInlineData) {
  // Test to make sure we keep inline data when minifying external scripts.
  InitTest(100);
  ValidateExpected("StripInlineWhitespace",
                   StrCat("<script src='", kOrigJsName, "'> data </script>"),
                   StrCat("<script src='",
                          Encode(kTestDomain, "jm", "0", kOrigJsName, "js"),
                          "'> data </script>"));
}

// Test minification of a simple inline script in markup with no
// mimetype, where the script is wrapped in a commented-out CDATA.
//
// Note that javascript_filter never adds CDATA.  It only removes it
// if it's sure the mimetype is HTML.
TEST_F(JavascriptFilterTest, CdataJavascriptNoMimetype) {
  InitTest(100);
  ValidateExpected(
      "cdata javascript no mimetype",
      StringPrintf(kInlineJs, StringPrintf(kCdataWrapper, kJsData).c_str()),
      StringPrintf(kInlineJs, StringPrintf(kCdataWrapper, kJsMinData).c_str()));
  ValidateExpected(
      "cdata javascript no mimetype with \\r",
      StringPrintf(kInlineJs, StringPrintf(kCdataAltWrapper, kJsData).c_str()),
      StringPrintf(kInlineJs, StringPrintf(kCdataWrapper, kJsMinData).c_str()));
}

// Same as CdataJavascriptNoMimetype, but with explicit HTML mimetype.
TEST_F(JavascriptFilterTest, CdataJavascriptHtmlMimetype) {
  SetHtmlMimetype();
  InitTest(100);
  ValidateExpected(
      "cdata javascript with explicit HTML mimetype",
      StringPrintf(kInlineJs, StringPrintf(kCdataWrapper, kJsData).c_str()),
      StringPrintf(kInlineJs, kJsMinData));
  ValidateExpected(
      "cdata javascript with explicit HTML mimetype and \\r",
      StringPrintf(kInlineJs, StringPrintf(kCdataAltWrapper, kJsData).c_str()),
      StringPrintf(kInlineJs, kJsMinData));
}

// Same as CdataJavascriptNoMimetype, but with explicit XHTML mimetype.
TEST_F(JavascriptFilterTest, CdataJavascriptXhtmlMimetype) {
  SetXhtmlMimetype();
  InitTest(100);
  ValidateExpected(
      "cdata javascript with explicit XHTML mimetype",
      StringPrintf(kInlineJs, StringPrintf(kCdataWrapper, kJsData).c_str()),
      StringPrintf(kInlineJs, StringPrintf(kCdataWrapper, kJsMinData).c_str()));
  ValidateExpected(
      "cdata javascript with explicit XHTML mimetype and \\r",
      StringPrintf(kInlineJs, StringPrintf(kCdataAltWrapper, kJsData).c_str()),
      StringPrintf(kInlineJs, StringPrintf(kCdataWrapper, kJsMinData).c_str()));
}

TEST_F(JavascriptFilterTest, XHtmlInlineJavascript) {
  // Test minification of a simple inline script in xhtml
  // where it must be wrapped in CDATA.
  InitTest(100);
  const GoogleString xhtml_script_format =
      StrCat(kXhtmlDtd, StringPrintf(kInlineJs, kCdataWrapper));
  ValidateExpected("xhtml inline javascript",
                   StringPrintf(xhtml_script_format.c_str(), kJsData),
                   StringPrintf(xhtml_script_format.c_str(), kJsMinData));
  const GoogleString xhtml_script_alt_format =
      StrCat(kXhtmlDtd, StringPrintf(kInlineJs, kCdataAltWrapper));
  ValidateExpected("xhtml inline javascript",
                   StringPrintf(xhtml_script_alt_format.c_str(), kJsData),
                   StringPrintf(xhtml_script_format.c_str(), kJsMinData));
}

// http://code.google.com/p/modpagespeed/issues/detail?id=324
TEST_F(JavascriptFilterTest, RetainExtraHeaders) {
  GoogleString url = StrCat(kTestDomain, kOrigJsName);
  SetResponseWithDefaultHeaders(url, kContentTypeJavascript, kJsData, 300);
  TestRetainExtraHeaders(kOrigJsName, "jm", "js");
}

// http://code.google.com/p/modpagespeed/issues/detail?id=327 -- we were
// previously busting regexps with backslashes in them.
TEST_F(JavascriptFilterTest, BackslashInRegexp) {
  GoogleString input = StringPrintf(kInlineJs, "/http:\\/\\/[^/]+\\//");
  ValidateNoChanges("backslash_in_regexp", input);
}

TEST_F(JavascriptFilterTest, WeirdSrcCrash) {
  // These used to crash due to bugs in the lexer breaking invariants some
  // filters relied on.
  //
  // Note that the attribute-value "foo<bar" gets converted into "foo%3Cbar"
  // by this line:
  //   const GoogleUrl resource_url(base_url(), input_url);
  // in CommonFilter::CreateInputResource.  Following that, resource_url.Spec()
  // has the %3C in it.  I guess that's probably the right thing to do, but
  // I was a little surprised.
  static const char kUrl[] = "foo%3Cbar";
  SetResponseWithDefaultHeaders(kUrl, kContentTypeJavascript, kJsData, 300);
  ValidateExpected("weird_attr", "<script src=foo<bar>Content",
                   StrCat("<script src=",
                          Encode(kTestDomain, "jm", "0", kUrl, "js"),
                          ">Content"));
  ValidateNoChanges("weird_tag", "<script<foo>");
}

TEST_F(JavascriptFilterTest, MinificationFailure) {
  SetResponseWithDefaultHeaders("foo.js", kContentTypeJavascript,
                                "/* truncated comment", 100);
  ValidateNoChanges("fail", "<script src=foo.js></script>");

  EXPECT_EQ(0, blocks_minified_->Get());
  EXPECT_EQ(1, minification_failures_->Get());
  EXPECT_EQ(0, num_uses_->Get());
}

TEST_F(JavascriptFilterTest, ReuseRewrite) {
  InitTest(100);

  ValidateExpected("reuse_rewrite1",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
  // First time: We minify JS and use the minified version.
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(1, num_uses_->Get());

  ClearStats();
  ValidateExpected("reuse_rewrite2",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
  // Second time: We reuse the original rewrite.
  EXPECT_EQ(0, blocks_minified_->Get());
  EXPECT_EQ(1, num_uses_->Get());
}

TEST_F(JavascriptFilterTest, NoReuseInline) {
  InitTest(100);

  ValidateExpected("reuse_inline1",
                   StringPrintf(kInlineJs, kJsData),
                   StringPrintf(kInlineJs, kJsMinData));
  // First time: We minify JS and use the minified version.
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(1, num_uses_->Get());

  ClearStats();
  ValidateExpected("reuse_inline2",
                   StringPrintf(kInlineJs, kJsData),
                   StringPrintf(kInlineJs, kJsMinData));
  // Second time: Apparently we minify it again.
  // NOTE: This test is here to document current behavior. It should be fine
  // to change this behavior so that the rewrite is cached (although it may
  // not be worth it).
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(1, num_uses_->Get());
}

TEST_F(JavascriptFilterTest, FlushInInlineJS) {
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText("<html><body><script>  alert  (  'Hel");
  // Flush in middle of inline JS.
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("lo, World!'  )  </script></body></html>");
  rewrite_driver()->FinishParse();

  // Expect text to be rewritten because it is coalesced.
  // HtmlParse will send events like this to filter:
  //   StartElement script
  //   Flush
  //   Characters ...
  //   EndElement script
  EXPECT_EQ("<html><body><script>alert('Hello, World!')</script></body></html>",
            output_buffer_);
}


TEST_F(JavascriptFilterTest, FlushInEndTag) {
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText(
      "<html><body><script>  alert  (  'Hello, World!'  )  </scr");
  // Flush in middle of closing </script> tag.
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("ipt></body></html>");
  rewrite_driver()->FinishParse();

  // Expect text to be rewritten because it is coalesced.
  // HtmlParse will send events like this to filter:
  //   StartElement script
  //   Characters ...
  //   Flush
  //   EndElement script
  EXPECT_EQ("<html><body><script>alert('Hello, World!')</script></body></html>",
            output_buffer_);
}


}  // namespace net_instaweb
