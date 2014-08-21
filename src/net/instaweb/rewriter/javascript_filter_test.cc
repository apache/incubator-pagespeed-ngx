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
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/debug_filter.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/support_noscript_filter.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/http/google_url.h"

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
const char kOrigJsNameRegexp[] = "*hello.js*";
const char kUnauthorizedJs[] = "http://other.domain.com/hello.js";
const char kRewrittenJsName[] = "hello.js";
const char kLibraryUrl[] = "https://www.example.com/hello/1.0/hello.js";
const char kIntrospectiveJS[] =
    "<script type='text/javascript' src='introspective.js'></script>";

const char kJsonData[] = "  {  'foo' :  [ 'bar' , 'baz' ]  }  ";
const char kJsonMinData[] = "{'foo':['bar','baz']}";
const char kOrigJsonName[] = "hello.json";
const char kRewrittenJsonName[] = "hello.json";

}  // namespace

namespace net_instaweb {

class JavascriptFilterTest : public RewriteTestBase,
                             public ::testing::WithParamInterface<bool> {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->set_use_experimental_js_minifier(GetParam());
    expected_rewritten_path_ = Encode("", kFilterId, "0",
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
    options()->EnableFilter(RewriteOptions::kRewriteJavascriptExternal);
    options()->EnableFilter(RewriteOptions::kRewriteJavascriptInline);
    options()->EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
    rewrite_driver_->AddFilters();
  }

  void InitTest(int64 ttl) {
    SetResponseWithDefaultHeaders(
        kOrigJsName, kContentTypeJavascript, kJsData, ttl);
    SetResponseWithDefaultHeaders(
        kUnauthorizedJs, kContentTypeJavascript, kJsData, ttl);
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
    EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, munged_url), &out));

    // Rewrite again; should still get normal URL
    ValidateExpected("no_ext_corruption",
                    GenerateHtml(kOrigJsName),
                    GenerateHtml(expected_rewritten_path_.c_str()));
  }

  void SourceMapTest(StringPiece input_js, StringPiece expected_output_js,
                     StringPiece expected_mapping_vlq);

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

