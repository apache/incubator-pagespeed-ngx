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

// Unit-test the cache extender.

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/css_outline_filter.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

const char kCssFormat[] = "<link rel='stylesheet' href='%s' type='text/css'>\n";
const char kHtmlFormat[] =
    "<link rel='stylesheet' href='%s' type='text/css'>%s\n"
    "<img src='%s'/>%s\n"
    "<script type='text/javascript' src='%s'></script>%s\n";

// See Issue 295: cache_extender, which only rewrites content on
// fetch, failed to recognize a cache-extended CSS file specified with
// a query-param as CSS.  It failed to recognize it because its
// file-extension was obscured by a query-param.  Moreover, we should
// not be dependent on input resource extensions to determine
// content-type.  Thus it did not run its absolutification pass.
//
// Instead we must ensure that the content-type is discovered from the
// input resource response headers.
const char kCssFile[]       = "sub/a.css?v=1";
const char kCssTail[]       = "a.css?v=1";
const char kCssSubdir[]     = "sub/";
const char kCssDataFormat[] = ".blue {color: blue; src: url(%sembedded.png);}";
const char kFilterId[]      = "ce";
const char kImageData[]     = "Not really JPEG but irrelevant for this test";
const char kJsData[]        = "alert('hello, world!')";
const char kJsDataIntrospective[] = "$('script')";
const char kNewDomain[]     = "http://new.com/";
const int kShortTtlSec      = 100;
const int kMediumTtlSec     = 100000;
const int kLongTtlSec       = 100000000;

class CacheExtenderTest : public RewriteTestBase {
 protected:
  enum InputOrOutput { kInput, kOutput, kBoth };

  CacheExtenderTest()
      : kCssData(CssData("")),
        kCssPath(StrCat(kTestDomain, kCssSubdir)) {
    num_cache_extended_ = statistics()->GetVariable(
        CacheExtender::kCacheExtensions);
  }

  // TODO(matterbury): Delete this method as it should be redundant.
  virtual void SetUp() {
    RewriteTestBase::SetUp();
  }

  void InitTest(int64 ttl) {
    options()->EnableExtendCacheFilters();
    InitTestWithoutFilters(ttl);
  }

  void InitTestWithoutFilters(int64 ttl) {
    rewrite_driver()->AddFilters();
    SetResponseWithDefaultHeaders(kCssFile, kContentTypeCss, kCssData, ttl);
    SetResponseWithDefaultHeaders("b.jpg", kContentTypeJpeg, kImageData, ttl);
    SetResponseWithDefaultHeaders("c.js", kContentTypeJavascript, kJsData, ttl);
    SetResponseWithDefaultHeaders("introspective.js", kContentTypeJavascript,
                                  kJsDataIntrospective, ttl);
    // Reset stats.
    num_cache_extended_->Clear();
  }

  // Generate HTML loading 3 resources with the specified URLs
  GoogleString GenerateHtml(const GoogleString& a,
                            const GoogleString& b,
                            const GoogleString& c,
                            InputOrOutput input_or_output) {
    GoogleString a_debug, b_debug, c_debug;
    if (input_or_output == kOutput) {
      // We never generate debug messages on input html!
      a_debug = DebugMessage(a);
      b_debug = DebugMessage(b);
      c_debug = DebugMessage(c);
    }
    return StringPrintf(kHtmlFormat,
                        a.c_str(), a_debug.c_str(),
                        b.c_str(), b_debug.c_str(),
                        c.c_str(), c_debug.c_str());
  }

  // Helper to test for how we handle trailing junk in URLs
  void TestCorruptUrl(StringPiece junk, bool append_junk) {
    InitTest(kShortTtlSec);
    GoogleString a_ext = Encode(kCssSubdir, "ce", "0", kCssTail, "css");
    GoogleString b_ext = Encode("", "ce", "0", "b.jpg", "jpg");
    GoogleString c_ext = Encode("", "ce", "0", "c.js", "js");

    ValidateExpected("no_ext_corrupt_fetched",
                     GenerateHtml(kCssFile, "b.jpg", "c.js", kInput),
                     GenerateHtml(a_ext, b_ext, c_ext, kOutput));
    GoogleString output;
    EXPECT_TRUE(FetchResourceUrl(
        ChangeSuffix(StrCat(kTestDomain, a_ext), append_junk, ".css", junk),
        &output));
    EXPECT_TRUE(FetchResourceUrl(
        ChangeSuffix(StrCat(kTestDomain, b_ext), append_junk, ".jpg", junk),
        &output));
    EXPECT_TRUE(FetchResourceUrl(
        ChangeSuffix(StrCat(kTestDomain, c_ext), append_junk, ".js", junk),
        &output));
    ValidateExpected("no_ext_corrupt_cached",
                     GenerateHtml(kCssFile, "b.jpg", "c.js", kInput),
                     GenerateHtml(a_ext, b_ext, c_ext, kOutput));
  }

  static GoogleString CssData(const StringPiece& url) {
    return StringPrintf(kCssDataFormat, url.as_string().c_str());
  }

