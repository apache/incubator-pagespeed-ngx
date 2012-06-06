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
#include "net/instaweb/rewriter/public/css_outline_filter.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kCssFormat[] = "<link rel='stylesheet' href='%s' type='text/css'>\n";
const char kHtmlFormat[] =
    "<link rel='stylesheet' href='%s' type='text/css'>\n"
    "<img src='%s'/>\n"
    "<script type='text/javascript' src='%s'></script>\n";

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

class CacheExtenderTest : public ResourceManagerTestBase {
 protected:
  CacheExtenderTest()
      : kCssData(CssData("")),
        kCssPath(StrCat(kTestDomain, kCssSubdir)) {
  }

  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();
  }

  void InitTest(int64 ttl) {
    options()->EnableExtendCacheFilters();
    rewrite_driver()->AddFilters();
    SetResponseWithDefaultHeaders(kCssFile, kContentTypeCss, kCssData, ttl);
    SetResponseWithDefaultHeaders("b.jpg", kContentTypeJpeg, kImageData, ttl);
    SetResponseWithDefaultHeaders("c.js", kContentTypeJavascript, kJsData, ttl);
    SetResponseWithDefaultHeaders("introspective.js", kContentTypeJavascript,
                                  kJsDataIntrospective, ttl);
  }

  // Generate HTML loading 3 resources with the specified URLs
  GoogleString GenerateHtml(const GoogleString& a,
                            const GoogleString& b,
                            const GoogleString& c) {
    return StringPrintf(kHtmlFormat, a.c_str(), b.c_str(), c.c_str());
  }

  // Helper to test for how we handle trailing junk in URLs
  void TestCorruptUrl(StringPiece junk, bool append_junk) {
    InitTest(kShortTtlSec);
    GoogleString a_ext = Encode(kCssPath, "ce", "0", kCssTail, "css");
    GoogleString b_ext = Encode(kTestDomain, "ce", "0", "b.jpg", "jpg");
    GoogleString c_ext = Encode(kTestDomain, "ce", "0", "c.js", "js");

    ValidateExpected("no_ext_corrupt_fetched",
                     GenerateHtml(kCssFile, "b.jpg", "c.js"),
                     GenerateHtml(a_ext, b_ext, c_ext));
    GoogleString output;
    EXPECT_TRUE(
        FetchResourceUrl(
            ChangeSuffix(a_ext, append_junk, ".css", junk), &output));
    EXPECT_TRUE(
        FetchResourceUrl(
            ChangeSuffix(b_ext, append_junk, ".jpg", junk), &output));
    EXPECT_TRUE(
        FetchResourceUrl(
            ChangeSuffix(c_ext, append_junk, ".js", junk), &output));
    ValidateExpected("no_ext_corrupt_cached",
                     GenerateHtml(kCssFile, "b.jpg", "c.js"),
                     GenerateHtml(a_ext, b_ext, c_ext));
  }

  static GoogleString CssData(const StringPiece& url) {
    return StringPrintf(kCssDataFormat, url.as_string().c_str());
  }

  const GoogleString kCssData;
  const GoogleString kCssPath;
};

TEST_F(CacheExtenderTest, DoExtend) {
  InitTest(kShortTtlSec);
  for (int i = 0; i < 3; i++) {
    ValidateExpected(
        "do_extend",
        GenerateHtml(kCssFile, "b.jpg", "c.js"),
        GenerateHtml(Encode(kCssPath, "ce", "0", kCssTail, "css"),
                     Encode(kTestDomain, "ce", "0", "b.jpg", "jpg"),
                     Encode(kTestDomain, "ce", "0", "c.js", "js")));
  }
}

TEST_F(CacheExtenderTest, DoNotExtendIntrospectiveJavascript) {
  options()->ClearSignatureForTesting();
  options()->set_avoid_renaming_introspective_javascript(true);
  InitTest(kShortTtlSec);
  const char kJsTemplate[] = "<script src=\"%s\"></script>";
  ValidateExpected(
      "dont_extend_introspective_js",
      StringPrintf(kJsTemplate, "introspective.js"),
      StringPrintf(kJsTemplate, "introspective.js"));
}

