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

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kXhtmlHeader[] =
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" "
    "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">";

const char kHtmlFormat[] =
    "<script type='text/javascript' src='%s'></script>\n";

const char kCdataWrapper[] = "//<![CDATA[\n%s\n//]]>";

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

class JavascriptFilterTest : public ResourceManagerTestBase,
                             public ::testing::WithParamInterface<bool> {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
    SetAsynchronousRewrites(GetParam());
    AddFilter(RewriteOptions::kRewriteJavascript);
    expected_rewritten_path_ = Encode(kTestDomain, kFilterId, "0",
                                      kRewrittenJsName, "js");
  }

  void InitTest(int64 ttl) {
    InitResponseHeaders(kOrigJsName, kContentTypeJavascript, kJsData, ttl);
  }

  // Generate HTML loading 3 resources with the specified URLs
  GoogleString GenerateHtml(const char* a) {
    return StringPrintf(kHtmlFormat, a);
  }

  void TestCorruptUrl(const char* junk, bool should_fetch_ok) {
    // Do a normal rewrite test
    InitTest(100);
    ValidateExpected("no_ext_corruption",
                    GenerateHtml(kOrigJsName),
                    GenerateHtml(expected_rewritten_path_.c_str()));

    // Fetch messed up URL.
    GoogleString out;
    EXPECT_EQ(should_fetch_ok,
              ServeResourceUrl(StrCat(expected_rewritten_path_, junk), &out));

    // Rewrite again; should still get normal URL
    ValidateExpected("no_ext_corruption",
                    GenerateHtml(kOrigJsName),
                    GenerateHtml(expected_rewritten_path_.c_str()));
  }

  GoogleString expected_rewritten_path_;
};

TEST_P(JavascriptFilterTest, DoRewrite) {
  InitTest(100);
  ValidateExpected("do_rewrite",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
}

TEST_P(JavascriptFilterTest, RewriteAlreadyCachedProperly) {
  InitTest(100000000);  // cached for a long time to begin with
  // But we will rewrite because we can make the data smaller.
  ValidateExpected("rewrite_despite_being_cached_properly",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
}

TEST_P(JavascriptFilterTest, NoRewriteOriginUncacheable) {
  InitTest(0);  // origin not cacheable
  ValidateExpected("no_extend_origin_not_cacheable",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kOrigJsName));
}

TEST_P(JavascriptFilterTest, ServeFiles) {
  TestServeFiles(&kContentTypeJavascript, kFilterId, "js",
                 kOrigJsName, kJsData,
                 kRewrittenJsName, kJsMinData);

  // Finally, serve from a completely separate server.
  ServeResourceFromManyContexts(expected_rewritten_path_, kJsMinData);
}

TEST_P(JavascriptFilterTest, InvalidInputMimetype) {
  // Make sure we can rewrite properly even when input has corrupt mimetype.
  ContentType not_java_script = kContentTypeJavascript;
  not_java_script.mime_type_ = "text/semicolon-inserted";
  const char* kNotJsFile = "script.notjs";

  InitResponseHeaders(kNotJsFile, not_java_script, kJsData, 100);
  ValidateExpected("wrong_mime",
                   GenerateHtml(kNotJsFile),
                   GenerateHtml(Encode(kTestDomain, "jm", "0",
                                       kNotJsFile, "js").c_str()));
}

TEST_P(JavascriptFilterTest, RewriteJs404) {
  // Test to make sure that a missing input is handled well.
  SetFetchResponse404("404.js");
  ValidateNoChanges("404", "<script src='404.js'></script>");

  // Second time, to make sure caching doesn't break it.
  ValidateNoChanges("404", "<script src='404.js'></script>");
}

// Make sure bad requests do not corrupt our extension.
TEST_P(JavascriptFilterTest, NoExtensionCorruption) {
  TestCorruptUrl("%22", false);
}

TEST_P(JavascriptFilterTest, NoQueryCorruption) {
  TestCorruptUrl("?query", true);
}

TEST_P(JavascriptFilterTest, InlineJavascript) {
  // Test minification of a simple inline script
  InitTest(100);
  ValidateExpected("inline javascript",
                   StringPrintf(kInlineJs, kJsData),
                   StringPrintf(kInlineJs, kJsMinData));
}

TEST_P(JavascriptFilterTest, StripInlineWhitespace) {
  // Make sure we strip inline whitespace when minifying external scripts.
  InitTest(100);
  ValidateExpected(
      "StripInlineWhitespace",
      StrCat("<script src='", kOrigJsName, "'>   \t\n   </script>"),
      StrCat("<script src='",
             Encode(kTestDomain, "jm", "0", kOrigJsName, "js"),
             "'></script>"));
}

TEST_P(JavascriptFilterTest, RetainInlineData) {
  // Test to make sure we keep inline data when minifying external scripts.
  InitTest(100);
  ValidateExpected("StripInlineWhitespace",
                   StrCat("<script src='", kOrigJsName, "'> data </script>"),
                   StrCat("<script src='",
                          Encode(kTestDomain, "jm", "0", kOrigJsName, "js"),
                          "'> data </script>"));
}

TEST_P(JavascriptFilterTest, CdataJavascript) {
  // Test minification of a simple inline script in html (NOT xhtml) where the
  // script is wrapped in a commented-out CDATA.
  InitTest(100);
  ValidateExpected(
      "cdata non-xhtml javascript",
      StringPrintf(kInlineJs, StringPrintf(kCdataWrapper, kJsData).c_str()),
      StringPrintf(kInlineJs, kJsMinData));
}

TEST_P(JavascriptFilterTest, XHtmlInlineJavascript) {
  // Test minification of a simple inline script in xhtml
  // where it must be wrapped in CDATA.
  InitTest(100);
  const GoogleString xhtml_script_format =
      StrCat(kXhtmlHeader, StringPrintf(kInlineJs, kCdataWrapper));
  ValidateExpected("xhtml inline javascript",
                   StringPrintf(xhtml_script_format.c_str(), kJsData),
                   StringPrintf(xhtml_script_format.c_str(), kJsMinData));
}

// http://code.google.com/p/modpagespeed/issues/detail?id=324
TEST_P(JavascriptFilterTest, RetainExtraHeaders) {
  GoogleString url = StrCat(kTestDomain, kOrigJsName);
  InitResponseHeaders(url, kContentTypeJavascript, kJsData, 300);
  TestRetainExtraHeaders(kOrigJsName, kOrigJsName, "jm", "js");
}

// http://code.google.com/p/modpagespeed/issues/detail?id=327 -- we were
// previously busting regexps with backslashes in them.
TEST_P(JavascriptFilterTest, BackslashInRegexp) {
  GoogleString input = StringPrintf(kInlineJs, "/http:\\/\\/[^/]+\\//");
  ValidateNoChanges("backslash_in_regexp", input);
}

// We runs the test with GetParam() both true and false, in order to
// test both the traditional and async flows.
INSTANTIATE_TEST_CASE_P(JavascriptFilterTestInstance,
                        JavascriptFilterTest,
                        ::testing::Bool());

}  // namespace net_instaweb