  void TestExtendFromHtml() {
    InitTest(kShortTtlSec);
    for (int i = 0; i < 3; i++) {
      const GoogleString input_html =
          GenerateHtml(kCssFile, "b.jpg", "c.js", kInput);
      if (lru_cache()->IsHealthy()) {
        AbstractLogRecord* log_record =
            rewrite_driver()->request_context()->log_record();
        log_record->SetAllowLoggingUrls(true);
        ValidateExpected(
            "do_extend",
            input_html,
            GenerateHtml(Encode(kCssSubdir, "ce", "0", kCssTail, "css"),
                         Encode("", "ce", "0", "b.jpg", "jpg"),
                         Encode("", "ce", "0", "c.js", "js"), kOutput));
        EXPECT_EQ((i + 1) * 3, num_cache_extended_->Get())
            << "Number of cache extended resources is wrong";
        EXPECT_STREQ("ec,ei,es", AppliedRewriterStringFromLog());
        VerifyRewriterInfoEntry(log_record, "ec", 0, (i * 3), (i + 1) * 3,
                                3, "http://test.com/sub/a.css?v=1");
        VerifyRewriterInfoEntry(log_record, "ei", 1, 1 + (i * 3), (i + 1) * 3,
                                3, "http://test.com/b.jpg");
        VerifyRewriterInfoEntry(log_record, "es", 2, 2 + (i * 3), (i + 1) * 3,
                                3, "http://test.com/c.js");
      } else {
        ValidateNoChanges("unhealthy", input_html);
        EXPECT_EQ(0, num_cache_extended_->Get())
            << "Number of cache extended resources is wrong";
        EXPECT_STREQ("", AppliedRewriterStringFromLog());
      }
    }
  }

  void TestServeFiles() {
    GoogleString content;

    InitTest(kShortTtlSec);
    // To ensure there's no absolutification (below) of embedded.png's URL in
    // the served CSS file, we have to serve it from test.com and not from
    // cdn.com which TestUrlNamer does when it's being used.
    ASSERT_TRUE(FetchResourceUrl(
        EncodeNormal(kCssPath, kFilterId, "0", kCssTail, "css"), &content));
    EXPECT_EQ(kCssData, content);  // no absolutification
    ASSERT_TRUE(FetchResource(kTestDomain, kFilterId, "b.jpg", "jpg",
                              &content));
    EXPECT_EQ(GoogleString(kImageData), content);
    ASSERT_TRUE(FetchResource(kTestDomain, kFilterId, "c.js", "js", &content));
    EXPECT_EQ(GoogleString(kJsData), content);
  }

  void VerifyUnauthorizedResourcesNotExtended() {
    EnableDebug();
    SetResponseWithDefaultHeaders("http://unauth.example.com/unauth.js",
                                  kContentTypeJavascript, kJsData,
                                  kShortTtlSec);
    SetResponseWithDefaultHeaders("http://unauth.example.com/unauth.css",
                                  kContentTypeCss, kCssData, kShortTtlSec);
    const char kJsReference[] =
        "<script src='http://unauth.example.com/unauth.js'></script>";
    GoogleUrl gurl("http://unauth.example.com/unauth.xxx");
    const GoogleString kDebugMessage = StrCat(
        "<!--", RewriteDriver::GenerateUnauthorizedDomainDebugComment(gurl),
        "-->");
    const char kCssReference[] =
        "<link rel=stylesheet href='http://unauth.example.com/unauth.css'>";
    ValidateExpected("dont_extend_unauth_js",
                     StrCat(kJsReference, kCssReference),
                     StrCat(kJsReference, kDebugMessage,
                            kCssReference, kDebugMessage));
    EXPECT_EQ(0, num_cache_extended_->Get())
        << "Number of cache extended resources is wrong";
    EXPECT_STREQ("", AppliedRewriterStringFromLog());
  }

  Variable* num_cache_extended_;
  const GoogleString kCssData;
  const GoogleString kCssPath;
};

TEST_F(CacheExtenderTest, DoExtend) {
  TestExtendFromHtml();
  EXPECT_EQ(6, lru_cache()->num_hits()) << "3 metadata * 2 cached iterations";
  EXPECT_EQ(6, lru_cache()->num_misses()) << "3 metadata + 3 input resources";
  EXPECT_EQ(6, lru_cache()->num_inserts()) << "3 metadata + 3 input resources";
  EXPECT_EQ(3, counting_url_async_fetcher()->fetch_count());
}