TEST_P(JavascriptFilterTest, DoRewrite) {
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

TEST_P(JavascriptFilterTest, DontRewriteUnauthorizedDomain) {
  InitFiltersAndTest(100);
  ValidateNoChanges("dont_rewrite", GenerateHtml(kUnauthorizedJs));
}

TEST_P(JavascriptFilterTest, DebugForUnauthorizedDomain) {
  StringVector expected_disabled_filters;
  SupportNoscriptFilter tmp(rewrite_driver());
  expected_disabled_filters.push_back(tmp.Name());
  const char kCaseId[] = "debug_unauthorized_domain";
  const GoogleString html_input = GenerateHtml(kUnauthorizedJs);
  // Remove the trailing newline as it's in the way :-(
  GoogleString html_output = html_input.substr(0, html_input.length() - 1);
  GoogleUrl gurl(kUnauthorizedJs);
  StrAppend(&html_output,
            "<!--",
            RewriteDriver::GenerateUnauthorizedDomainDebugComment(gurl),
            "-->"
            "\n");
  html_output = AddHtmlBody(html_output);
  GoogleString end_document_message = DebugFilter::FormatEndDocumentMessage(
      0, 0, 0, 0, 0, false, StringSet(), expected_disabled_filters);
  options()->EnableFilter(RewriteOptions::kDebug);
  InitFiltersAndTest(100);
  Parse(kCaseId, html_input);
  EXPECT_HAS_SUBSTR(html_output, output_buffer_) << "Test id:" << kCaseId;
  EXPECT_HAS_SUBSTR(end_document_message, output_buffer_)
      << "Test id:" << kCaseId;
}

TEST_P(JavascriptFilterTest, DontRewriteUnauthorizedDomainWithUnauthOptionSet) {
  InitFiltersAndTest(100);
  options()->ClearSignatureForTesting();
  options()->AddInlineUnauthorizedResourceType(semantic_type::kScript);
  server_context()->ComputeSignature(options());
  ValidateNoChanges("dont_rewrite", GenerateHtml(kUnauthorizedJs));
}

TEST_P(JavascriptFilterTest, DontRewriteDisallowedScripts) {
  SetResponseWithDefaultHeaders(
      "a.js", kContentTypeJavascript, "document.write('a');", 100);
  options()->Disallow("*a.js*");
  options()->EnableFilter(RewriteOptions::kInlineJavascript);
  SetHtmlMimetype();
  InitFiltersAndTest(100);
  ValidateExpected("inline javascript disallowed",
                   StringPrintf(kHtmlFormat, "a.js"),
                   StringPrintf(kHtmlFormat, "a.js"));
}

TEST_P(JavascriptFilterTest, DoInlineAllowedForInliningScripts) {
  SetResponseWithDefaultHeaders(
      "a.js", kContentTypeJavascript, "document.write('a');", 100);
  options()->AllowOnlyWhenInlining("*a.js*");
  options()->EnableFilter(RewriteOptions::kInlineJavascript);
  SetHtmlMimetype();
  InitFiltersAndTest(100);
  ValidateExpected("inline javascript allowed for inlining",
                   StringPrintf(kHtmlFormat, "a.js"),
                   StringPrintf(kInlineJs, "document.write('a');"));
}

TEST_P(JavascriptFilterTest, RewriteButExceedLogThreshold) {
  InitFiltersAndTest(100);
  rewrite_driver_->log_record()->SetRewriterInfoMaxSize(0);
  ValidateExpected("do_rewrite",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
  EXPECT_STREQ("", AppliedRewriterStringFromLog());
}

TEST_P(JavascriptFilterTest, DoRewriteUnhealthy) {
  lru_cache()->set_is_healthy(false);

  InitFiltersAndTest(100);
  ValidateNoChanges("do_rewrite", GenerateHtml(kOrigJsName));
  EXPECT_STREQ("", AppliedRewriterStringFromLog());
}

TEST_P(JavascriptFilterTest, RewriteAlreadyCachedProperly) {
  InitFiltersAndTest(100000000);  // cached for a long time to begin with
  // But we will rewrite because we can make the data smaller.
  ValidateExpected("rewrite_despite_being_cached_properly",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));
}

TEST_P(JavascriptFilterTest, NoRewriteOriginUncacheable) {
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

TEST_P(JavascriptFilterTest, IdentifyLibrary) {
  RegisterLibrary();
  InitFiltersAndTest(100);
  ValidateExpected("identify_library",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kLibraryUrl));

  EXPECT_EQ(1, libraries_identified_->Get());
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
}

TEST_P(JavascriptFilterTest, IdentifyLibraryTwice) {
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

TEST_P(JavascriptFilterTest, JsPreserveURLsOnTest) {
  // Make sure that when in conservative mode the URL stays the same.
  RegisterLibrary();
  options()->SoftEnableFilterForTesting(
      RewriteOptions::kRewriteJavascriptExternal);
  options()->SoftEnableFilterForTesting(
      RewriteOptions::kCanonicalizeJavascriptLibraries);
  options()->set_js_preserve_urls(true);
  rewrite_driver()->AddFilters();
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kRewriteJavascriptExternal));
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

TEST_P(JavascriptFilterTest, JsPreserveOverridingExtend) {
  // Make sure that when in conservative mode the URL stays the same.
  RegisterLibrary();

  scoped_ptr<RewriteOptions> global_options(options()->NewOptions());
  global_options->EnableFilter(RewriteOptions::kExtendCacheCss);

  scoped_ptr<RewriteOptions> vhost_options(options()->NewOptions());
  vhost_options->SoftEnableFilterForTesting(
      RewriteOptions::kRewriteJavascriptExternal);
  vhost_options->SoftEnableFilterForTesting(
      RewriteOptions::kCanonicalizeJavascriptLibraries);
  vhost_options->set_js_preserve_urls(true);
  options()->Merge(*global_options);
  options()->Merge(*vhost_options);

  rewrite_driver()->AddFilters();
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kRewriteJavascriptExternal));
  InitTest(100);

  // Make sure the URL doesn't change.
  ValidateExpected("js_urls_preserved",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kOrigJsName));

  // We should have preemptively optimized the JS even though we didn't render
  // the URL.
  ClearStats();
  GoogleString out_js_url = Encode(kTestDomain, "jm", "0", kOrigJsName, "js");
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