TEST_F(CacheExtenderTest, DoExtendIntrospectiveJavascriptByDefault) {
  InitTest(kShortTtlSec);
  const char kJsTemplate[] = "<script src=\"%s\"></script>";
  ValidateExpected(
      "do_extend_introspective_js",
      StringPrintf(kJsTemplate, "introspective.js"),
      StringPrintf(kJsTemplate,
                   Encode(kTestDomain, "ce", "0",
                          "introspective.js", "js").c_str()));
}

TEST_F(CacheExtenderTest, DoExtendLinkRelCaseInsensitive) {
  InitTest(kShortTtlSec);
  const char kMixedCaseTemplate[] = "<link rel=StyleSheet href='%s'>";
  ValidateExpected(
      "extend_ci",
      StringPrintf(kMixedCaseTemplate, kCssFile),
      StringPrintf(kMixedCaseTemplate,
                   Encode(kCssPath, "ce", "0", kCssTail, "css").c_str()));
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
        GenerateHtml(kCssFile, "b.jpg", "c.js"),
        GenerateHtml(kCssFile,
                     Encode(kTestDomain, "ce", "0", "b.jpg", "jpg"),
                     "c.js"));
  }
}

TEST_F(CacheExtenderTest, Handle404) {
  // Test to make sure that a missing input is handled well.
  SetFetchResponse404("404.css");
  ValidateNoChanges("404", "<link rel=stylesheet href='404.css'>");

  // Second time, to make sure caching doesn't break it.
  ValidateNoChanges("404", "<link rel=stylesheet href='404.css'>");
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
  ValidateNoChanges("url_too_long", GenerateHtml(css_name, jpg_name, js_name));
}

TEST_F(CacheExtenderTest, NoInputResource) {
  InitTest(kShortTtlSec);
  // Test for not crashing on bad/disallowed URL.
  ValidateNoChanges("bad url",
                    GenerateHtml("swly://example.com/sub/a.css",
                                 "http://evil.com/b.jpg",
                                 "http://moreevil.com/c.js"));
}

TEST_F(CacheExtenderTest, NoExtendAlreadyCachedProperly) {
  InitTest(kLongTtlSec);  // cached for a long time to begin with
  ValidateNoChanges("no_extend_cached_properly",
                    GenerateHtml(kCssFile, "b.jpg", "c.js"));
}

TEST_F(CacheExtenderTest, ExtendIfSharded) {
  InitTest(kLongTtlSec);  // cached for a long time to begin with
  EXPECT_TRUE(options()->domain_lawyer()->AddShard(
      kTestDomain, "shard0.com,shard1.com", &message_handler_));
  // shard0 is always selected in the test because of our mock hasher
  // that always returns 0.
  ValidateExpected("extend_if_sharded",
                   GenerateHtml(kCssFile, "b.jpg", "c.js"),
                   GenerateHtml(
                       Encode(StrCat("http://shard0.com/", kCssSubdir),
                              "ce", "0", kCssTail, "css"),
                       Encode("http://shard0.com/", "ce", "0", "b.jpg", "jpg"),
                       Encode("http://shard0.com/", "ce", "0", "c.js", "js")));
}

TEST_F(CacheExtenderTest, ExtendIfOriginMappedHttps) {
  InitTest(kShortTtlSec);
  EXPECT_TRUE(options()->domain_lawyer()->AddOriginDomainMapping(
      kTestDomain, "https://cdn.com", &message_handler_));
  ValidateExpected("extend_if_origin_mapped_https",
                   GenerateHtml("https://cdn.com/sub/a.css?v=1",
                                "https://cdn.com/b.jpg",
                                "https://cdn.com/c.js"),
                   GenerateHtml(
                       Encode("https://cdn.com/sub/", "ce", "0", kCssTail,
                              "css"),
                       Encode("https://cdn.com/", "ce", "0", "b.jpg", "jpg"),
                       Encode("https://cdn.com/", "ce", "0", "c.js", "js")));
}