TEST_F(CacheExtenderTest, ExtendUnhealthy) {
  lru_cache()->set_is_healthy(false);
  TestExtendFromHtml();
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

class CacheExtenderTestPreserveURLs : public CacheExtenderTest {
 public:
  virtual void SetUp() {}

  // This function should only be called once, as it sets the filters
  // and options.
  void TestExtend(bool img_extend, bool css_extend, bool js_extend) {
    options()->SoftEnableFilterForTesting(RewriteOptions::kExtendCacheCss);
    options()->SoftEnableFilterForTesting(RewriteOptions::kExtendCacheImages);
    options()->SoftEnableFilterForTesting(RewriteOptions::kExtendCacheScripts);
    if (!img_extend) {
      options()->set_image_preserve_urls(true);
    }
    if (!css_extend) {
      options()->set_css_preserve_urls(true);
    }
    if (!js_extend) {
      options()->set_js_preserve_urls(true);
    }
    CacheExtenderTest::SetUp();
    InitTestWithoutFilters(kShortTtlSec);

    GoogleString expected_img_html = "b.jpg";
    GoogleString expected_css_html = kCssFile;
    GoogleString expected_js_html = "c.js";

    if (img_extend) {
      expected_img_html = Encode("", "ce", "0", "b.jpg", "jpg");
    }
    if (css_extend) {
      expected_css_html = Encode(kCssSubdir, "ce", "0", kCssTail, "css");
    }
    if (js_extend) {
      expected_js_html = Encode("", "ce", "0", "c.js", "js");
    }
    ValidateExpected(
        "do_extend",
        GenerateHtml(kCssFile, "b.jpg", "c.js", kInput),
        GenerateHtml(
            expected_css_html, expected_img_html, expected_js_html, kOutput));
  }
};

TEST_F(CacheExtenderTestPreserveURLs, CacheExtenderPreserveImageURLsOn) {
  TestExtend(false,  // img_extend
             true,   // css_extend
             true);  // js_extend
}

TEST_F(CacheExtenderTestPreserveURLs, CacheExtenderPreserveCssURLsOn) {
  TestExtend(true,   // img_extend
             false,  // css_extend
             true);  // js_extend
}

TEST_F(CacheExtenderTestPreserveURLs, CacheExtenderPreserveJsURLsOn) {
  TestExtend(true,    // img_extend
             true,    // css_extend
             false);  // js_extend
}

TEST_F(CacheExtenderTestPreserveURLs, CacheExtenderPreserveAllURLsOn) {
  TestExtend(false,   // img_extend
             false,   // css_extend
             false);  // js_extend
}

TEST_F(CacheExtenderTest, DoNotExtendUnauthorizedResources) {
  InitTest(kShortTtlSec);
  VerifyUnauthorizedResourcesNotExtended();
}

TEST_F(CacheExtenderTest, DoNotExtendUnauthorizedResourcesWithUnauthEnabled) {
  InitTest(kShortTtlSec);
  options()->ClearSignatureForTesting();
  options()->AddInlineUnauthorizedResourceType(semantic_type::kStylesheet);
  options()->AddInlineUnauthorizedResourceType(semantic_type::kScript);
  server_context()->ComputeSignature(options());
  VerifyUnauthorizedResourcesNotExtended();
}

TEST_F(CacheExtenderTest,
       DontExtendIntrospectiveDefault) {
  InitTest(kShortTtlSec);
  const char kJsTemplate[] = "<script src=\"%s\"></script>";
  ValidateExpected(
      "dont_extend_introspective_js",
      StringPrintf(kJsTemplate, "introspective.js"),
      StringPrintf(kJsTemplate, "introspective.js"));
  EXPECT_EQ(0, num_cache_extended_->Get())
      << "Number of cache extended resources is wrong";
  EXPECT_STREQ("", AppliedRewriterStringFromLog());
}

TEST_F(CacheExtenderTest,
       DontExtendIntrospectiveDebug) {
  options()->EnableFilter(RewriteOptions::kDebug);
  InitTest(kShortTtlSec);
  const char kJsTemplate[] = "<script src=\"%s\"></script>";
  GoogleString kInsertComment(
      StrCat(StringPrintf(kJsTemplate, "introspective.js"), "<!--",
                          JavascriptCodeBlock::kIntrospectionComment, "-->"));
  Parse("dont_extend_introspective_js",
        StringPrintf(kJsTemplate, "introspective.js"));
  EXPECT_THAT(output_buffer_, ::testing::HasSubstr(kInsertComment));
  EXPECT_EQ(0, num_cache_extended_->Get())
      << "Number of cache extended resources is wrong";
  EXPECT_STREQ("", AppliedRewriterStringFromLog());
}

TEST_F(CacheExtenderTest, DoExtendIntrospectiveJavascript) {
  options()->ClearSignatureForTesting();
  options()->set_avoid_renaming_introspective_javascript(false);
  InitTest(kShortTtlSec);
  const char kJsTemplate[] = "<script src=\"%s\"></script>";
  ValidateExpected(
      "do_extend_introspective_js",
      StringPrintf(kJsTemplate, "introspective.js"),
      StringPrintf(kJsTemplate,
                   Encode("", "ce", "0", "introspective.js", "js").c_str()));
}

TEST_F(CacheExtenderTest, DoExtendLinkRelCaseInsensitive) {
  InitTest(kShortTtlSec);
  const char kMixedCaseTemplate[] = "<link rel=StyleSheet href='%s'>";
  ValidateExpected(
      "extend_ci",
      StringPrintf(kMixedCaseTemplate, kCssFile),
      StringPrintf(kMixedCaseTemplate,
                   Encode(kCssSubdir, "ce", "0", kCssTail, "css").c_str()));
}

TEST_F(CacheExtenderTest, DoExtendForImagesOnly) {
  AddFilter(RewriteOptions::kExtendCacheImages);
  SetResponseWithDefaultHeaders(kCssFile, kContentTypeCss,
                                kCssData, kShortTtlSec);
  SetResponseWithDefaultHeaders("b.jpg", kContentTypeJpeg,
                                kImageData, kShortTtlSec);
  SetResponseWithDefaultHeaders("c.js", kContentTypeJavascript,
                                kJsData, kShortTtlSec);

  for (int i = 0; i < 3; i++) {
    ValidateExpected(
        "do_extend",
        GenerateHtml(kCssFile, "b.jpg", "c.js", kInput),
        GenerateHtml(
            kCssFile, Encode("", "ce", "0", "b.jpg", "jpg"), "c.js", kOutput));
    EXPECT_EQ((i + 1), num_cache_extended_->Get())
        << "Number of cache extended resources is wrong";
    EXPECT_STREQ("ei", AppliedRewriterStringFromLog());
  }
}

TEST_F(CacheExtenderTest, Handle404) {
  // Test to make sure that a missing input is handled well.
  options()->EnableExtendCacheFilters();
  rewrite_driver()->AddFilters();
  DebugWithMessage("<!--4xx status code, preventing rewriting of %url%-->");
  SetFetchResponse404("404.css");
  for (int i = 0; i < 2; ++i) {
    // Validate twice to make sure caching doesn't break it.
    const char kLink[] = "<link rel=stylesheet href='404.css'>";
    ValidateExpected("404", kLink, StrCat(kLink, DebugMessage("404.css")));
  }
}

TEST_F(CacheExtenderTest, Handle503) {
  // Test to make sure that a missing input is handled well.
  options()->EnableExtendCacheFilters();
  rewrite_driver()->AddFilters();
  DebugWithMessage("<!--Fetch failure, preventing rewriting of %url%-->");
  GoogleString url = AbsolutifyUrl("503.css");
  ResponseHeaders response_headers;
  response_headers.SetStatusAndReason(HttpStatus::kUnavailable);
  SetFetchResponse(url, response_headers, StringPiece());
  for (int i = 0; i < 2; ++i) {
    // Validate twice to make sure caching doesn't break it.
    const char kLink[] = "<link rel=stylesheet href='503.css'>";
    ValidateExpected("503", kLink, StrCat(kLink, DebugMessage(url)));
  }
}

TEST_F(CacheExtenderTest, UrlTooLong) {
  options()->EnableExtendCacheFilters();
  rewrite_driver()->AddFilters();

  // Make the filename too long.
  GoogleString long_string(options()->max_url_segment_size() + 1, 'z');

  GoogleString css_name = StrCat("style.css?z=", long_string);
  GoogleString jpg_name = StrCat("image.jpg?z=", long_string);
  GoogleString js_name  = StrCat("script.js?z=", long_string);
  SetResponseWithDefaultHeaders(css_name, kContentTypeCss,
                                kCssData, kShortTtlSec);
  SetResponseWithDefaultHeaders(jpg_name, kContentTypeJpeg,
                                kImageData, kShortTtlSec);
  SetResponseWithDefaultHeaders(js_name, kContentTypeJavascript,
                                kJsData, kShortTtlSec);

  // If filename wasn't too long, this would be rewritten (like in DoExtend).
  ValidateNoChanges(
      "url_too_long", GenerateHtml(css_name, jpg_name, js_name, kBoth));
  EXPECT_EQ(0, num_cache_extended_->Get())
      << "Number of cache extended resources is wrong";
}

TEST_F(CacheExtenderTest, NoInputResource) {
  InitTest(kShortTtlSec);
  // Test for not crashing on bad/disallowed URL.
  ValidateNoChanges("bad url",
                    GenerateHtml("swly://example.com/sub/a.css",
                                 "http://evil.com/b.jpg",
                                 "http://moreevil.com/c.js", kBoth));
}

TEST_F(CacheExtenderTest, NoExtendAlreadyCachedProperly) {
  InitTest(kLongTtlSec);  // cached for a long time to begin with
  ValidateNoChanges("no_extend_cached_properly",
                    GenerateHtml(kCssFile, "b.jpg", "c.js", kBoth));
  EXPECT_EQ(0, num_cache_extended_->Get())
      << "Number of cache extended resources is wrong";
}

TEST_F(CacheExtenderTest, ExtendIfSharded) {
  InitTest(kLongTtlSec);  // cached for a long time to begin with
  EXPECT_TRUE(AddShard(kTestDomain, "shard0.com,shard1.com"));
  // shard0 is always selected in the test because of our mock hasher
  // that always returns 0.
  ValidateExpected(
      "extend_if_sharded",
      GenerateHtml(kCssFile, "b.jpg", "c.js", kOutput),
      GenerateHtml(
          Encode(StrCat("http://shard0.com/", kCssSubdir),
                 "ce", "0", kCssTail, "css"),
          Encode("http://shard0.com/", "ce", "0", "b.jpg", "jpg"),
          Encode("http://shard0.com/", "ce", "0", "c.js", "js"), kOutput));
}

TEST_F(CacheExtenderTest, ExtendIfOriginMappedHttps) {
  InitTest(kShortTtlSec);
  EXPECT_TRUE(AddOriginDomainMapping(kTestDomain, "https://cdn.com"));
  ValidateExpected(
      "extend_if_origin_mapped_https",
      GenerateHtml("https://cdn.com/sub/a.css?v=1",
                   "https://cdn.com/b.jpg",
                   "https://cdn.com/c.js", kInput),
      GenerateHtml(
          Encode("https://cdn.com/sub/", "ce", "0", kCssTail, "css"),
          Encode("https://cdn.com/", "ce", "0", "b.jpg", "jpg"),
          Encode("https://cdn.com/", "ce", "0", "c.js", "js"), kOutput));
}

TEST_F(CacheExtenderTest, ExtendIfRewritten) {
  InitTest(kLongTtlSec);  // cached for a long time to begin with

  EXPECT_TRUE(AddRewriteDomainMapping("cdn.com", kTestDomain));
  ValidateExpected(
      "extend_if_rewritten",
      GenerateHtml(kCssFile, "b.jpg", "c.js", kInput),
      GenerateHtml(
          Encode("http://cdn.com/sub/", "ce", "0", kCssTail, "css"),
          Encode("http://cdn.com/", "ce", "0", "b.jpg", "jpg"),
          Encode("http://cdn.com/", "ce", "0", "c.js", "js"), kOutput));
  EXPECT_EQ(3, num_cache_extended_->Get())
      << "Number of cache extended resources is wrong";
  EXPECT_STREQ("ec,ei,es", AppliedRewriterStringFromLog());
}

TEST_F(CacheExtenderTest, ExtendIfShardedAndRewritten) {
  InitTest(kLongTtlSec);  // cached for a long time to begin with

  EXPECT_TRUE(AddRewriteDomainMapping("cdn.com", kTestDomain));

  // Domain-rewriting is performed first.  Then we shard.
  EXPECT_TRUE(AddShard("cdn.com", "shard0.com,shard1.com"));
  // shard0 is always selected in the test because of our mock hasher
  // that always returns 0.
  ValidateExpected(
      "extend_if_sharded_and_rewritten",
      GenerateHtml(kCssFile, "b.jpg", "c.js", kInput),
      GenerateHtml(
          Encode("http://shard0.com/sub/", "ce", "0", kCssTail, "css"),
          Encode("http://shard0.com/", "ce", "0", "b.jpg", "jpg"),
          Encode("http://shard0.com/", "ce", "0", "c.js", "js"), kOutput));
}

TEST_F(CacheExtenderTest, ExtendIfShardedToHttps) {
  InitTest(kLongTtlSec);

  // This Origin Mapping ensures any fetches are converted to http so work.
  EXPECT_TRUE(AddOriginDomainMapping(kTestDomain, "https://test.com"));

  EXPECT_TRUE(AddShard("https://test.com",
                       "https://shard0.com,https://shard1.com"));
  // shard0 is always selected in the test because of our mock hasher
  // that always returns 0.
  ValidateExpected(
      "extend_if_sharded_to_https",
      GenerateHtml("https://test.com/sub/a.css?v=1",
                   "https://test.com/b.jpg",
                   "https://test.com/c.js", kInput),
      GenerateHtml(
          Encode("https://shard0.com/sub/", "ce", "0", kCssTail, "css"),
          Encode("https://shard0.com/", "ce", "0", "b.jpg", "jpg"),
          Encode("https://shard0.com/", "ce", "0", "c.js", "js"), kOutput));
}

TEST_F(CacheExtenderTest, ExtendIfShardedAndRewritingAndMappingHttps) {
  // This test started out trying to unit test mod_pagespeed issue #400 by
  // replicating the settings the poster used. They didn't work, basically
  // because the wildcard directive for *test.com conflicted with the later
  // non-wildcard ones. After much experimentation we came up with these
  // settings (without wildcards) that seem to do what the poster wants.
  InitTest(kLongTtlSec);
  SetResponseWithDefaultHeaders(StrCat("http://www.test.com/", kCssFile),
                                kContentTypeCss, kCssData, kLongTtlSec);
  SetResponseWithDefaultHeaders("http://www.test.com/b.jpg", kContentTypeJpeg,
                                kImageData, kLongTtlSec);
  SetResponseWithDefaultHeaders("http://www.test.com/c.js",
                                kContentTypeJavascript, kJsData, kLongTtlSec);

  // Set up the mappings that -should- work for issue 400.
  ASSERT_TRUE(AddRewriteDomainMapping("http://cdn.com",
                                      "http://test.com,http://www.test.com"));
  ASSERT_TRUE(AddRewriteDomainMapping("https://cdn.com",
                                      "https://test.com,https://www.test.com"));
  ASSERT_TRUE(AddShard("http://cdn.com",
                       "http://s1.cdn.com,http://s2.cdn.com"));
  ASSERT_TRUE(AddShard("https://cdn.com",
                       "https://s1.cdn.com,https://s2.cdn.com"));
  ASSERT_TRUE(AddOriginDomainMapping("http://test.com", "https://test.com"));
  ASSERT_TRUE(AddOriginDomainMapping("http://test.com",
                                     "https://www.test.com"));

  // shard0 is always selected in the test because of our mock hasher
  // that always returns 0.
  ValidateExpected(
      "extend_if_sharded_rewriting_mapping_bare_domain_http",
      GenerateHtml("http://test.com/sub/a.css?v=1",
                   "http://test.com/b.jpg",
                   "http://test.com/c.js", kInput),
      GenerateHtml(
          Encode("http://s1.cdn.com/sub/", "ce", "0", kCssTail, "css"),
          Encode("http://s1.cdn.com/", "ce", "0", "b.jpg", "jpg"),
          Encode("http://s1.cdn.com/", "ce", "0", "c.js", "js"), kOutput));
  ValidateExpected(
      "extend_if_sharded_rewriting_mapping_bare_domain_https",
      GenerateHtml("https://test.com/sub/a.css?v=1",
                   "https://test.com/b.jpg",
                   "https://test.com/c.js", kInput),
      GenerateHtml(
          Encode("https://s1.cdn.com/sub/", "ce", "0", kCssTail, "css"),
          Encode("https://s1.cdn.com/", "ce", "0", "b.jpg", "jpg"),
          Encode("https://s1.cdn.com/", "ce", "0", "c.js", "js"), kOutput));
  ValidateExpected(
      "extend_if_sharded_rewriting_mapping_www_domain_http",
      GenerateHtml("http://www.test.com/sub/a.css?v=1",
                   "http://www.test.com/b.jpg",
                   "http://www.test.com/c.js", kInput),
      GenerateHtml(
          Encode("http://s1.cdn.com/sub/", "ce", "0", kCssTail, "css"),
          Encode("http://s1.cdn.com/", "ce", "0", "b.jpg", "jpg"),
          Encode("http://s1.cdn.com/", "ce", "0", "c.js", "js"), kOutput));
  ValidateExpected(
      "extend_if_sharded_rewriting_mapping_www_domain_https",
      GenerateHtml("https://www.test.com/sub/a.css?v=1",
                   "https://www.test.com/b.jpg",
                   "https://www.test.com/c.js", kInput),
      GenerateHtml(
          Encode("https://s1.cdn.com/sub/", "ce", "0", kCssTail, "css"),
          Encode("https://s1.cdn.com/", "ce", "0", "b.jpg", "jpg"),
          Encode("https://s1.cdn.com/", "ce", "0", "c.js", "js"), kOutput));
}

// TODO(jmarantz): consider implementing and testing the sharding and
// domain-rewriting of uncacheable resources -- just don't sign the URLs.

TEST_F(CacheExtenderTest, NoExtendOriginUncacheable) {
  InitTest(0);  // origin not cacheable
  DebugWithMessage("<!--Uncacheable content, preventing rewriting of %url%-->");
  ValidateExpected(
      "no_extend_origin_not_cacheable",
      GenerateHtml(kCssFile, "b.jpg", "c.js", kInput),
      GenerateHtml(kCssFile, "b.jpg", "c.js", kOutput));
  EXPECT_EQ(0, num_cache_extended_->Get())
      << "Number of cache extended resources is wrong";
}

TEST_F(CacheExtenderTest, ServeFiles) {
  TestServeFiles();
}

TEST_F(CacheExtenderTest, ServeFilesUnhealthy) {
  lru_cache()->set_is_healthy(false);
  TestServeFiles();
}

TEST_F(CacheExtenderTest, ConsistentHashWithRewrite) {
  // Since CacheExtend is an on-the-fly filter, ServeFilesWithRewrite, above,
  // verifies that we can decode a cache-extended CSS file and properly
  // domain-rewrite embedded images.  However, we go through the exercise
  // of generating the rewritten content in the HTML path too -- we just
  // don't cache it.  However, what we must do is generate the correct hash
  // code.  To test that we need to use the real hasher.
  UseMd5Hasher();
  AddRewriteDomainMapping(kNewDomain, kTestDomain);
  InitTest(kShortTtlSec);

  // First do the HTML rewrite.
  GoogleString hash = hasher()->Hash(kCssData);
  GoogleString extended_css =
      Encode(StrCat(kNewDomain, kCssSubdir), "ce", hash, kCssTail, "css");
  ValidateExpected("consistent_hash",
                   StringPrintf(kCssFormat, kCssFile),
                   StringPrintf(kCssFormat, extended_css.c_str()));

  // Note that the only output that gets cached is the MetaData insert, not
  // the rewritten content, because this is an on-the-fly filter and we
  // elect not to add cache pressure. We do of course also cache the original,
  // and under traditional flow also get it from the cache.
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_hits());

  // TODO(jmarantz): To make this test pass we need to set up the mock
  // fetcher so it can find the resource in new.com, not just
  // test.com.  Functionally, this wouldn't be needed with a
  // functional installation where both test.com and new.com are the
  // same physical server.  However it does indicate that we are going
  // to fetch the resource using its original resolved name while
  // rewriting HTML, but then when we serve the cache-extended
  // resource we will not have it in our cache; we will have to fetch
  // it again using the new name.  We ought to be canonicalizing the
  // URLs we write into the cache so we don't need this.  This also
  // applies to sharding.
  SetResponseWithDefaultHeaders(StrCat(kNewDomain, kCssFile), kContentTypeCss,
                                kCssData, kShortTtlSec);

  // Now serve the resource, as in ServeFilesWithRewrite above.
  GoogleString content;
  ASSERT_TRUE(FetchResourceUrl(extended_css, &content));
  EXPECT_EQ(kCssData, content);
}

