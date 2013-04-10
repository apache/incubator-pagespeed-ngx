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
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace {

const char kHtmlFormat[] =
    "<script type='text/javascript' src='%s'></script>\n";
const char kInlineScriptFormat[] =
    "<script type='text/javascript'>"
    "%s"
    "</script>";
const char kEndInlineScript[] = "<script type='text/javascript'>";

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
const char kLibraryUrl[] = "https://www.example.com/hello/1.0/hello.js";

}  // namespace

namespace net_instaweb {

class JavascriptFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    expected_rewritten_path_ = Encode(kTestDomain, kFilterId, "0",
                                      kRewrittenJsName, "js");

    blocks_minified_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kBlocksMinified);
    libraries_identified_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kLibrariesIdentified);
    minification_failures_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kMinificationFailures);
    total_bytes_saved_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kTotalBytesSaved);
    total_original_bytes_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kTotalOriginalBytes);
    num_uses_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kMinifyUses);
    did_not_shrink_ = statistics()->GetVariable(
        JavascriptRewriteConfig::kJSDidNotShrink);
  }

  void InitFilters() {
    options()->EnableFilter(RewriteOptions::kRewriteJavascript);
    options()->EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
    rewrite_driver_->AddFilters();
  }

  void InitTest(int64 ttl) {
    SetResponseWithDefaultHeaders(
        kOrigJsName, kContentTypeJavascript, kJsData, ttl);
  }

  void InitFiltersAndTest(int64 ttl) {
    InitFilters();
    InitTest(ttl);
  }

  void RegisterLibrary() {
    MD5Hasher hasher(JavascriptLibraryIdentification::kNumHashChars);
    GoogleString hash = hasher.Hash(kJsMinData);
    EXPECT_TRUE(
        options()->RegisterLibrary(
            STATIC_STRLEN(kJsMinData), hash, kLibraryUrl));
    EXPECT_EQ(JavascriptLibraryIdentification::kNumHashChars, hash.size());
  }

  // Generate HTML loading a single script with the specified URL.
  GoogleString GenerateHtml(const char* a) {
    return StringPrintf(kHtmlFormat, a);
  }

  // Generate HTML loading a single script twice from the specified URL.
  GoogleString GenerateTwoHtml(const char* a) {
    GoogleString once = GenerateHtml(a);
    return StrCat(once, once);
  }

  void TestCorruptUrl(const char* new_suffix) {
    // Do a normal rewrite test
    InitFiltersAndTest(100);
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
  Variable* libraries_identified_;
  Variable* minification_failures_;
  Variable* did_not_shrink_;
  Variable* total_bytes_saved_;
  Variable* total_original_bytes_;
  Variable* num_uses_;
};

TEST_F(JavascriptFilterTest, DoRewrite) {
  InitFiltersAndTest(100);
  AbstractLogRecord* log_record =
      rewrite_driver_->request_context()->log_record();
  log_record->SetAllowLoggingUrls(true);
  ValidateExpected("do_rewrite",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));

  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(STATIC_STRLEN(kJsData) - STATIC_STRLEN(kJsMinData),
            total_bytes_saved_->Get());
  EXPECT_EQ(STATIC_STRLEN(kJsData), total_original_bytes_->Get());
  EXPECT_EQ(1, num_uses_->Get());
  EXPECT_STREQ("jm", AppliedRewriterStringFromLog());
  VerifyRewriterInfoEntry(log_record, "jm", 0, 0, 1, 1,
                        "http://test.com/hello.js");
}

