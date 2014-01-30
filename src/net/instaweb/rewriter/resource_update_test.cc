/*
 * Copyright 2011 Google Inc.
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

// Unit-test the RewriteContext class.  This is made simplest by
// setting up some dummy rewriters in our test framework.

#include <vector>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"  // for Code::kOK
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_context_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

// Test resource update behavior.
class ResourceUpdateTest : public RewriteContextTestBase {
 protected:
  static const char kOriginalUrl[];

  ResourceUpdateTest() {
    FetcherUpdateDateHeaders();
  }

  // Rewrite supplied HTML, search find rewritten resource URL (EXPECT only 1),
  // and return the fetched contents of that resource.
  //
  // Helper function to specific functions below.
  GoogleString RewriteResource(const StringPiece& id,
                               const GoogleString& html_input) {
    // We use MD5 hasher instead of mock hasher so that different resources
    // are assigned different URLs.
    UseMd5Hasher();

    // Rewrite HTML.
    Parse(id, html_input);

    // Find rewritten resource URL.
    StringVector css_urls;
    CollectCssLinks(StrCat(id, "-collect"), output_buffer_, &css_urls);
    EXPECT_EQ(1UL, css_urls.size());
    const GoogleString& rewritten_url = AbsolutifyUrl(css_urls[0]);

    // Fetch rewritten resource
    return FetchUrlAndCheckHash(rewritten_url);
  }

  GoogleString FetchUrlAndCheckHash(const StringPiece& url) {
    // Fetch resource.
    GoogleString contents;
    EXPECT_TRUE(FetchResourceUrl(url, &contents));

    // Check that hash code is correct.
    ResourceNamer namer;
    namer.Decode(url);
    EXPECT_EQ(hasher()->Hash(contents), namer.hash());

    return contents;
  }

  // Simulates requesting HTML doc and then loading resource.
  GoogleString RewriteSingleResource(const StringPiece& id) {
    return RewriteResource(id, CssLinkHref(kOriginalUrl));
  }

  GoogleString CombineResources(const StringPiece& id) {
    return RewriteResource(
        id, StrCat(CssLinkHref("web/a.css"), CssLinkHref("file/b.css"),
                   CssLinkHref("web/c.css"), CssLinkHref("file/d.css")));
  }

  StringVector RewriteNestedResources(const StringPiece& id) {
    // Rewrite everything and fetch the rewritten main resource.
    GoogleString rewritten_list = RewriteResource(id, CssLinkHref("main.txt"));

    // Parse URLs for subresources.
    StringPieceVector urls;
    SplitStringPieceToVector(rewritten_list, "\n", &urls, true);

    // Load text of subresources.
    StringVector subresources;
    for (StringPieceVector::const_iterator iter = urls.begin();
         iter != urls.end(); ++iter) {
      subresources.push_back(FetchUrlAndCheckHash(*iter));
    }
    return subresources;
  }

  void ReconfigureNestedFilter(
      bool expected_nested_rewrite_result) {
    nested_filter_->set_expected_nested_rewrite_result(
        expected_nested_rewrite_result);
  }
};

const char ResourceUpdateTest::kOriginalUrl[] = "a.css";

// Test to make sure that 404's expire.
TEST_F(ResourceUpdateTest, TestExpire404) {
  InitTrimFilters(kRewrittenResource);

  // First, set a 404.
  SetFetchResponse404(kOriginalUrl);

  // Trying to rewrite it should not do anything..
  ValidateNoChanges("404", CssLinkHref(kOriginalUrl));

  // Now move forward 20 years and upload a new version. We should
  // be ready to optimize at that point.
  // "And thus Moses wandered the desert for only 20 years, because of a
  // limitation in the implementation of time_t."
  AdvanceTimeMs(20 * Timer::kYearMs);
  SetResponseWithDefaultHeaders(kOriginalUrl, kContentTypeCss, " init ", 100);
  EXPECT_EQ("init", RewriteSingleResource("200"));
}

TEST_F(ResourceUpdateTest, OnTheFly) {
  InitTrimFilters(kOnTheFlyResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  SetResponseWithDefaultHeaders(kOriginalUrl, kContentTypeCss,
                                " init ", ttl_ms / 1000);
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  // TODO(sligocki): Why are we rewriting twice here?
  // EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources have expired.
  AdvanceTimeMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 3) Change resource.
  SetResponseWithDefaultHeaders(kOriginalUrl, kContentTypeCss,
                                " new ", ttl_ms / 1000);
  ClearStats();
  // Rewrite should still be the same, because it's found in cache.
  EXPECT_EQ("init", RewriteSingleResource("stale_content"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  AdvanceTimeMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  // EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

TEST_F(ResourceUpdateTest, Rewritten) {
  FetcherUpdateDateHeaders();
  InitTrimFilters(kRewrittenResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  ResponseHeaders response_headers;
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  response_headers.Add(HttpAttributes::kContentType,
                       kContentTypeCss.mime_type());
  response_headers.Add(HttpAttributes::kEtag, "original");
  response_headers.SetDateAndCaching(timer()->NowMs(), ttl_ms);
  response_headers.ComputeCaching();
  mock_url_fetcher()->SetConditionalResponse(
      "http://test.com/a.css", -1, "original", response_headers, " init ");

  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(6, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources have expired.
  AdvanceTimeMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 3) Change resource.
  response_headers.Replace(HttpAttributes::kEtag, "new");
  mock_url_fetcher()->SetConditionalResponse(
      "http://test.com/a.css", -1, "new", response_headers, " new ");

  ClearStats();
  // Rewrite should still be the same, because it's found in cache.
  EXPECT_EQ("init", RewriteSingleResource("stale_content"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  AdvanceTimeMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(5, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 5) Advance time so that the new input resource expires and is conditionally
  // refreshed.
  AdvanceTimeMs(2 * ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(
      1,
      server_context()->rewrite_stats()->num_conditional_refreshes()->Get());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

TEST_F(ResourceUpdateTest, LoadFromFileOnTheFly) {
  options()->file_load_policy()->Associate(kTestDomain, "/test/");
  InitTrimFilters(kOnTheFlyResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  WriteFile("/test/a.css", " init ");
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  // EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // EXPECT_EQ(1, file_system()->num_input_file_opens());
  EXPECT_EQ(2, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources would have expired if
  // they were loaded by UrlFetch.
  AdvanceTimeMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // 3) Change resource.
  WriteFile("/test/a.css", " new ");
  ClearStats();
  // Rewrite should immediately update.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  // EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // EXPECT_EQ(1, file_system()->num_input_file_opens());
  EXPECT_EQ(2, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  AdvanceTimeMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());
}

TEST_F(ResourceUpdateTest, LoadFromFileRewritten) {
  options()->file_load_policy()->Associate(kTestDomain, "/test/");
  InitTrimFilters(kRewrittenResource);

  int64 ttl_ms = 5 * Timer::kMinuteMs;

  // 1) Set first version of resource.
  WriteFile("/test/a.css", " init ");
  ClearStats();
  EXPECT_EQ("init", RewriteSingleResource("first_load"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // 2) Advance time, but not so far that resources would have expired if
  // they were loaded by UrlFetch.
  AdvanceTimeMs(ttl_ms / 2);
  ClearStats();
  // Rewrite should be the same.
  EXPECT_EQ("init", RewriteSingleResource("advance_time"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());

  // 3) Change resource.
  WriteFile("/test/a.css", " new ");
  ClearStats();
  // Rewrite should immediately update.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // 4) Advance time so that old cached input resource expires.
  AdvanceTimeMs(ttl_ms);
  ClearStats();
  // Rewrite should now use new resource.
  EXPECT_EQ("new", RewriteSingleResource("updated_content"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
}

class CombineResourceUpdateTest : public ResourceUpdateTest {
 protected:
};

TEST_F(CombineResourceUpdateTest, CombineDifferentTTLs) {
  // Initialize system.
  InitCombiningFilter(0);
  options()->file_load_policy()->Associate("http://test.com/file/", "/test/");

  // Initialize resources.
  int64 kLongTtlMs = 1 * Timer::kMonthMs;
  int64 kShortTtlMs = 1 * Timer::kMinuteMs;
  SetResponseWithDefaultHeaders("http://test.com/web/a.css", kContentTypeCss,
                                " a1 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b1 ");
  SetResponseWithDefaultHeaders("http://test.com/web/c.css", kContentTypeCss,
                                " c1 ", kShortTtlMs / 1000);
  WriteFile("/test/d.css", " d1 ");

  // 1) Initial combined resource.
  EXPECT_EQ(" a1  b1  c1  d1 ", CombineResources("first_load"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(2, file_system()->num_input_file_opens());
  // Note that we stat each file as we load it in.
  EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();

  // 2) Advance time, but not so far that any resources have expired.
  AdvanceTimeMs(kShortTtlMs / 2);
  // Rewrite should be the same.
  EXPECT_EQ(" a1  b1  c1  d1 ", CombineResources("advance_time"));
  EXPECT_EQ(0, combining_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();

  // 3) Change resources
  SetResponseWithDefaultHeaders("http://test.com/web/a.css", kContentTypeCss,
                                " a2 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b2 ");
  SetResponseWithDefaultHeaders("http://test.com/web/c.css", kContentTypeCss,
                                " c2 ", kShortTtlMs / 1000);
  WriteFile("/test/d.css", " d2 ");
  // File-based resources should be updated, but web-based ones still cached.
  EXPECT_EQ(" a1  b2  c1  d2 ", CombineResources("stale_content"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());  // Because inputs updated.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(2, file_system()->num_input_file_opens());  // Read both files.
  // 2 reads + stat of b
  EXPECT_EQ(3, file_system()->num_input_file_stats());
  ClearStats();

  // 4) Advance time so that short-cached input expires.
  AdvanceTimeMs(kShortTtlMs);
  // All but long TTL UrlInputResource should be updated.
  EXPECT_EQ(" a1  b2  c2  d2 ", CombineResources("short_updated"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());  // Because inputs updated.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());  // One expired.
  EXPECT_EQ(2, file_system()->num_input_file_opens());  // Re-read files.
  // 2 file reads + stat of b, which we get to as a has long TTL,
  // as well as as of d (for figuring out revalidation strategy).
  EXPECT_EQ(4, file_system()->num_input_file_stats());
  ClearStats();

  // 5) Advance time so that all inputs have expired and been updated.
  AdvanceTimeMs(kLongTtlMs);
  // Rewrite should now use all new resources.
  EXPECT_EQ(" a2  b2  c2  d2 ", CombineResources("all_updated"));
  EXPECT_EQ(1, combining_filter_->num_rewrites());  // Because inputs updated.
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());  // Both expired.
  EXPECT_EQ(2, file_system()->num_input_file_opens());  // Re-read files.
  // 2 read-induced stats, 2 stats to figure out how to deal with
  // c + d for invalidation.
  EXPECT_EQ(4, file_system()->num_input_file_stats());
  ClearStats();
}

TEST_F(ResourceUpdateTest, NestedTestExpireNested404) {
  UseMd5Hasher();
  InitNestedFilter(NestedFilter::kExpectNestedRewritesFail);

  const int64 kDecadeMs = 10 * Timer::kYearMs;

  // Have the nested one have a 404...
  const GoogleString kOutUrl = Encode("", "nf", "sdUklQf3sx",
                                      "main.txt", "css");
  SetResponseWithDefaultHeaders("http://test.com/main.txt", kContentTypeCss,
                                "a.css\n", 4 * kDecadeMs / 1000);
  SetFetchResponse404("a.css");

  ValidateExpected("nested_404", CssLinkHref("main.txt"), CssLinkHref(kOutUrl));
  GoogleString contents;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, kOutUrl), &contents));
  EXPECT_EQ("http://test.com/a.css\n", contents);

  // Determine if we're using the TestUrlNamer, for the hash later.
  CHECK(!factory_->use_test_url_namer());

  // Now move forward two decades, and upload a new version. We should
  // be ready to optimize at that point, but input should not be expired.
  AdvanceTimeMs(2 * kDecadeMs);
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, " lowercase ", 100);
  ReconfigureNestedFilter(NestedFilter::kExpectNestedRewritesSucceed);
  const GoogleString kFullOutUrl =
      Encode("", "nf", "G60oQsKZ9F", "main.txt", "css");
  const GoogleString kInnerUrl = StrCat(Encode("", "uc", "N4LKMOq9ms",
                                               "a.css", "css"), "\n");
  ValidateExpected("nested_404", CssLinkHref("main.txt"),
                   CssLinkHref(kFullOutUrl));
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, kFullOutUrl), &contents));
  EXPECT_EQ(StrCat(kTestDomain, kInnerUrl), contents);
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, kInnerUrl), &contents));
  EXPECT_EQ(" LOWERCASE ", contents);
}

TEST_F(ResourceUpdateTest, NestedDifferentTTLs) {
  // Initialize system.
  InitNestedFilter(NestedFilter::kExpectNestedRewritesSucceed);
  options()->file_load_policy()->Associate("http://test.com/file/", "/test/");

  // Initialize resources.
  const int64 kExtraLongTtlMs = 10 * Timer::kMonthMs;
  const int64 kLongTtlMs = 1 * Timer::kMonthMs;
  const int64 kShortTtlMs = 1 * Timer::kMinuteMs;
  SetResponseWithDefaultHeaders("http://test.com/main.txt", kContentTypeCss,
                                "web/a.css\n"
                                "file/b.css\n"
                                "web/c.css\n", kExtraLongTtlMs / 1000);
  SetResponseWithDefaultHeaders("http://test.com/web/a.css", kContentTypeCss,
                                " a1 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b1 ");
  SetResponseWithDefaultHeaders("http://test.com/web/c.css", kContentTypeCss,
                                " c1 ", kShortTtlMs / 1000);

  ClearStats();
  // 1) Initial combined resource.
  StringVector result_vector;
  result_vector = RewriteNestedResources("first_load");
  ASSERT_EQ(3, result_vector.size());
  EXPECT_EQ(" A1 ", result_vector[0]);
  EXPECT_EQ(" B1 ", result_vector[1]);
  EXPECT_EQ(" C1 ", result_vector[2]);
  EXPECT_EQ(1, nested_filter_->num_top_rewrites());
  // 3 nested rewrites during actual rewrite, 3 when redoing them for
  // on-the-fly when checking the output.
  EXPECT_EQ(6, nested_filter_->num_sub_rewrites());
  EXPECT_EQ(3, counting_url_async_fetcher()->fetch_count());
  // b.css, twice (rewrite and fetch)
  EXPECT_EQ(2, file_system()->num_input_file_opens());
  // b.css twice, again.
  EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();

  // 2) Advance time, but not so far that any resources have expired.
  AdvanceTimeMs(kShortTtlMs / 2);
  // Rewrite should be the same.
  result_vector = RewriteNestedResources("advance_time");
  ASSERT_EQ(3, result_vector.size());
  EXPECT_EQ(" A1 ", result_vector[0]);
  EXPECT_EQ(" B1 ", result_vector[1]);
  EXPECT_EQ(" C1 ", result_vector[2]);
  EXPECT_EQ(0, nested_filter_->num_top_rewrites());
  EXPECT_EQ(3, nested_filter_->num_sub_rewrites());  // on inner fetch.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // b on rewrite.
  EXPECT_EQ(1, file_system()->num_input_file_opens());
  // re-check of b, b on rewrite.
  EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();

  // 3) Change resources
  SetResponseWithDefaultHeaders("http://test.com/web/a.css", kContentTypeCss,
                                " a2 ", kLongTtlMs / 1000);
  WriteFile("/test/b.css", " b2 ");
  SetResponseWithDefaultHeaders("http://test.com/web/c.css", kContentTypeCss,
                                " c2 ", kShortTtlMs / 1000);
  // File-based resources should be updated, but web-based ones still cached.
  result_vector = RewriteNestedResources("stale_content");
  ASSERT_EQ(3, result_vector.size());
  EXPECT_EQ(" A1 ", result_vector[0]);
  EXPECT_EQ(" B2 ", result_vector[1]);
  EXPECT_EQ(" C1 ", result_vector[2]);
  EXPECT_EQ(1, nested_filter_->num_top_rewrites());  // Because inputs updated

  // on rewrite, b.css; when checking inside RewriteNestedResources, all 3
  // got rewritten.
  EXPECT_EQ(4, nested_filter_->num_sub_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // b.css, b.css.pagespeed.nf.HASH.css
  EXPECT_EQ(2, file_system()->num_input_file_opens());

  // The stats here are:
  // 1) Stat b.css to figure out if top-level rewrite is valid.
  // 2) Stats of the 3 inputs when doing on-the-fly rewriting when
  //    responding to fetches of inner stuff.
  EXPECT_EQ(4, file_system()->num_input_file_stats());
  ClearStats();

  // 4) Advance time so that short-cached input expires.
  AdvanceTimeMs(kShortTtlMs);
  // All but long TTL UrlInputResource should be updated.
  result_vector = RewriteNestedResources("short_updated");
  ASSERT_EQ(3, result_vector.size());
  EXPECT_EQ(" A1 ", result_vector[0]);
  EXPECT_EQ(" B2 ", result_vector[1]);
  EXPECT_EQ(" C2 ", result_vector[2]);
  EXPECT_EQ(1, nested_filter_->num_top_rewrites());  // Because inputs updated
  EXPECT_EQ(4, nested_filter_->num_sub_rewrites());  // c.css + check fetches
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());  // c.css
  // b.css
  EXPECT_EQ(1, file_system()->num_input_file_opens());
  // b.css, when rewriting outer and fetching inner
  EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();

  // 5) Advance time so that all inputs have expired and been updated.
  AdvanceTimeMs(kLongTtlMs);
  // Rewrite should now use all new resources.
  result_vector = RewriteNestedResources("short_updated");
  ASSERT_EQ(3, result_vector.size());
  EXPECT_EQ(" A2 ", result_vector[0]);
  EXPECT_EQ(" B2 ", result_vector[1]);
  EXPECT_EQ(" C2 ", result_vector[2]);
  EXPECT_EQ(1, nested_filter_->num_top_rewrites());  // Because inputs updated

  // For rewrite of top-level, we re-do a.css (actually changed) and c.css
  // (as it's expired, and we don't check if it's really changed for on-the-fly
  // filters). Then there are 3 when we actually fetch them individually.
  EXPECT_EQ(5, nested_filter_->num_sub_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());  // a.css, c.css
  EXPECT_EQ(1, file_system()->num_input_file_opens());
  EXPECT_EQ(2, file_system()->num_input_file_stats());
  ClearStats();
}

}  // namespace net_instaweb