TEST_F(CacheExtenderTest, ConsistentHashWithShard) {
  // Similar to ConsistentHashWithRewrite, except that we've added sharding,
  // and the shard computed for the embedded image is (luckily for the test)
  // different than that for the .css file, thus the references within the
  // css file are rewritten as absolute.
  UseMd5Hasher();
  AddRewriteDomainMapping(kNewDomain, kTestDomain);
  AddShard(kNewDomain, "shard1.com,shard2.com");
  InitTest(kShortTtlSec);

  // First do the HTML rewrite.
  const char kHash[] = "MnXHB3ChUY";
  GoogleString extended_css = Encode("http://shard2.com/sub/", "ce", kHash,
                                     kCssTail, "css");
  ValidateExpected("consistent_hash",
                   StringPrintf(kCssFormat, kCssFile),
                   StringPrintf(kCssFormat, extended_css.c_str()));

  // Note that the only output that gets cached is the MetaData insert, not
  // the rewritten content, because this is an on-the-fly filter and we
  // elect not to add cache pressure. We do of course also cache the original,
  // and under traditional flow also get it from the cache.
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_hits());

  // TODO(jmarantz): eliminate this when we canonicalize URLs before caching.
  SetResponseWithDefaultHeaders(StrCat("http://shard2.com/", kCssFile),
                                kContentTypeCss, kCssData, kShortTtlSec);

  // Now serve the resource, as in ServeFilesWithRewrite above.
  GoogleString content;
  ASSERT_TRUE(FetchResourceUrl(extended_css, &content));

  // Note that, through the luck of hashes, we've sharded the embedded
  // image differently than the css file.
  EXPECT_EQ(CssData("http://shard1.com/sub/"), content);
  EXPECT_EQ(kHash, hasher()->Hash(content));
}