TEST_F(JavascriptFilterTest, RewriteButExceedLogThreshold) {
  InitFiltersAndTest(100);
  rewrite_driver_->log_record()->SetRewriterInfoMaxSize(0);
  ValidateExpected("do_rewrite",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
  EXPECT_STREQ("", AppliedRewriterStringFromLog());
}

TEST_F(JavascriptFilterTest, DoRewriteUnhealthy) {
  lru_cache()->set_is_healthy(false);

  InitFiltersAndTest(100);
  ValidateNoChanges("do_rewrite", GenerateHtml(kOrigJsName));
  EXPECT_STREQ("", AppliedRewriterStringFromLog());
}

TEST_F(JavascriptFilterTest, RewriteAlreadyCachedProperly) {
  InitFiltersAndTest(100000000);  // cached for a long time to begin with
  // But we will rewrite because we can make the data smaller.
  ValidateExpected("rewrite_despite_being_cached_properly",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
}

TEST_F(JavascriptFilterTest, NoRewriteOriginUncacheable) {
  InitFiltersAndTest(0);  // origin not cacheable
  ValidateExpected("no_extend_origin_not_cacheable",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kOrigJsName));

  EXPECT_EQ(0, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(0, total_bytes_saved_->Get());
  EXPECT_EQ(0, total_original_bytes_->Get());
  EXPECT_EQ(0, num_uses_->Get());
}

TEST_F(JavascriptFilterTest, IdentifyLibrary) {
  RegisterLibrary();
  InitFiltersAndTest(100);
  ValidateExpected("identify_library",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kLibraryUrl));

  EXPECT_EQ(1, libraries_identified_->Get());
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
}

TEST_F(JavascriptFilterTest, IdentifyLibraryTwice) {
  // Make sure cached recognition is handled properly.
  RegisterLibrary();
  InitFiltersAndTest(100);
  ValidateExpected("identify_library_twice",
                   GenerateTwoHtml(kOrigJsName),
                   GenerateTwoHtml(kLibraryUrl));
  // The second rewrite uses cached data from the first rewrite.
  EXPECT_EQ(1, libraries_identified_->Get());
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
}

TEST_F(JavascriptFilterTest, JsPreserveURLsOnTest) {
  // Make sure that when in conservative mode the URL stays the same.
  RegisterLibrary();
  options()->EnableFilter(RewriteOptions::kRewriteJavascript);
  options()->EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  options()->set_js_preserve_urls(true);
  rewrite_driver()->AddFilters();
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kRewriteJavascript));
  // Verify that preserve had a chance to forbid some filters.
  EXPECT_FALSE(options()->Enabled(
      RewriteOptions::kCanonicalizeJavascriptLibraries));
  InitTest(100);
  // Make sure the URL doesn't change.
  ValidateExpected("js_urls_preserved",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kOrigJsName));

  // We should have optimized the JS even though we didn't render the URL.
  ClearStats();
  GoogleString out_js_url = Encode(kTestDomain, "jm", "0", kRewrittenJsName,
                                   "js");
  GoogleString out_js;
  EXPECT_TRUE(FetchResourceUrl(out_js_url, &out_js));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));

  // Was the JS minified?
  EXPECT_EQ(kJsMinData, out_js);
}

TEST_F(JavascriptFilterTest, JsPreserveURLsNoPreemptiveRewriteTest) {
  // Make sure that when in conservative mode the URL stays the same.
  RegisterLibrary();
  options()->EnableFilter(RewriteOptions::kRewriteJavascript);
  options()->EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  options()->set_js_preserve_urls(true);
  options()->set_in_place_preemptive_rewrite_javascript(false);
  rewrite_driver()->AddFilters();
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kRewriteJavascript));
  // Verify that preserve had a chance to forbid some filters.
  EXPECT_FALSE(options()->Enabled(
      RewriteOptions::kCanonicalizeJavascriptLibraries));
  InitTest(100);
  // Make sure the URL doesn't change.
  ValidateExpected("js_urls_preserved_no_preemptive",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kOrigJsName));

  // We should not have attempted any rewriting.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_hits()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_misses()));
  EXPECT_EQ(0, static_cast<int>(lru_cache()->num_inserts()));

  // But, if we fetch the JS directly, we should receive the optimized version.
  ClearStats();
  GoogleString out_js_url = Encode(kTestDomain, "jm", "0", kRewrittenJsName,
                                   "js");
  GoogleString out_js;
  EXPECT_TRUE(FetchResourceUrl(out_js_url, &out_js));
  EXPECT_EQ(kJsMinData, out_js);
}

TEST_F(JavascriptFilterTest, IdentifyLibraryNoMinification) {
  // Don't enable kRewriteJavascript.  This should still identify the library.
  RegisterLibrary();
  options()->EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  rewrite_driver_->AddFilters();
  InitTest(100);
  ValidateExpected("identify_library_no_minification",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kLibraryUrl));

  EXPECT_EQ(1, libraries_identified_->Get());
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(0, total_bytes_saved_->Get());
}

TEST_F(JavascriptFilterTest, IdentifyFailureNoMinification) {
  // Don't enable kRewriteJavascript.  We should attempt library identification,
  // fail, and not modify the code even though it can be minified.
  options()->EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  rewrite_driver_->AddFilters();
  InitTest(100);
  // We didn't register any libraries, so we should see that minification
  // happened but that nothing changed on the page.
  ValidateExpected("identify_failure_no_minification",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kOrigJsName));

  EXPECT_EQ(0, libraries_identified_->Get());
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
}