TEST_P(JavascriptFilterTest, JsExtendOverridingPreserve) {
  // Make sure that when in conservative mode the URL stays the same.
  RegisterLibrary();

  scoped_ptr<RewriteOptions> global_options(options()->NewOptions());
  global_options->set_js_preserve_urls(true);
  global_options->EnableFilter(RewriteOptions::kRewriteJavascriptExternal);

  scoped_ptr<RewriteOptions> vhost_options(options()->NewOptions());
  vhost_options->EnableFilter(RewriteOptions::kExtendCacheScripts);
  options()->Merge(*global_options);
  options()->Merge(*vhost_options);
  rewrite_driver()->AddFilters();
  EXPECT_TRUE(options()->Enabled(RewriteOptions::kRewriteJavascriptExternal));
  InitTest(100);

  // Make sure the URL is updated.
  ValidateExpected("js_extend_overrides_preserve",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(
                       Encode("", "jm", "0", kRewrittenJsName, "js").c_str()));

  ClearStats();
  GoogleString out_js;
  GoogleString out_js_url = Encode(kTestDomain, "jm", "0", kRewrittenJsName,
                                   "js");
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

TEST_P(JavascriptFilterTest, JsPreserveURLsNoPreemptiveRewriteTest) {
  // Make sure that when in conservative mode the URL stays the same.
  RegisterLibrary();
  options()->SoftEnableFilterForTesting(
      RewriteOptions::kRewriteJavascriptExternal);
  options()->set_js_preserve_urls(true);
  options()->set_in_place_preemptive_rewrite_javascript(false);
  rewrite_driver()->AddFilters();
  EXPECT_TRUE(options()->Enabled(
      RewriteOptions::kRewriteJavascriptExternal));
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

TEST_P(JavascriptFilterTest, IdentifyLibraryNoMinification) {
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

TEST_P(JavascriptFilterTest, DisallowedUrlsNotCheckedForCanonicalization) {
  RegisterLibrary();
  options()->EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  options()->Disallow(kOrigJsNameRegexp);
  rewrite_driver_->AddFilters();
  InitTest(100);
  ValidateExpected("no_library_identification",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kOrigJsName));

  EXPECT_EQ(0, libraries_identified_->Get());
  EXPECT_EQ(0, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(0, total_bytes_saved_->Get());
}

TEST_P(JavascriptFilterTest, AllowWhenInliningUrlsStillNotChecked) {
  RegisterLibrary();
  options()->EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  options()->AllowOnlyWhenInlining(kOrigJsNameRegexp);
  rewrite_driver_->AddFilters();
  InitTest(100);
  ValidateExpected("no_library_identification",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kOrigJsName));

  EXPECT_EQ(0, libraries_identified_->Get());
  EXPECT_EQ(0, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(0, total_bytes_saved_->Get());
}

TEST_P(JavascriptFilterTest, IdentifyFailureNoMinification) {
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

TEST_P(JavascriptFilterTest, IgnoreLibraryNoIdentification) {
  RegisterLibrary();
  // We register the library but don't enable library redirection.
  options()->EnableFilter(RewriteOptions::kRewriteJavascriptExternal);
  rewrite_driver_->AddFilters();
  InitTest(100);
  ValidateExpected("ignore_library",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(expected_rewritten_path_.c_str()));

  EXPECT_EQ(0, libraries_identified_->Get());
  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
}

TEST_P(JavascriptFilterTest, DontCombineIdentified) {
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

TEST_P(JavascriptFilterTest, DontInlineIdentified) {
  // Don't inline a one-line library that was rewritten to a canonical url.
  RegisterLibrary();
  options()->EnableFilter(RewriteOptions::kInlineJavascript);
  InitFiltersAndTest(100);
  ValidateExpected("DontInlineIdentified",
                   GenerateHtml(kOrigJsName),
                   GenerateHtml(kLibraryUrl));
}

TEST_P(JavascriptFilterTest, ServeFiles) {
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
  ServeResourceFromManyContexts(StrCat(kTestDomain, expected_rewritten_path_),
                                kJsMinData);
}

TEST_P(JavascriptFilterTest, ServeFilesUnhealthy) {
  lru_cache()->set_is_healthy(false);

  InitFilters();
  InitTest(100);
  TestServeFiles(&kContentTypeJavascript, kFilterId, "js",
                 kOrigJsName, kJsData,
                 kRewrittenJsName, kJsMinData);
}

TEST_P(JavascriptFilterTest, ServeRewrittenLibrary) {
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

TEST_P(JavascriptFilterTest, ServeJsonFile) {
  InitFilters();
  // Set content type extension to "js" because filter is caching it with js
  // extension, but the in-place lookup will still cache and serve with original
  // content type. Tests for this in in_place_rewrite_context_test.
  TestServeFiles(&kContentTypeJson, kFilterId, "js",
                 kOrigJsonName, kJsonData,
                 kRewrittenJsonName, kJsonMinData);

  EXPECT_EQ(1, blocks_minified_->Get());
  EXPECT_EQ(0, minification_failures_->Get());
  EXPECT_EQ(STATIC_STRLEN(kJsonData) - STATIC_STRLEN(kJsonMinData),
            total_bytes_saved_->Get());
  EXPECT_EQ(STATIC_STRLEN(kJsonData), total_original_bytes_->Get());
  // Note: We do not count any uses, because we did not write the URL into
  // an HTML file, just served it on request.
  EXPECT_EQ(0, num_uses_->Get());
}

TEST_P(JavascriptFilterTest, IdentifyAjaxLibrary) {
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

TEST_P(JavascriptFilterTest, InvalidInputMimetype) {
  InitFilters();
  // Make sure we can rewrite properly even when input has corrupt mimetype.
  ContentType not_java_script = kContentTypeJavascript;
  not_java_script.mime_type_ = "text/semicolon-inserted";
  const char* kNotJsFile = "script.notjs";

  SetResponseWithDefaultHeaders(kNotJsFile, not_java_script, kJsData, 100);
  ValidateExpected("wrong_mime",
                   GenerateHtml(kNotJsFile),
                   GenerateHtml(Encode("", "jm", "0",
                                       kNotJsFile, "js").c_str()));
}

TEST_P(JavascriptFilterTest, RewriteJs404) {
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
TEST_P(JavascriptFilterTest, NoExtensionCorruption) {
  TestCorruptUrl(".js%22");
}

TEST_P(JavascriptFilterTest, NoQueryCorruption) {
  TestCorruptUrl(".js?query");
}

TEST_P(JavascriptFilterTest, NoWrongExtCorruption) {
  TestCorruptUrl(".html");
}

TEST_P(JavascriptFilterTest, InlineJavascript) {
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

TEST_P(JavascriptFilterTest, NoMinificationInlineJS) {
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

TEST_P(JavascriptFilterTest, StripInlineWhitespace) {
  // Make sure we strip inline whitespace when minifying external scripts.
  InitFiltersAndTest(100);
  ValidateExpected(
      "StripInlineWhitespace",
      StrCat("<script src='", kOrigJsName, "'>   \t\n   </script>"),
      StrCat("<script src='",
             Encode("", "jm", "0", kOrigJsName, "js"),
             "'></script>"));
}

TEST_P(JavascriptFilterTest, RetainInlineData) {
  // Test to make sure we keep inline data when minifying external scripts.
  InitFiltersAndTest(100);
  ValidateExpected("StripInlineWhitespace",
                   StrCat("<script src='", kOrigJsName, "'> data </script>"),
                   StrCat("<script src='",
                          Encode("", "jm", "0", kOrigJsName, "js"),
                          "'> data </script>"));
}

// Test minification of a simple inline script in markup with no
// mimetype, where the script is wrapped in a commented-out CDATA.
//
// Note that javascript_filter never adds CDATA.  It only removes it
// if it's sure the mimetype is HTML.
TEST_P(JavascriptFilterTest, CdataJavascriptNoMimetype) {
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
TEST_P(JavascriptFilterTest, CdataJavascriptHtmlMimetype) {
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
TEST_P(JavascriptFilterTest, CdataJavascriptXhtmlMimetype) {
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

TEST_P(JavascriptFilterTest, XHtmlInlineJavascript) {
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
TEST_P(JavascriptFilterTest, RetainExtraHeaders) {
  InitFilters();
  GoogleString url = StrCat(kTestDomain, kOrigJsName);
  SetResponseWithDefaultHeaders(url, kContentTypeJavascript, kJsData, 300);
  TestRetainExtraHeaders(kOrigJsName, "jm", "js");
}

// http://code.google.com/p/modpagespeed/issues/detail?id=327 -- we were
// previously busting regexps with backslashes in them.
TEST_P(JavascriptFilterTest, BackslashInRegexp) {
  InitFilters();
  GoogleString input = StringPrintf(kInlineJs, "/http:\\/\\/[^/]+\\//");
  ValidateNoChanges("backslash_in_regexp", input);
}

TEST_P(JavascriptFilterTest, WeirdSrcCrash) {
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
                          Encode("", "jm", "0", kUrl, "js"),
                          ">Content"));
  ValidateNoChanges("weird_tag", "<script<foo>");
}

TEST_P(JavascriptFilterTest, MinificationFailure) {
  InitFilters();
  SetResponseWithDefaultHeaders("foo.js", kContentTypeJavascript,
                                "/* truncated comment", 100);
  ValidateNoChanges("fail", "<script src=foo.js></script>");

  EXPECT_EQ(0, blocks_minified_->Get());
  EXPECT_EQ(1, minification_failures_->Get());
  EXPECT_EQ(0, num_uses_->Get());
  EXPECT_EQ(1, did_not_shrink_->Get());
}

TEST_P(JavascriptFilterTest, ReuseRewrite) {
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

TEST_P(JavascriptFilterTest, NoReuseInline) {
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
TEST_P(JavascriptFilterTest, ExtraCdataOnMalformedInput) {
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

TEST_P(JavascriptFilterTest, ValidCdata) {
  InitFiltersAndTest(100);

  const GoogleString kHtmlInput = StringPrintf(
      kInlineScriptFormat,
      StringPrintf(kCdataWrapper, "alert ( 'foo' ) ; \n").c_str());
  const GoogleString kHtmlOutput = StringPrintf(
      kInlineScriptFormat,
      StringPrintf(kCdataWrapper, "alert('foo');").c_str());
  ValidateExpected("valid_cdata", kHtmlInput, kHtmlOutput);
}

TEST_P(JavascriptFilterTest, FlushInInlineJS) {
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

TEST_P(JavascriptFilterTest, FlushInEndTag) {
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

TEST_P(JavascriptFilterTest, FlushAfterBeginScript) {
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

TEST_P(JavascriptFilterTest, StripInlineWhitespaceFlush) {
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

TEST_P(JavascriptFilterTest, Aris) {
  options()->EnableFilter(RewriteOptions::kDebug);
  InitFilters();

  const char introspective_js[] =
      "var script_tags = document.getElementsByTagName('script');";
  SetResponseWithDefaultHeaders("introspective.js", kContentTypeJavascript,
                                introspective_js, 100);

  Parse("introspective", GenerateHtml("introspective.js"));
  const GoogleString kInsertComment =
      StrCat(kIntrospectiveJS, "<!--",
             JavascriptCodeBlock::kIntrospectionComment, "-->");
  EXPECT_THAT(output_buffer_, ::testing::HasSubstr(kInsertComment));
}

TEST_P(JavascriptFilterTest, ArisSourceMaps) {
  options()->EnableFilter(RewriteOptions::kIncludeJsSourceMaps);
  options()->EnableFilter(RewriteOptions::kDebug);
  InitFilters();

  const char introspective_js[] =
      "var script_tags = document.getElementsByTagName('script');";
  SetResponseWithDefaultHeaders("introspective.js", kContentTypeJavascript,
                                introspective_js, 100);

  Parse("introspective", GenerateHtml("introspective.js"));
  const GoogleString kInsertComment =
      StrCat(kIntrospectiveJS, "<!--",
             JavascriptCodeBlock::kIntrospectionComment, "-->");
  EXPECT_THAT(output_buffer_, ::testing::HasSubstr(kInsertComment));
}

TEST_P(JavascriptFilterTest, ArisCombineJs) {
  options()->EnableFilter(RewriteOptions::kCombineJavascript);
  options()->EnableFilter(RewriteOptions::kDebug);
  InitFilters();

  const char introspective_js[] =
      "var script_tags = document.getElementsByTagName('script');";
  SetResponseWithDefaultHeaders("introspective.js", kContentTypeJavascript,
                                introspective_js, 100);
  SetResponseWithDefaultHeaders("a.js", kContentTypeJavascript,
                                kJsData, 100);
  SetResponseWithDefaultHeaders("b.js", kContentTypeJavascript,
                                kJsData, 100);

  static const char kHtmlBefore[] =
      "<script type='text/javascript' src='introspective.js'></script>\n"
      "<script type='text/javascript' src='a.js'></script>\n"
      "<script type='text/javascript' src='b.js'></script>\n";
  const GoogleString kHtmlAfter = StrCat(
      kIntrospectiveJS, "<!--", JavascriptCodeBlock::kIntrospectionComment,
      "-->\n", "<script src=\"a.js+b.js.pagespeed.jc.0.js\"></script>",
      "<script>eval(mod_pagespeed_0);</script>\n",
      "<script>eval(mod_pagespeed_0);</script>\n");
  Parse("introspective", kHtmlBefore);
  EXPECT_THAT(output_buffer_, ::testing::HasSubstr(kHtmlAfter));
}

void JavascriptFilterTest::SourceMapTest(StringPiece input_js,
                                         StringPiece expected_output_js,
                                         StringPiece expected_mapping_vlq) {
  UseMd5Hasher();
  options()->EnableFilter(RewriteOptions::kIncludeJsSourceMaps);
  InitFilters();

  SetResponseWithDefaultHeaders("input.js", kContentTypeJavascript,
                                input_js, 100);

  GoogleString expected_map = StrCat(
      ")]}'\n{\"mappings\":\"", expected_mapping_vlq, "\",\"names\":[],"
      "\"sources\":[\"http://test.com/input.js?PageSpeed=off\"],"
      "\"version\":3}\n");

  GoogleString source_map_url =
      Encode(kTestDomain, RewriteOptions::kJavascriptMinSourceMapId,
             hasher()->Hash(expected_map), "input.js", "map");

  GoogleString expected_output = expected_output_js.as_string();
  if (options()->use_experimental_js_minifier()) {
    StrAppend(&expected_output, "\n"
              "//# sourceMappingURL=", source_map_url, "\n");
  }

  const GoogleString rewritten_js_name =
      Encode("", RewriteOptions::kJavascriptMinId,
             hasher()->Hash(expected_output), "input.js", "js");
  ValidateExpected("source_maps",
                   GenerateHtml("input.js"),
                   GenerateHtml(rewritten_js_name.c_str()));


  GoogleString output_js;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, rewritten_js_name),
                               &output_js));
  EXPECT_EQ(expected_output, output_js);

  if (options()->use_experimental_js_minifier()) {
    GoogleString map;
    EXPECT_TRUE(FetchResourceUrl(source_map_url, &map));
    EXPECT_EQ(expected_map, map);

    // Test Resource flow without HTML flow.
    ServeResourceFromManyContexts(source_map_url, expected_map);

    // Test fetching Source Map with wrong/out-of-date hash.
    GoogleString different_hash_url =
        Encode(kTestDomain, RewriteOptions::kJavascriptMinSourceMapId,
               "Different", "input.js", "map");
    EXPECT_TRUE(FetchResourceUrl(different_hash_url, &map));
    // TODO(sligocki): Get this working. Currently we do the standard resource
    // reconstruction path, serving the same map even though the hash is diff.
    // EXPECT_FALSE(FetchResourceUrl(different_hash_url, &map));
    // EXPECT_EQ("", map);
  }
}

TEST_P(JavascriptFilterTest, SourceMapsSimple) {
  const char input_js[] = "  foo  bar  ";
  const char expected_output_js[] = "foo bar";
  const char vlq[] =
      // Comment format: (gen_line, gen_col, src_file, src_line, src_col) token
      "AAAE,"  // (0,  0,  0,  0,  2)  foo [space]
      "IAAK";  // (0, +4, +0, +0, +5)  bar
  SourceMapTest(input_js, expected_output_js, vlq);
}

TEST_P(JavascriptFilterTest, SourceMapsMedium) {
  const char input_js[] =
      "alert     (    'hello, world!'    ) \n"
      " /* removed */ <!-- removed --> \n"
      " // single-line-comment\n"
      "document.write( \"<!-- comment -->\" );";
  const char expected_output_js[] =
      "alert('hello, world!')\n"
      "document.write(\"<!-- comment -->\");";
  const char vlq[] =
      // Comment format: (gen_line, gen_col, src_file, src_line, src_col) token
      "AAAA,"    // (0,   0,  0,  0,   0)  alert
      "KAAU,"    // (0,  +5, +0, +0, +10)  (
      "CAAK,"    // (0,  +1, +0, +0,  +5)  'hello, world!'
      "eAAmB;"   // (0, +15, +0, +0, +19)  )
      "AAGlC,"   // (1,   0, +0, +3, -34)  document.write(
      "eAAgB,"   // (1, +15, +0, +0, +16)  "<!-- comment -->"
      "kBAAmB";  // (1, +18, +0, +0, +19)  );
  SourceMapTest(input_js, expected_output_js, vlq);
}

TEST_P(JavascriptFilterTest, NoSourceMapJsCombine) {
  options()->EnableFilter(RewriteOptions::kCombineJavascript);
  options()->EnableFilter(RewriteOptions::kIncludeJsSourceMaps);
  InitFilters();

  SetResponseWithDefaultHeaders("a.js", kContentTypeJavascript,
                                kJsData, 100);
  SetResponseWithDefaultHeaders("b.js", kContentTypeJavascript,
                                kJsData, 100);

  const char combined_name[] = "a.js+b.js.pagespeed.jc.0.js";

  static const char kHtmlBefore[] =
      "<script type='text/javascript' src='a.js'></script>\n"
      "<script type='text/javascript' src='b.js'></script>\n";
  GoogleString kHtmlAfter = StrCat(
      "<script src=\"", combined_name, "\"></script>"
      "<script>eval(mod_pagespeed_0);</script>\n"
      "<script>eval(mod_pagespeed_0);</script>\n");
  ValidateExpected("introspective", kHtmlBefore, kHtmlAfter);

  // Note: There is no //# ScriptSourceMap in combine output.
  GoogleString expected_output = StrCat(
      "var mod_pagespeed_0 = \"", kJsMinData, "\";\n"
      "var mod_pagespeed_0 = \"", kJsMinData, "\";\n");
  GoogleString output_js;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, combined_name), &output_js));
  EXPECT_EQ(expected_output, output_js);
}

TEST_P(JavascriptFilterTest, SourceMapUnsanitaryUrl) {
  if (!options()->use_experimental_js_minifier()) return;

  options()->EnableFilter(RewriteOptions::kIncludeJsSourceMaps);
  InitFilters();
  // Most servers will ignore unknown query params.
  mock_url_fetcher()->set_strip_query_params(true);

  SetResponseWithDefaultHeaders("input.js", kContentTypeJavascript,
                                kJsData, 100);

  GoogleString unsanitary_url =
      Encode(kTestDomain, RewriteOptions::kJavascriptMinId,
             "0", "input.js?evil=\n", "js");

  GoogleString output_js;
  EXPECT_TRUE(FetchResourceUrl(unsanitary_url, &output_js));
  // Note: The important thing is that there's no newline in the mapping URL.
  GoogleString expected_output_js = StrCat(
      kJsMinData, "\n//# sourceMappingURL="
      "http://test.com/input.js,qevil=.pagespeed.sm.0.map\n");
  EXPECT_EQ(expected_output_js, output_js);
}

TEST_P(JavascriptFilterTest, InlineAndNotExternal) {
  options()->EnableFilter(RewriteOptions::kRewriteJavascriptInline);
  options()->DisableFilter(RewriteOptions::kRewriteJavascriptExternal);
  rewrite_driver_->AddFilters();
  InitTest(100);
  ValidateExpected("inline_not_external",
                   StrCat(StringPrintf(kInlineJs, kJsData),
                          StringPrintf(kHtmlFormat, kOrigJsName)),
                   StrCat(StringPrintf(kInlineJs, kJsMinData),
                          StringPrintf(kHtmlFormat, kOrigJsName)));
}

TEST_P(JavascriptFilterTest, InlineAndNotExternalPreserve) {
  // js_preserve_urls should not affect minification of inline JS.
  options()->set_js_preserve_urls(true);
  options()->set_in_place_preemptive_rewrite_javascript(false);
  options()->EnableFilter(RewriteOptions::kRewriteJavascriptInline);
  options()->DisableFilter(RewriteOptions::kRewriteJavascriptExternal);
  rewrite_driver_->AddFilters();
  InitTest(100);
  ValidateExpected("inline_not_external",
                   StrCat(StringPrintf(kInlineJs, kJsData),
                          StringPrintf(kHtmlFormat, kOrigJsName)),
                   StrCat(StringPrintf(kInlineJs, kJsMinData),
                          StringPrintf(kHtmlFormat, kOrigJsName)));
}

TEST_P(JavascriptFilterTest, InlineAndCanonicalNotExternal) {
  options()->EnableFilter(RewriteOptions::kRewriteJavascriptInline);
  options()->EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  options()->DisableFilter(RewriteOptions::kRewriteJavascriptExternal);
  rewrite_driver_->AddFilters();
  InitTest(100);
  ValidateExpected("inline_and_canonical_and_not_external",
                   StrCat(StringPrintf(kInlineJs, kJsData),
                          StringPrintf(kHtmlFormat, kOrigJsName)),
                   StrCat(StringPrintf(kInlineJs, kJsMinData),
                          StringPrintf(kHtmlFormat, kOrigJsName)));
}

TEST_P(JavascriptFilterTest, ExternalAndNotInline) {
  options()->EnableFilter(RewriteOptions::kRewriteJavascriptExternal);
  options()->DisableFilter(RewriteOptions::kRewriteJavascriptInline);
  rewrite_driver_->AddFilters();
  InitTest(100);
  ValidateExpected("external_not_inline",
                   StrCat(StringPrintf(kInlineJs, kJsData),
                          StringPrintf(kHtmlFormat, kOrigJsName)),
                   StrCat(StringPrintf(kInlineJs, kJsData),
                          StringPrintf(kHtmlFormat,
                                       expected_rewritten_path_.c_str())));
}

// We test with use_experimental_minifier == GetParam() as both true and false.
INSTANTIATE_TEST_CASE_P(JavascriptFilterTestInstance, JavascriptFilterTest,
                        ::testing::Bool());

}  // namespace net_instaweb