TEST_F(CacheExtenderTest, ServeFilesWithRewriteDomainsEnabled) {
  GoogleString content;
  AddRewriteDomainMapping(kNewDomain, kTestDomain);
  InitTest(kShortTtlSec);
  ASSERT_TRUE(FetchResource(kCssPath, kFilterId, kCssTail, "css", &content));
  EXPECT_EQ(CssData("http://new.com/sub/"), content);
}

TEST_F(CacheExtenderTest, ServeFilesWithRewriteDomainAndPathEnabled) {
  GoogleString content;
  AddRewriteDomainMapping("http://new.com/test/", kTestDomain);
  InitTest(kShortTtlSec);
  ASSERT_TRUE(FetchResource(kCssPath, kFilterId, kCssTail, "css", &content));
  EXPECT_EQ(CssData("http://new.com/test/sub/"), content);
}

TEST_F(CacheExtenderTest, ServeFilesWithShard) {
  GoogleString content;
  AddRewriteDomainMapping(kNewDomain, kTestDomain);
  AddShard(kNewDomain, "shard1.com,shard2.com");
  InitTest(kShortTtlSec);
  ASSERT_TRUE(FetchResource(kCssPath, kFilterId, kCssTail, "css", &content));
  EXPECT_EQ(CssData("http://shard1.com/sub/"), content);
}