TEST_F(CacheExtenderTest, ExtendIfRewritten) {
  InitTest(kLongTtlSec);  // cached for a long time to begin with

  EXPECT_TRUE(options()->domain_lawyer()->AddRewriteDomainMapping(
      "cdn.com", kTestDomain, &message_handler_));
  ValidateExpected("extend_if_rewritten",
                   GenerateHtml(kCssFile, "b.jpg", "c.js"),
                   GenerateHtml(
                       Encode("http://cdn.com/sub/", "ce", "0", kCssTail,
                              "css"),
                       Encode("http://cdn.com/", "ce", "0", "b.jpg", "jpg"),
                       Encode("http://cdn.com/", "ce", "0", "c.js", "js")));
}

TEST_F(CacheExtenderTest, ExtendIfShardedAndRewritten) {
  InitTest(kLongTtlSec);  // cached for a long time to begin with

  EXPECT_TRUE(options()->domain_lawyer()->AddRewriteDomainMapping(
      "cdn.com", kTestDomain, &message_handler_));

  // Domain-rewriting is performed first.  Then we shard.
  EXPECT_TRUE(options()->domain_lawyer()->AddShard(
      "cdn.com", "shard0.com,shard1.com", &message_handler_));
  // shard0 is always selected in the test because of our mock hasher
  // that always returns 0.
  ValidateExpected("extend_if_sharded_and_rewritten",
                   GenerateHtml(kCssFile, "b.jpg", "c.js"),
                   GenerateHtml(
                       Encode("http://shard0.com/sub/", "ce", "0", kCssTail,
                              "css"),
                       Encode("http://shard0.com/", "ce", "0", "b.jpg", "jpg"),
                       Encode("http://shard0.com/", "ce", "0", "c.js", "js")));
}

TEST_F(CacheExtenderTest, ExtendIfShardedToHttps) {
  InitTest(kLongTtlSec);

  // This Origin Mapping ensures any fetches are converted to http so work.
  EXPECT_TRUE(options()->domain_lawyer()->AddOriginDomainMapping(
      kTestDomain, "https://test.com", &message_handler_));

  EXPECT_TRUE(options()->domain_lawyer()->AddShard(
      "https://test.com", "https://shard0.com,https://shard1.com",
      &message_handler_));
  // shard0 is always selected in the test because of our mock hasher
  // that always returns 0.
  ValidateExpected("extend_if_sharded_to_https",
                   GenerateHtml("https://test.com/sub/a.css?v=1",
                                "https://test.com/b.jpg",
                                "https://test.com/c.js"),
                   GenerateHtml(
                       Encode("https://shard0.com/sub/", "ce", "0", kCssTail,
                              "css"),
                       Encode("https://shard0.com/", "ce", "0", "b.jpg", "jpg"),
                       Encode("https://shard0.com/", "ce", "0", "c.js", "js")));
}

// TODO(jmarantz): consider implementing and testing the sharding and
// domain-rewriting of uncacheable resources -- just don't sign the URLs.

TEST_F(CacheExtenderTest, NoExtendOriginUncacheable) {
  InitTest(0);  // origin not cacheable
  ValidateNoChanges("no_extend_origin_not_cacheable",
                    GenerateHtml(kCssFile, "b.jpg", "c.js"));
}

TEST_F(CacheExtenderTest, ServeFiles) {
  GoogleString content;

  InitTest(kShortTtlSec);
  // To ensure there's no absolutification (below) of embedded.png's URL in
  // the served CSS file, we have to serve it from test.com and not from
  // cdn.com which TestUrlNamer does when it's being used.
  ASSERT_TRUE(FetchResourceUrl(
      EncodeNormal(kCssPath, kFilterId, "0", kCssTail, "css"), &content));
  EXPECT_EQ(kCssData, content);  // no absolutification
  ASSERT_TRUE(FetchResource(kTestDomain, kFilterId, "b.jpg", "jpg", &content));
  EXPECT_EQ(GoogleString(kImageData), content);
  ASSERT_TRUE(FetchResource(kTestDomain, kFilterId, "c.js", "js", &content));
  EXPECT_EQ(GoogleString(kJsData), content);
}