TEST_F(JavascriptFilterTest, IgnoreLibraryNoIdentification) {
  RegisterLibrary();
  // We register the library but don't enable library redirection.
  options()->EnableFilter(RewriteOptions::kRewriteJavascript);
  rewrite_driver_->AddFilters();
  InitTest(100);
  ValidateExpected("ignore_library",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));

  EXPECT_EQ(0, libraries_identified_->Get());
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
}

TEST_F(JavascriptFilterTest, DontCombineIdentified) {
  // Don't combine a 3rd-party library with other scripts if we'd otherwise
  // redirect that library to its canonical url.  Doing so will cause us to
  // download content that we think has a fair probability of being cached in
  // the browser already.  If we're better off combining, we shouldn't be
  // considering the library as a candidate for library identification in the
  // first place.
  RegisterLibrary();
  options()->EnableFilter(RewriteOptions::kCombineJavascript);
  InitFiltersAndTest(100);
  ValidateExpected("DontCombineIdentified",
                   GenerateTwoHtml(kOrigJsName),
                   GenerateTwoHtml(kLibraryUrl));
}

TEST_F(JavascriptFilterTest, DontInlineIdentified) {
  // Don't inline a one-line library that was rewritten to a canonical url.
  RegisterLibrary();
  options()->EnableFilter(RewriteOptions::kInlineJavascript);
  InitFiltersAndTest(100);
  ValidateExpected("DontInlineIdentified",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kLibraryUrl));
}

TEST_F(JavascriptFilterTest, ServeFiles) {
  InitFilters();
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

TEST_F(JavascriptFilterTest, ServeFilesUnhealthy) {
  lru_cache()->set_is_healthy(false);

  InitFilters();
  InitTest(100);
  TestServeFiles(&kContentTypeJavascript, kFilterId, "js",
                 kOrigJsName, kJsData,
                 kRewrittenJsName, kJsMinData);
}

TEST_F(JavascriptFilterTest, ServeRewrittenLibrary) {
  // If a request comes in for the rewritten version of a JS library
  // that we have identified as matching a canonical library, we should
  // still serve some useful content.  It won't be minified because we
  // don't want to update metadata cache entries on the fly.
  RegisterLibrary();
  InitFiltersAndTest(100);
  GoogleString content;
  EXPECT_TRUE(
      FetchResource(kTestDomain, "jm", kRewrittenJsName, "js", &content));
  EXPECT_EQ(kJsData, content);

  // And having done so, we should still identify the library in subsequent html
  // (ie the cache should not be corrupted to prevent library identification).
  ValidateExpected("identify_library",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kLibraryUrl));
}

TEST_F(JavascriptFilterTest, IdentifyAjaxLibrary) {
  // If ajax rewriting is enabled, we won't minify a library when it is fetched,
  // but it will still be replaced on the containing page.
  RegisterLibrary();
  options()->set_in_place_rewriting_enabled(true);
  options()->EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  rewrite_driver_->AddFilters();
  InitTest(100);
  GoogleString url = StrCat(kTestDomain, kOrigJsName);
  // Do resource fetch for js; this will cause it to be filtered in the
  // background.
  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(url, &content));
  EXPECT_EQ(kJsData, content);
  // A second resource fetch for the js will still obtain the unminified
  // content, since we don't save the minified version for identified libraries.
  content.clear();
  EXPECT_TRUE(FetchResourceUrl(url, &content));
  EXPECT_EQ(kJsData, content);
  // But rewriting the page will see the js url.
  ValidateExpected("IdentifyAjaxLibrary",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kLibraryUrl));
}