TEST_F(CacheExtenderTest, ServeFilesFromDelayedFetch) {
  InitTest(kShortTtlSec);
  // To ensure there's no absolutification (below) of embedded.png's URL in
  // the served CSS file, we have to serve it from test.com and not from
  // cdn.com which TestUrlNamer does when it's being used.
  ServeResourceFromManyContexts(
      EncodeNormal(kCssPath, "ce", "0", kCssTail, "css"), kCssData);
  ServeResourceFromManyContexts(Encode(kTestDomain, "ce", "0", "b.jpg", "jpg"),
                                kImageData);
  ServeResourceFromManyContexts(Encode(kTestDomain, "ce", "0", "c.js", "js"),
                                kJsData);

  // TODO(jmarantz): make ServeResourceFromManyContexts check:
  //  1. Gets the data from the cache, with no mock fetchers, null file system
  //  2. Gets the data from the file system, with no cache, no mock fetchers.
  //  3. Gets the data from the mock fetchers: no cache, no file system.
}

TEST_F(CacheExtenderTest, MinimizeCacheHits) {
  options()->EnableFilter(RewriteOptions::kOutlineCss);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  options()->set_css_outline_min_bytes(1);
  rewrite_driver()->AddFilters();
  GoogleString html_input = StrCat("<style>", kCssData, "</style>");
  GoogleString html_output = StringPrintf(
      "<link rel=\"stylesheet\" href=\"%s\">",
      Encode(kTestDomain, CssOutlineFilter::kFilterId, "0", "_",
             "css").c_str());
  ValidateExpected("no_extend_origin_not_cacheable", html_input, html_output);

  // The key thing about this test is that the CacheExtendFilter should
  // not pound the cache looking to see if it's already rewritten this
  // resource.  If we try, in the cache extend filter, to this already-optimized
  // resource from the cache, then we'll get a cache-hit and decide that
  // it's already got a long cache lifetime.  But we should know, just from
  // the name of the resource, that it should not be cache extended.
  // The CSS outliner also should not produce any cache misses, as it currently
  // does not cache.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
}