TEST_F(CacheExtenderTest, ConsistentHashWithRewrite) {
  // Since CacheExtend is an on-the-fly filter, ServeFilesWithRewrite, above,
  // verifies that we can decode a cache-extended CSS file and properly
  // domain-rewrite embedded images.  However, we go through the exercise
  // of generating the rewritten content in the HTML path too -- we just
  // don't cache it.  However, what we must do is generate the correct hash
  // code.  To test that we need to use the real hasher.
  UseMd5Hasher();
  DomainLawyer* lawyer = options()->domain_lawyer();
  lawyer->AddRewriteDomainMapping(kNewDomain, kTestDomain, &message_handler_);
  InitTest(kShortTtlSec);

  // First do the HTML rewrite.
  GoogleString hash = hasher()->Hash(kCssData);
  GoogleString extended_css = Encode(StrCat(kNewDomain, kCssSubdir), "ce", hash,
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
  DomainLawyer* lawyer = options()->domain_lawyer();
  lawyer->AddRewriteDomainMapping(kNewDomain, kTestDomain, &message_handler_);
  lawyer->AddShard(kNewDomain, "shard1.com,shard2.com", &message_handler_);
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
  DomainLawyer* lawyer = options()->domain_lawyer();
  lawyer->AddRewriteDomainMapping(kNewDomain, kTestDomain, &message_handler_);
  InitTest(kShortTtlSec);
  ASSERT_TRUE(FetchResource(kCssPath, kFilterId, kCssTail, "css", &content));
  EXPECT_EQ(CssData("http://new.com/sub/"), content);
}

TEST_F(CacheExtenderTest, ServeFilesWithRewriteDomainAndPathEnabled) {
  GoogleString content;
  DomainLawyer* lawyer = options()->domain_lawyer();
  lawyer->AddRewriteDomainMapping("http://new.com/test/", kTestDomain,
                                  &message_handler_);
  InitTest(kShortTtlSec);
  ASSERT_TRUE(FetchResource(kCssPath, kFilterId, kCssTail, "css", &content));
  EXPECT_EQ(CssData("http://new.com/test/sub/"), content);
}

TEST_F(CacheExtenderTest, ServeFilesWithShard) {
  GoogleString content;
  DomainLawyer* lawyer = options()->domain_lawyer();
  lawyer->AddRewriteDomainMapping(kNewDomain, kTestDomain, &message_handler_);
  lawyer->AddShard(kNewDomain, "shard1.com,shard2.com", &message_handler_);
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

  GoogleString b_ext = Encode(kTestDomain, "ce", "0", "b.jpg", "jpg");
  ValidateExpected("and_img", "<img src=\"b.jpg\">",
                   StrCat("<img src=\"", b_ext, "\">"));

  RewriteStats* stats = resource_manager()->rewrite_stats();
  EXPECT_EQ(0, stats->cached_resource_fetches()->Get());
  EXPECT_EQ(0, stats->succeeded_filter_resource_fetches()->Get());
  GoogleString out;
  EXPECT_TRUE(FetchResourceUrl(b_ext, &out));
  EXPECT_EQ(0, stats->cached_resource_fetches()->Get());
  EXPECT_EQ(1, stats->succeeded_filter_resource_fetches()->Get());
}

// http://code.google.com/p/modpagespeed/issues/detail?id=324
TEST_F(CacheExtenderTest, RetainExtraHeaders) {
  GoogleString url = StrCat(kTestDomain, "retain.css");
  SetResponseWithDefaultHeaders(url, kContentTypeCss, kCssData, 300);
  // We must explicitly call ComputeSignature here because we are not
  // calling InitTest in this test.
  resource_manager()->ComputeSignature(options());
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
  DomainLawyer* lawyer = options()->domain_lawyer();
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
}

}  // namespace

}  // namespace net_instaweb