TEST_F(JavascriptFilterTest, InvalidInputMimetype) {
  InitFilters();
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
  InitFilters();
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
  InitFiltersAndTest(100);
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

TEST_F(JavascriptFilterTest, NoMinificationInlineJS) {
  // Test no minification of a simple inline script.
  InitFiltersAndTest(100);
  const char kSmallJS[] = "alert('hello');";
  ValidateExpected("inline javascript",
                   StringPrintf(kInlineJs, kSmallJS),
                   StringPrintf(kInlineJs, kSmallJS));

  // There was no minification error, so 1 here.
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(1, did_not_shrink_->Get());
  // No failures though.
  EXPECT_EQ(0, minification_failures_->Get());
}

TEST_F(JavascriptFilterTest, StripInlineWhitespace) {
  // Make sure we strip inline whitespace when minifying external scripts.
  InitFiltersAndTest(100);
  ValidateExpected(
      "StripInlineWhitespace",
      StrCat("<script src='", kOrigJsName, "'>   \t\n   </script>"),
      StrCat("<script src='",
             Encode(kTestDomain, "jm", "0", kOrigJsName, "js"),
             "'></script>"));
}

TEST_F(JavascriptFilterTest, RetainInlineData) {
  // Test to make sure we keep inline data when minifying external scripts.
  InitFiltersAndTest(100);
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
  InitFiltersAndTest(100);
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
  InitFiltersAndTest(100);
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
  InitFiltersAndTest(100);
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
  InitFiltersAndTest(100);
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
  InitFilters();
  GoogleString url = StrCat(kTestDomain, kOrigJsName);
  SetResponseWithDefaultHeaders(url, kContentTypeJavascript, kJsData, 300);
  TestRetainExtraHeaders(kOrigJsName, "jm", "js");
}

// http://code.google.com/p/modpagespeed/issues/detail?id=327 -- we were
// previously busting regexps with backslashes in them.
TEST_F(JavascriptFilterTest, BackslashInRegexp) {
  InitFilters();
  GoogleString input = StringPrintf(kInlineJs, "/http:\\/\\/[^/]+\\//");
  ValidateNoChanges("backslash_in_regexp", input);
}

TEST_F(JavascriptFilterTest, WeirdSrcCrash) {
  InitFilters();
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
  InitFilters();
  SetResponseWithDefaultHeaders("foo.js", kContentTypeJavascript,
                                "/* truncated comment", 100);
  ValidateNoChanges("fail", "<script src=foo.js></script>");

  EXPECT_EQ(0, blocks_minified_->Get());
  EXPECT_EQ(1, minification_failures_->Get());
  EXPECT_EQ(0, num_uses_->Get());
  EXPECT_EQ(1, did_not_shrink_->Get());
}

TEST_F(JavascriptFilterTest, ReuseRewrite) {
  InitFiltersAndTest(100);

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
  InitFiltersAndTest(100);

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

// See http://code.google.com/p/modpagespeed/issues/detail?id=542
TEST_F(JavascriptFilterTest, ExtraCdataOnMalformedInput) {
  InitFiltersAndTest(100);

  // This is an entirely bogus thing to have in a script tag, but that was
  // what was reported  by a user.  We were wrapping this an an extra CDATA
  // tag, so this test proves we are no longer doing that.
  static const char kIssue542LinkInScript[] =
      "<![CDATA[<link href='http://fonts.googleapis.com/css'>]]>";

  const GoogleString kHtmlInput = StringPrintf(
      kInlineScriptFormat,
      StrCat("\n", kIssue542LinkInScript, "\n").c_str());
  const GoogleString kHtmlOutput = StringPrintf(
      kInlineScriptFormat,
      kIssue542LinkInScript);
  ValidateExpected("broken_cdata", kHtmlInput, kHtmlOutput);
}

TEST_F(JavascriptFilterTest, ValidCdata) {
  InitFiltersAndTest(100);

  const GoogleString kHtmlInput = StringPrintf(
      kInlineScriptFormat,
      StringPrintf(kCdataWrapper, "alert ( 'foo' ) ; \n").c_str());
  const GoogleString kHtmlOutput = StringPrintf(
      kInlineScriptFormat,
      StringPrintf(kCdataWrapper, "alert('foo');").c_str());
  ValidateExpected("valid_cdata", kHtmlInput, kHtmlOutput);
}

TEST_F(JavascriptFilterTest, FlushInInlineJS) {
  InitFilters();
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
  InitFilters();
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

TEST_F(JavascriptFilterTest, FlushAfterBeginScript) {
  InitFilters();
  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText(
      "<html><body><script>");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("alert  (  'Hello, World!'  )  </script>"
                              "<script></script></body></html>");
  rewrite_driver()->FinishParse();

  // Expect text to be rewritten because it is coalesced.
  // HtmlParse will send events like this to filter:
  //   StartElement script
  //   Flush
  //   Characters ...
  //   EndElement script
  //   StartElement script
  //   EndElement script
  EXPECT_EQ("<html><body><script>alert('Hello, World!')</script>"
            "<script></script></body></html>",
            output_buffer_);
}

TEST_F(JavascriptFilterTest, StripInlineWhitespaceFlush) {
  // Make sure we strip inline whitespace when minifying external scripts even
  // if there's a flush between open and close.
  InitFiltersAndTest(100);
  SetupWriter();
  const GoogleString kScriptTag =
      StrCat("<script type='text/javascript' src='", kOrigJsName, "'>");
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText(kScriptTag);
  rewrite_driver()->ParseText("   \t\n");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("   </script>\n");
  rewrite_driver()->FinishParse();
  const GoogleString expected =
      StringPrintf(kHtmlFormat, expected_rewritten_path_.c_str());
  EXPECT_EQ(expected, output_buffer_);
}

}  // namespace net_instaweb