TEST_F(CacheExtenderTest, NoExtensionCorruption) {
  TestCorruptUrl("%22", true /* append %22 */);
}

TEST_F(CacheExtenderTest, NoQueryCorruption) {
  TestCorruptUrl("?query", true /* append ?query*/);
}

TEST_F(CacheExtenderTest, NoWrongExtCorruption) {
  TestCorruptUrl(".html", false /* replace ext with .html */);
}

TEST_F(CacheExtenderTest, MadeOnTheFly) {
  // Make sure our fetches go through on-the-fly construction and not the cache.
  InitTest(kMediumTtlSec);

  GoogleString b_ext = Encode("", "ce", "0", "b.jpg", "jpg");
  ValidateExpected("and_img", "<img src=\"b.jpg\">",
                   StrCat("<img src=\"", b_ext, "\">"));

  RewriteStats* stats = server_context()->rewrite_stats();
  EXPECT_EQ(0, stats->cached_resource_fetches()->Get());
  EXPECT_EQ(0, stats->succeeded_filter_resource_fetches()->Get());
  GoogleString out;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, b_ext), &out));
  EXPECT_EQ(0, stats->cached_resource_fetches()->Get());
  EXPECT_EQ(1, stats->succeeded_filter_resource_fetches()->Get());
}

// http://code.google.com/p/modpagespeed/issues/detail?id=324
TEST_F(CacheExtenderTest, RetainExtraHeaders) {
  GoogleString url = StrCat(kTestDomain, "retain.css");
  SetResponseWithDefaultHeaders(url, kContentTypeCss, kCssData, 300);
  // We must explicitly call ComputeSignature here because we are not
  // calling InitTest in this test.
  server_context()->ComputeSignature(options());
  TestRetainExtraHeaders("retain.css", "ce", "css");
}

TEST_F(CacheExtenderTest, TrimUrlInteraction) {
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  InitTest(kMediumTtlSec);

  // Force all URL encoding to use normal encoding so that the relative URL
  // trimming logic can work and give us a relative URL result as expected.
  TestUrlNamer::UseNormalEncoding(true);

  GoogleString a_ext = Encode(kCssSubdir, "ce", "0", kCssTail, "css");
  ValidateExpected("ce_then_trim",
                   StringPrintf(kCssFormat, kCssFile),
                   StringPrintf(kCssFormat, a_ext.c_str()));
}

TEST_F(CacheExtenderTest, DefangHtml) {
  options()->EnableExtendCacheFilters();
  rewrite_driver()->AddFilters();
  // Make sure that we downgrade HTML and similar executable types
  // to text/plain if we cache extend them. This closes off XSS
  // vectors if the domain lawyer is (mis)configured too loosely.
  SetResponseWithDefaultHeaders("a.html", kContentTypeHtml,
                                "boo!", kShortTtlSec);
  SetResponseWithDefaultHeaders("a.xhtml", kContentTypeXhtml,
                                "bwahahaha!", kShortTtlSec);
  SetResponseWithDefaultHeaders("a.xml", kContentTypeXml,
                                "boo!", kShortTtlSec);

  const GoogleString css_before = StrCat(CssLinkHref("a.html"),
                                         CssLinkHref("a.xhtml"),
                                         CssLinkHref("a.xml"));
  ValidateNoChanges("defang", css_before);
}

// Negative test to ensure we do not cache-extend CSS that was already
// minified (and thus has a long cache lifetime).
TEST_F(CacheExtenderTest, DoNotExtendRewrittenCss) {
  static const char kRewriteDomain[] = "http://rewrite.example.com/";
  static const char kShard1Domain[] = "http://shard1.example.com/";
  static const char kShard2Domain[] = "http://shard2.example.com/";
  DomainLawyer* lawyer = options()->WriteableDomainLawyer();
  lawyer->AddRewriteDomainMapping(kRewriteDomain, kTestDomain,
                                  message_handler());
  lawyer->AddShard(kRewriteDomain,
                   StrCat(kShard1Domain, ",", kShard2Domain),
                   message_handler());
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  InitTest(kShortTtlSec);
  ValidateExpected(
      "do_not_extend_rewritten_css",
      StringPrintf(kCssFormat, kCssFile),
      StringPrintf(kCssFormat, Encode(
          StrCat(kShard1Domain, kCssSubdir), RewriteOptions::kCssFilterId,
          "0", kCssTail, "css").c_str()));

  // Also we shouldn't have bumped our stat mistakenly just because
  // the CSS filter optimized the slot.
  EXPECT_EQ(0,
            rewrite_driver()->statistics()->GetVariable(
                CacheExtender::kCacheExtensions)->Get());
}

// See: http://www.alistapart.com/articles/alternate/
//  and http://www.w3.org/TR/html4/present/styles.html#h-14.3.1
TEST_F(CacheExtenderTest, AlternateStylesheet) {
  InitTest(kMediumTtlSec);

  const char html_format[] = "<link rel='%s' href='%s' title='foo'>";
  const GoogleString new_url = Encode(kCssSubdir, "ce", "0", kCssTail, "css");

  ValidateExpected("preferred_stylesheet",
                   StringPrintf(html_format, "stylesheet", kCssFile),
                   StringPrintf(html_format, "stylesheet", new_url.c_str()));

  ValidateExpected("alternate_stylesheet",
                   StringPrintf(html_format, "alternate stylesheet", kCssFile),
                   StringPrintf(html_format, "alternate stylesheet",
                                new_url.c_str()));

  ValidateExpected("alternate_stylesheet2",
                   StringPrintf(html_format, " StyleSheet alterNATE  ",
                                kCssFile),
                   StringPrintf(html_format, " StyleSheet alterNATE  ",
                                new_url.c_str()));

  ValidateExpected("alternate_stylesheet_and_more",
                   StringPrintf(html_format, "  foo stylesheet alternate bar ",
                                kCssFile),
                   StringPrintf(html_format, "  foo stylesheet alternate bar ",
                                new_url.c_str()));
  ValidateNoChanges("alternate_not_stylesheet",
                    StringPrintf(html_format, "alternate snowflake", kCssFile));
}

TEST_F(CacheExtenderTest, PreserveUrlRelativity) {
  options()->set_preserve_url_relativity(true);
  InitTest(kMediumTtlSec);

  ValidateExpected("preserve_url_relativity",
                   "<img src=b.jpg>",
                   "<img src=b.jpg.pagespeed.ce.0.jpg>");
}

TEST_F(CacheExtenderTest, NoPreserveUrlRelativity) {
  options()->set_preserve_url_relativity(false);
  InitTest(kMediumTtlSec);

  ValidateExpected("preserve_url_relativity",
                   "<img src=b.jpg>",
                   "<img src=http://test.com/b.jpg.pagespeed.ce.0.jpg>");
}

}  // namespace

}  // namespace net_instaweb
