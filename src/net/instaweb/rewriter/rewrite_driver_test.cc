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

// Author: jmarantz@google.com (Joshua D. Marantz)

#include "net/instaweb/rewriter/public/rewrite_driver.h"

#include "net/instaweb/htmlparse/public/empty_html_filter.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/mock_resource_callback.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/worker_test_base.h"
#include "pagespeed/kernel/base/abstract_mutex.h"

namespace net_instaweb {

class RewriteFilter;

class RewriteDriverTest : public RewriteTestBase {
 protected:
  RewriteDriverTest() {}

  bool CanDecodeUrl(const StringPiece& url) {
    GoogleUrl gurl(url);
    RewriteFilter* filter;
    OutputResourcePtr resource(
        rewrite_driver()->DecodeOutputResource(gurl, &filter));
    return (resource.get() != NULL);
  }

  GoogleString BaseUrlSpec() {
    return rewrite_driver()->base_url().Spec().as_string();
  }

  // A helper to call ComputeCurrentFlushWindowRewriteDelayMs() that allows
  // us to keep it private.
  int64 GetFlushTimeout() {
    return rewrite_driver()->ComputeCurrentFlushWindowRewriteDelayMs();
  }

  bool IsDone(RewriteDriver::WaitMode wait_mode, bool deadline_reached) {
    ScopedMutex lock(rewrite_driver()->rewrite_mutex());
    return rewrite_driver()->IsDone(wait_mode, deadline_reached);
  }

  void IncrementAsyncEventsCount() {
    rewrite_driver()->increment_async_events_count();
  }

  void DecrementAsyncEventsCount() {
    rewrite_driver()->decrement_async_events_count();
  }

  // Helper method used by various DownstreamCache*Test
  // test classes to setup options related to downstream cache handling.
  void SetUpOptionsForDownstreamCacheTesting(
      const StringPiece& downstream_cache_purge_method,
      const StringPiece& downstream_cache_purge_location_prefix) {
    options()->ClearSignatureForTesting();
    options()->set_downstream_cache_rewritten_percentage_threshold(95);
    options()->set_downstream_cache_purge_method(downstream_cache_purge_method);
    GoogleString msg;
    options()->ParseAndSetOptionFromName1(
        RewriteOptions::kDownstreamCachePurgeLocationPrefix,
        downstream_cache_purge_location_prefix, &msg,
        message_handler());
    options()->ComputeSignature();
  }

  void SetupResponsesForDownstreamCacheTesting() {
    // Setup responses for the resources.
    const char kCss[] = "* { display: none; }";
    SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);
    SetResponseWithDefaultHeaders("test/b.css", kContentTypeCss, kCss, 100);

    // Setup a fake response for the expected purge path.
    SetResponseWithDefaultHeaders("http://localhost:1234/purge/",
                                  kContentTypeCss, "", 100);
  }

  void ProcessHtmlForDownstreamCacheTesting() {
    GoogleString input_html(
        StrCat(CssLinkHref("a.css"), "  ", CssLinkHref("test/b.css")));
    ParseUrl(kTestDomain, input_html);
  }

  void TestBlockingRewrite(RequestHeaders* request_headers,
                           bool expected_blocking_rewrite,
                           bool expected_fast_blocking_rewrite) {
    rewrite_driver()->EnableBlockingRewrite(request_headers);
    EXPECT_EQ(expected_blocking_rewrite,
              rewrite_driver()->fully_rewrite_on_flush());
    EXPECT_EQ(expected_fast_blocking_rewrite,
              rewrite_driver()->fast_blocking_rewrite());
    // Reset the flags to their default values after the test.
    rewrite_driver()->set_fully_rewrite_on_flush(false);
    rewrite_driver()->set_fast_blocking_rewrite(true);
    EXPECT_FALSE(request_headers->Has(
        HttpAttributes::kXPsaBlockingRewrite));
    EXPECT_FALSE(request_headers->Has(
        HttpAttributes::kXPsaBlockingRewriteMode));
  }

  void TestPendingEventsIsDone(bool wait_for_completion) {
    EXPECT_TRUE(IsDone(RewriteDriver::kWaitForShutDown, false));
    EXPECT_TRUE(IsDone(RewriteDriver::kWaitForCompletion, false));

    IncrementAsyncEventsCount();
    EXPECT_FALSE(IsDone(RewriteDriver::kWaitForShutDown, false));
    EXPECT_EQ(wait_for_completion,
              IsDone(RewriteDriver::kWaitForCompletion, false));
    DecrementAsyncEventsCount();

    EXPECT_TRUE(IsDone(RewriteDriver::kWaitForShutDown, false));
    EXPECT_TRUE(IsDone(RewriteDriver::kWaitForCompletion, false));
  }

  void TestPendingEventsDriverCleanup(bool blocking_rewrite,
                                      bool fast_blocking_rewrite) {
    RewriteDriver* other_driver =
        server_context()->NewRewriteDriver(CreateRequestContext());
    other_driver->set_fully_rewrite_on_flush(blocking_rewrite);
    other_driver->set_fast_blocking_rewrite(fast_blocking_rewrite);
    other_driver->increment_async_events_count();
    other_driver->Cleanup();
    other_driver->decrement_async_events_count();
    EXPECT_EQ(0, server_context()->num_active_rewrite_drivers());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RewriteDriverTest);
};

namespace {

const char kBikePngFile[] = "BikeCrashIcn.png";

const char kNonRewrittenCachableHtml[] =
    "<html>\n<link rel=stylesheet href=a.css>  "
    "<link rel=stylesheet href=test/b.css>\n</html>";

const char kRewrittenCachableHtmlWithCacheExtension[] =
    "<html>\n"
    "<link rel=stylesheet href=a.css.pagespeed.ce.0.css>  "
    "<link rel=stylesheet href=test/b.css.pagespeed.ce.0.css>"
    "\n</html>";

const char kRewrittenCachableHtmlWithCollapseWhitespace[] =
    "<html>\n<link rel=stylesheet href=a.css> "
    "<link rel=stylesheet href=test/b.css>\n</html>";

TEST_F(RewriteDriverTest, NoChanges) {
  ValidateNoChanges("no_changes",
                    "<head><script src=\"foo.js\"></script></head>"
                    "<body><form method=\"post\">"
                    "<input type=\"checkbox\" checked>"
                    "</form></body>");
}

TEST_F(RewriteDriverTest, TestLegacyUrl) {
  rewrite_driver()->AddFilters();
  EXPECT_FALSE(CanDecodeUrl("http://example.com/dir/123/jm.0.orig"))
      << "not enough dots";
  EXPECT_TRUE(CanDecodeUrl("http://example.com/dir/123/jm.0.orig.js"));
  EXPECT_TRUE(CanDecodeUrl(
      "http://x.com/dir/123/jm.0123456789abcdef0123456789ABCDEF.orig.js"));
  EXPECT_FALSE(CanDecodeUrl("http://example.com/dir/123/xx.0.orig.js"))
      << "invalid filter xx";
  ASSERT_FALSE(CanDecodeUrl("http://example.com/dir/123/jm.z.orig.js"))
      << "invalid hash code -- not hex";
  ASSERT_FALSE(CanDecodeUrl("http://example.com/dir/123/jm.ab.orig.js"))
      << "invalid hash code -- not 1 or 32 chars";
  ASSERT_FALSE(CanDecodeUrl("http://example.com/dir/123/jm.0.orig.x"))
      << "invalid extension";
}

TEST_F(RewriteDriverTest, PagespeedObliviousPositiveTest) {
  RewriteOptions* ops = options();
  ops->set_oblivious_pagespeed_urls(false);  // Decode Pagespeed URL.
  rewrite_driver()->AddFilters();

  EXPECT_TRUE(CanDecodeUrl(
      "http://www.example.com/foresee-trigger.js.pagespeed.jm.0D45DpKAeI.js"));
}

TEST_F(RewriteDriverTest, PagespeedObliviousNegativeTest) {
  RewriteOptions* ops = options();
  ops->set_oblivious_pagespeed_urls(true);  // Don't decode Pagespeed URL.
  rewrite_driver()->AddFilters();
  EXPECT_FALSE(CanDecodeUrl(
      "http://www.example.com/foresee-trigger.js.pagespeed.jm.0D45DpKAeI.js"));
}

TEST_F(RewriteDriverTest, TestModernUrl) {
  rewrite_driver()->AddFilters();

  // Sanity-check on a valid one
  EXPECT_TRUE(CanDecodeUrl(
      Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpg")));

  // Query is OK, too.
  EXPECT_TRUE(CanDecodeUrl(
      StrCat(Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpg"),
             "?s=ok")));

  // Invalid filter code
  EXPECT_FALSE(CanDecodeUrl(
      Encode("http://example.com/", "NOFILTER", "HASH", "Puzzle.jpg", "jpg")));

  // Nonsense extension -- we will just ignore it these days.
  EXPECT_TRUE(CanDecodeUrl(
      Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpgif")));

  // No hash
  GoogleString encoded_url(Encode("http://example.com/", "ce", "123456789",
                                  "Puzzle.jpg", "jpg"));
  GlobalReplaceSubstring("123456789", "", &encoded_url);
  EXPECT_FALSE(CanDecodeUrl(encoded_url));
}

class RewriteDriverTestUrlNamer : public RewriteDriverTest {
 public:
  RewriteDriverTestUrlNamer() {
    SetUseTestUrlNamer(true);
  }
};

TEST_F(RewriteDriverTestUrlNamer, TestEncodedUrls) {
  rewrite_driver()->AddFilters();

  // Sanity-check on a valid one
  EXPECT_TRUE(CanDecodeUrl(
      Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpg")));

  // Query is OK, too.
  EXPECT_TRUE(CanDecodeUrl(
      StrCat(Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpg"),
             "?s=ok")));

  // Invalid filter code
  EXPECT_FALSE(CanDecodeUrl(
      Encode("http://example.com/", "NOFILTER", "HASH", "Puzzle.jpg", "jpg")));

  // Nonsense extension -- we will just ignore it these days.
  EXPECT_TRUE(CanDecodeUrl(
      Encode("http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpgif")));

  // No hash
  GoogleString encoded_url(Encode("http://example.com/", "ce", "123456789",
                                  "Puzzle.jpg", "jpg"));
  GlobalReplaceSubstring("123456789", "", &encoded_url);
  EXPECT_FALSE(CanDecodeUrl(encoded_url));

  // Valid proxy domain but invalid decoded URL.
  encoded_url = Encode("http://example.com/", "ce", "0", "Puzzle.jpg", "jpg");
  GlobalReplaceSubstring("example.com/",
                         "example.comWYTHQ000JRJFCAAKYU1EMA6VUBDTS4DESLRWIPMS"
                         "KKMQH0XYN1FURDBBSQ9AYXVX3TZDKZEIJNLRHU05ATHBAWWAG2+"
                         "ADDCXPWGGP1VTHJIYU13IIFQYSYMGKIMSFIEBM+HCAACVNGO8CX"
                         "XO%81%9F%F1m/", &encoded_url);
  // By default TestUrlNamer doesn't proxy but we need it to for this test.
  TestUrlNamer::SetProxyMode(true);
  EXPECT_FALSE(CanDecodeUrl(encoded_url));
}

TEST_F(RewriteDriverTestUrlNamer, TestDecodeUrls) {
  // Sanity-check on a valid one
  GoogleUrl gurl_good(Encode(
      "http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpg"));
  rewrite_driver()->AddFilters();
  StringVector urls;
  TestUrlNamer::SetProxyMode(true);
  EXPECT_TRUE(rewrite_driver()->DecodeUrl(gurl_good, &urls));
  EXPECT_EQ(1, urls.size());
  EXPECT_EQ("http://example.com/Puzzle.jpg", urls[0]);

  // Invalid filter code
  urls.clear();
  GoogleUrl gurl_bad(Encode(
      "http://example.com/", "NOFILTER", "HASH", "Puzzle.jpg", "jpgif"));
  EXPECT_FALSE(rewrite_driver()->DecodeUrl(gurl_bad, &urls));

  // Combine filters
  urls.clear();
  GoogleUrl gurl_multi(Encode(
      "http://example.com/", "cc", "HASH", MultiUrl("a.css", "b.css"), "css"));
  EXPECT_TRUE(rewrite_driver()->DecodeUrl(gurl_multi, &urls));
  EXPECT_EQ(2, urls.size());
  EXPECT_EQ("http://example.com/a.css", urls[0]);
  EXPECT_EQ("http://example.com/b.css", urls[1]);

  // Invalid Url.
  urls.clear();
  GoogleUrl gurl_invalid("invalid url");
  EXPECT_FALSE(rewrite_driver()->DecodeUrl(gurl_invalid, &urls));
  EXPECT_EQ(0, urls.size());

  // ProxyMode off
  urls.clear();
  TestUrlNamer::SetProxyMode(false);
  SetUseTestUrlNamer(false);
  gurl_good.Reset(Encode(
      "http://example.com/", "ce", "HASH", "Puzzle.jpg", "jpg"));
  EXPECT_TRUE(rewrite_driver()->DecodeUrl(gurl_good, &urls));
  EXPECT_EQ(1, urls.size());
  EXPECT_EQ("http://example.com/Puzzle.jpg", urls[0]);

  urls.clear();
  gurl_multi.Reset(Encode(
      "http://example.com/", "cc", "HASH", MultiUrl("a.css", "b.css"), "css"));
  EXPECT_TRUE(rewrite_driver()->DecodeUrl(gurl_multi, &urls));
  EXPECT_EQ(2, urls.size());
  EXPECT_EQ("http://example.com/a.css", urls[0]);
  EXPECT_EQ("http://example.com/b.css", urls[1]);
}

// Test to make sure we do not put in extra things into the cache.
// This is using the CSS rewriter, which caches the output.
TEST_F(RewriteDriverTest, TestCacheUse) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString css_minified_url =
      Encode(kTestDomain, RewriteOptions::kCssFilterId,
             hasher()->Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_minified_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
}

// Extension of above with cache invalidation.
TEST_F(RewriteDriverTest, TestCacheUseWithInvalidation) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString css_minified_url =
      Encode(kTestDomain, RewriteOptions::kCssFilterId,
             hasher()->Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_minified_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result.
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  // Set cache invalidation timestamp (to now, so that response date header is
  // in the "past") and load. Should get inserted again.
  ClearStats();
  int64 now_ms = timer()->NowMs();
  options()->ClearSignatureForTesting();
  options()->set_cache_invalidation_timestamp(now_ms);
  options()->ComputeSignature();
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  // We expect: identical input a new rname entry (its version # changed),
  // and the output which may not may not auto-advance due to MockTimer
  // black magic.
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(2, lru_cache()->num_identical_reinserts());
}

TEST_F(RewriteDriverTest, TestCacheUseWithUrlPatternAllInvalidation) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString css_minified_url =
      Encode(kTestDomain, RewriteOptions::kCssFilterId,
             hasher()->Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_minified_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result.
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  ClearStats();
  int64 now_ms = timer()->NowMs();
  options()->ClearSignatureForTesting();
  // Set cache invalidation (to now) for all URLs with "a.css" and also
  // invalidate all metadata (the last 'false' argument below).
  options()->AddUrlCacheInvalidationEntry("*a.css*", now_ms, false);
  options()->ComputeSignature();
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  // We expect: identical input, a new rewrite entry (its version # changed),
  // and the output which may not may not auto-advance due to MockTimer black
  // magic.
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(2, lru_cache()->num_identical_reinserts());
}

TEST_F(RewriteDriverTest, TestCacheUseWithUrlPatternOnlyInvalidation) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString css_minified_url =
      Encode(kTestDomain, RewriteOptions::kCssFilterId,
             hasher()->Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_minified_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result.
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  ClearStats();
  int64 now_ms = timer()->NowMs();
  options()->ClearSignatureForTesting();
  // Set cache invalidation (to now) for all URLs with "a.css". Does not
  // invalidate any metadata (the last 'true' argument below).
  options()->AddUrlCacheInvalidationEntry("*a.css*", now_ms, true);
  options()->ComputeSignature();
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  // The output rewritten URL is invalidated, the input is also invalidated, and
  // fetched again.  The rewrite entry does not change, and gets reinserted.
  // Thus, we have identical input, rname entry, and the output.
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(3, lru_cache()->num_identical_reinserts());
}

TEST_F(RewriteDriverTest, TestCacheUseWithRewrittenUrlAllInvalidation) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString css_minified_url =
      Encode(kTestDomain, RewriteOptions::kCssFilterId,
             hasher()->Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_minified_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result.
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  ClearStats();
  int64 now_ms = timer()->NowMs();
  options()->ClearSignatureForTesting();
  // Set a URL cache invalidation entry for output URL.  Original input URL is
  // not affected.  Also invalidate all metadata (the
  // ignores_metadata_and_pcache argument being false below).
  options()->AddUrlCacheInvalidationEntry(
      css_minified_url, now_ms, false /* ignores_metadata_and_pcache */);
  options()->ComputeSignature();
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  // We expect:  a new rewrite entry (its version # changed), and identical
  // output.
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_identical_reinserts());
}

TEST_F(RewriteDriverTest, TestCacheUseWithRewrittenUrlOnlyInvalidation) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString css_minified_url =
      Encode(kTestDomain, RewriteOptions::kCssFilterId,
             hasher()->Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_minified_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result.
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  ClearStats();
  int64 now_ms = timer()->NowMs();
  options()->ClearSignatureForTesting();
  // Set cache invalidation (to now) for output URL.  Original input URL is not
  // affected.  Does not invalidate any metadata (the last 'true' argument
  // below).
  options()->AddUrlCacheInvalidationEntry(css_minified_url, now_ms, true);
  options()->ComputeSignature();
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  // We expect:  identical rewrite entry and output.
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(2, lru_cache()->num_identical_reinserts());
}

TEST_F(RewriteDriverTest, TestCacheUseWithOriginalUrlInvalidation) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  const char kMinCss[] = "*{display:none}";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString css_minified_url =
      Encode(kTestDomain, RewriteOptions::kCssFilterId,
             hasher()->Hash(kMinCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_minified_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result.
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  // Warm load. This one should not change the number of inserts at all
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  ClearStats();
  int64 now_ms = timer()->NowMs();
  options()->ClearSignatureForTesting();
  // Set cache invalidation (to now) for input URL.  Rewritten output URL is not
  // affected.  So there will be no cache inserts or reinserts.
  // Note:  Whether we invalidate all metadata (the last argument below) is
  // immaterial in this test.
  options()->AddUrlCacheInvalidationEntry("http://test.com/a.css", now_ms,
                                          false);
  options()->ComputeSignature();
  EXPECT_TRUE(TryFetchResource(css_minified_url));
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
}

// Similar to TestCacheUse, but with cache-extender which reconstructs on the
// fly.
TEST_F(RewriteDriverTest, TestCacheUseOnTheFly) {
  AddFilter(RewriteOptions::kExtendCacheCss);

  const char kCss[] = "* { display: none; }";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString cache_extended_url =
      Encode(kTestDomain, RewriteOptions::kCacheExtenderId,
             hasher()->Hash(kCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(cache_extended_url));

  // We should have 2 things inserted:
  // 1) the source data
  // 2) the rname entry for the result (only in sync)
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(2, cold_num_inserts);

  // Warm load. This one re-inserts in the rname entry, without changing it.
  EXPECT_TRUE(TryFetchResource(cache_extended_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_identical_reinserts());
}

// Verifies that the computed rewrite delay agrees with expectations
// depending on the configuration of constituent delay variables.
TEST_F(RewriteDriverTest, TestComputeCurrentFlushWindowRewriteDelayMs) {
  options()->set_rewrite_deadline_ms(1000);

  // "Start" a parse to configure the start time in the driver.
  ASSERT_TRUE(rewrite_driver()->StartParseId("http://site.com/",
                                             "compute_flush_window_test",
                                             kContentTypeHtml));

  // The per-page deadline is initially unconfigured.
  EXPECT_EQ(1000, GetFlushTimeout());

  // If the per-page deadline is less than the per-flush window timeout,
  // the per-page deadline is returned.
  rewrite_driver()->set_max_page_processing_delay_ms(500);
  EXPECT_EQ(500, GetFlushTimeout());

  // If the per-page deadline exceeds the per-flush window timeout, the flush
  // timeout is returned.
  rewrite_driver()->set_max_page_processing_delay_ms(1750);
  EXPECT_EQ(1000, GetFlushTimeout());

  // If we advance mock time to leave less than a flush window timeout remaining
  // against the page deadline, the appropriate page deadline difference is
  // returned.
  SetTimeMs(start_time_ms() + 1000);
  EXPECT_EQ(750, GetFlushTimeout());  // 1750 - 1000

  // If we advance mock time beyond the per-page limit, a value of 1 is
  // returned. (This is required since values <= 0 are interpreted by internal
  // timeout functions as unlimited.)
  SetTimeMs(start_time_ms() + 2000);
  EXPECT_EQ(1, GetFlushTimeout());

  rewrite_driver()->FinishParse();
}

// Extension of above with cache invalidation.
TEST_F(RewriteDriverTest, TestCacheUseOnTheFlyWithInvalidation) {
  AddFilter(RewriteOptions::kExtendCacheCss);

  const char kCss[] = "* { display: none; }";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);

  GoogleString cache_extended_url =
      Encode(kTestDomain, RewriteOptions::kCacheExtenderId,
             hasher()->Hash(kCss), "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(cache_extended_url));

  // We should have 2 things inserted:
  // 1) the source data
  // 2) the rname entry for the result
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(2, cold_num_inserts);

  // Warm load. This one re-inserts in the rname entry, without changing it.
  EXPECT_TRUE(TryFetchResource(cache_extended_url));
  EXPECT_EQ(cold_num_inserts, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_identical_reinserts());

  // Set cache invalidation timestamp (to now, so that response date header is
  // in the "past") and load.
  ClearStats();
  int64 now_ms = timer()->NowMs();
  options()->ClearSignatureForTesting();
  options()->set_cache_invalidation_timestamp(now_ms);
  options()->ComputeSignature();
  EXPECT_TRUE(TryFetchResource(cache_extended_url));
  // We expect: input re-insert, new metadata key
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_identical_reinserts());
}

TEST_F(RewriteDriverTest, BaseTags) {
  // Starting the parse, the base-tag will be derived from the html url.
  ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));
  rewrite_driver()->Flush();
  EXPECT_EQ("http://example.com/index.html", BaseUrlSpec());

  // If we then encounter a base tag, that will become the new base.
  rewrite_driver()->ParseText("<base href='http://new.example.com/subdir/'>");
  rewrite_driver()->Flush();
  EXPECT_EQ(0, message_handler()->TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());

  // A second base tag will be ignored, and an info message will be printed.
  rewrite_driver()->ParseText("<base href=http://second.example.com/subdir2>");
  rewrite_driver()->Flush();
  EXPECT_EQ(1, message_handler()->TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());

  // Restart the parse with a new URL and we start fresh.
  rewrite_driver()->FinishParse();
  ASSERT_TRUE(rewrite_driver()->StartParse(
      "http://restart.example.com/index.html"));
  rewrite_driver()->Flush();
  EXPECT_EQ("http://restart.example.com/index.html", BaseUrlSpec());

  // We should be able to reset again.
  rewrite_driver()->ParseText("<base href='http://new.example.com/subdir/'>");
  rewrite_driver()->Flush();
  EXPECT_EQ(1, message_handler()->TotalMessages());
  EXPECT_EQ("http://new.example.com/subdir/", BaseUrlSpec());
}

TEST_F(RewriteDriverTest, RelativeBaseTag) {
  // Starting the parse, the base-tag will be derived from the html url.
  ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));
  rewrite_driver()->ParseText("<base href='subdir/'>");
  rewrite_driver()->Flush();
  EXPECT_EQ(0, message_handler()->TotalMessages());
  EXPECT_EQ("http://example.com/subdir/", BaseUrlSpec());
}

TEST_F(RewriteDriverTest, InvalidBaseTag) {
  // Encountering an invalid base tag should be ignored (except info message).
  ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));

  // Note: Even nonsensical protocols must be accepted as base URLs.
  rewrite_driver()->ParseText("<base href='slwly:example.com/subdir'>");
  rewrite_driver()->Flush();
  EXPECT_EQ(0, message_handler()->TotalMessages());
  EXPECT_EQ("slwly:example.com/subdir", BaseUrlSpec());

  // Reasonable base URLs following that do not change it.
  rewrite_driver()->ParseText("<base href='http://example.com/absolute/'>");
  rewrite_driver()->Flush();
  EXPECT_EQ("slwly:example.com/subdir", BaseUrlSpec());
}

// The TestUrlNamer produces a url like below which is too long.
// http://cdn.com/http/base.example.com/http/unmapped.example.com/dir/test.jpg.pagespeed.xy.#.     NOLINT
TEST_F(RewriteDriverTest, CreateOutputResourceTooLongSeparateBase) {
  SetUseTestUrlNamer(true);
  OutputResourcePtr resource;

  options()->set_max_url_size(94);
  resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
      "http://mapped.example.com/dir/",
      "http://unmapped.example.com/dir/",
      "http://base.example.com/dir/",
      "xy",
      "test.jpg",
      kRewrittenResource));
  EXPECT_TRUE(NULL == resource.get());

  options()->set_max_url_size(95);
  resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
      "http://mapped.example.com/dir/",
      "http://unmapped.example.com/dir/",
      "http://base.example.com/dir/",
      "xy",
      "test.jpg",
      kRewrittenResource));
  EXPECT_TRUE(NULL != resource.get());
}

TEST_F(RewriteDriverTest, CreateOutputResourceTooLong) {
  const OutputResourceKind resource_kinds[] = {
    kRewrittenResource,
    kOnTheFlyResource,
    kOutlinedResource,
  };

  // short_path.size() < options()->max_url_size() < long_path.size()
  GoogleString short_path = "http://www.example.com/dir/";
  GoogleString long_path = short_path;
  for (int i = 0; 2 * i < options()->max_url_size(); ++i) {
    long_path += "z/";
  }

  // short_name.size() < options()->max_url_segment_size() < long_name.size()
  GoogleString short_name = "foo.html";
  GoogleString long_name =
      StrCat("foo.html?",
             GoogleString(options()->max_url_segment_size() + 1, 'z'));

  GoogleString dummy_filter_id = "xy";

  OutputResourcePtr resource;
  for (int k = 0; k < arraysize(resource_kinds); ++k) {
    // Short name should always succeed at creating new resource.
    resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
        short_path, dummy_filter_id, short_name, resource_kinds[k]));
    EXPECT_TRUE(NULL != resource.get());

    // Long leaf-name should always fail at creating new resource.
    resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
        short_path, dummy_filter_id, long_name, resource_kinds[k]));
    EXPECT_TRUE(NULL == resource.get());

    // Long total URL length should always fail at creating new resource.
    resource.reset(rewrite_driver()->CreateOutputResourceWithPath(
        long_path, dummy_filter_id, short_name, resource_kinds[k]));
    EXPECT_TRUE(NULL == resource.get());
  }
}

TEST_F(RewriteDriverTest, MultipleDomains) {
  rewrite_driver()->AddFilters();

  // Make sure we authorize domains for resources properly. This is a regression
  // test for where loading things from a domain would prevent loads from an
  // another domain from the same RewriteDriver.

  const char kCss[] = "* { display: none; }";
  const char kAltDomain[] = "http://www.example.co.uk/";
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "a.css"), kContentTypeCss,
                                kCss, 100);
  SetResponseWithDefaultHeaders(StrCat(kAltDomain, "b.css"), kContentTypeCss,
                                kCss, 100);

  GoogleString rewritten1 = Encode(kTestDomain,
                                   RewriteOptions::kCacheExtenderId,
                                   hasher()->Hash(kCss), "a.css", "css");

  GoogleString rewritten2 = Encode(kAltDomain, RewriteOptions::kCacheExtenderId,
                                   hasher()->Hash(kCss), "b.css", "css");

  EXPECT_TRUE(TryFetchResource(rewritten1));
  ClearRewriteDriver();
  EXPECT_TRUE(TryFetchResource(rewritten2));
}

TEST_F(RewriteDriverTest, ResourceCharset) {
  // Make sure we properly pick up the charset into a resource on read.
  const char kUrl[] = "http://www.example.com/foo.css";
  ResponseHeaders resource_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &resource_headers);
  resource_headers.Replace(HttpAttributes::kContentType,
                           "text/css; charset=koi8-r");

  const char kContents[] = "\xF5\xD2\xC1!";  // Ура!
  SetFetchResponse(kUrl, resource_headers, kContents);

  // We do this twice to make sure the cached version is OK, too.
  for (int round = 0; round < 2; ++round) {
    ResourcePtr resource(
        rewrite_driver()->CreateInputResourceAbsoluteUnchecked(kUrl));
    MockResourceCallback mock_callback(resource, factory()->thread_system());
    ASSERT_TRUE(resource.get() != NULL);
    resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                        rewrite_driver()->request_context(),
                        &mock_callback);
    EXPECT_TRUE(mock_callback.done());
    EXPECT_TRUE(mock_callback.success());
    EXPECT_EQ(kContents, resource->contents());
    ASSERT_TRUE(resource->type() != NULL);
    EXPECT_EQ(ContentType::kCss, resource->type()->type());
    EXPECT_STREQ("koi8-r", resource->charset());
  }
}

// Test caching behavior for normal UrlInputResources.
// This is the base case that LoadResourcesFromFiles below contrasts with.
TEST_F(RewriteDriverTest, LoadResourcesFromTheWeb) {
  rewrite_driver()->AddFilters();

  const char kStaticUrlPrefix[] = "http://www.example.com/";
  const char kResourceName[ ]= "foo.css";
  GoogleString resource_url = StrCat(kStaticUrlPrefix, kResourceName);
  const char kResourceContents1[] = "body { background: red; }";
  const char kResourceContents2[] = "body { background: blue; }";
  ResponseHeaders resource_headers;
  // This sets 1 year cache lifetime :/ TODO(sligocki): Shorten this.
  SetDefaultLongCacheHeaders(&kContentTypeCss, &resource_headers);
  // Clear the Etag and Last-Modified headers since this
  // SetDefaultLongCacheHeaders sets their value to constants which don't change
  // when their value is updated.
  resource_headers.RemoveAll(HttpAttributes::kEtag);
  resource_headers.RemoveAll(HttpAttributes::kLastModified);

  // Set the fetch value.
  SetFetchResponse(resource_url, resource_headers, kResourceContents1);
  // Make sure file can be loaded. Note this cannot be loaded through the
  // mock_url_fetcher, because it has not been set in that fetcher.
  ResourcePtr resource(
      rewrite_driver()->CreateInputResourceAbsoluteUnchecked(resource_url));
  MockResourceCallback mock_callback(resource, factory()->thread_system());
  ASSERT_TRUE(resource.get() != NULL);
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &mock_callback);
  EXPECT_TRUE(mock_callback.done());
  EXPECT_TRUE(mock_callback.success());
  EXPECT_EQ(kResourceContents1, resource->contents());
  // TODO(sligocki): Check it was cached.

  // Change the fetch value.
  SetFetchResponse(resource_url, resource_headers, kResourceContents2);
  // Check that the resource loads cached.
  ResourcePtr resource2(
      rewrite_driver()->CreateInputResourceAbsoluteUnchecked(resource_url));
  MockResourceCallback mock_callback2(resource2, factory()->thread_system());
  ASSERT_TRUE(resource2.get() != NULL);
  resource2->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       rewrite_driver()->request_context(),
                       &mock_callback2);
  EXPECT_TRUE(mock_callback2.done());
  EXPECT_TRUE(mock_callback2.success());
  EXPECT_EQ(kResourceContents1, resource2->contents());

  // Advance timer and check that the resource loads updated.
  AdvanceTimeMs(10 * Timer::kYearMs);

  // Check that the resource loads updated.
  ResourcePtr resource3(
      rewrite_driver()->CreateInputResourceAbsoluteUnchecked(resource_url));
  MockResourceCallback mock_callback3(resource3, factory()->thread_system());
  ASSERT_TRUE(resource3.get() != NULL);
  resource3->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       rewrite_driver()->request_context(),
                       &mock_callback3);
  EXPECT_TRUE(mock_callback3.done());
  EXPECT_EQ(kResourceContents2, resource3->contents());
}

// Test that we successfully load specified resources from files and that
// file resources have the appropriate properties, such as being loaded from
// file every time they are fetched (not being cached).
TEST_F(RewriteDriverTest, LoadResourcesFromFiles) {
  rewrite_driver()->AddFilters();

  const char kStaticUrlPrefix[] = "http://www.example.com/static/";
  const char kStaticFilenamePrefix[] = "/htmlcontent/static/";
  const char kResourceName[ ]= "foo.css";
  GoogleString resource_filename = StrCat(kStaticFilenamePrefix, kResourceName);
  GoogleString resource_url = StrCat(kStaticUrlPrefix, kResourceName);
  const char kResourceContents1[] = "body { background: red; }";
  const char kResourceContents2[] = "body { background: blue; }";

  // Tell RewriteDriver to associate static URLs with filenames.
  options()->file_load_policy()->Associate(kStaticUrlPrefix,
                                           kStaticFilenamePrefix);

  // Write a file.
  WriteFile(resource_filename.c_str(), kResourceContents1);
  // Make sure file can be loaded. Note this cannot be loaded through the
  // mock_url_fetcher, because it has not been set in that fetcher.
  ResourcePtr resource(
      rewrite_driver()->CreateInputResourceAbsoluteUnchecked(resource_url));
  ASSERT_TRUE(resource.get() != NULL);
  EXPECT_EQ(&kContentTypeCss, resource->type());
  MockResourceCallback mock_callback(resource, factory()->thread_system());
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &mock_callback);
  EXPECT_TRUE(mock_callback.done());
  EXPECT_TRUE(mock_callback.success());
  EXPECT_EQ(kResourceContents1, resource->contents());
  // TODO(sligocki): Check it wasn't cached.

  // Change the file.
  WriteFile(resource_filename.c_str(), kResourceContents2);
  // Make sure the resource loads updated.
  ResourcePtr resource2(
      rewrite_driver()->CreateInputResourceAbsoluteUnchecked(resource_url));
  ASSERT_TRUE(resource2.get() != NULL);
  EXPECT_EQ(&kContentTypeCss, resource2->type());
  MockResourceCallback mock_callback2(resource2, factory()->thread_system());
  resource2->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       rewrite_driver()->request_context(),
                       &mock_callback2);
  EXPECT_TRUE(mock_callback2.done());
  EXPECT_TRUE(mock_callback2.success());
  EXPECT_EQ(kResourceContents2, resource2->contents());
}

// Make sure the content-type is set correctly, even for URLs with queries.
// http://code.google.com/p/modpagespeed/issues/detail?id=405
TEST_F(RewriteDriverTest, LoadResourcesContentType) {
  rewrite_driver()->AddFilters();

  // Tell RewriteDriver to associate static URLs with filenames.
  options()->file_load_policy()->Associate("http://www.example.com/static/",
                                           "/htmlcontent/static/");

  // Write file with readable extension.
  WriteFile("/htmlcontent/foo.js", "");
  // Load the file with a query param (add .css at the end of the param just
  // for optimal trickyness).
  ResourcePtr resource(rewrite_driver()->CreateInputResourceAbsoluteUnchecked(
      "http://www.example.com/static/foo.js?version=2.css"));
  EXPECT_TRUE(resource.get() != NULL);
  EXPECT_EQ(&kContentTypeJavascript, resource->type());

  // Write file with bogus extension.
  WriteFile("/htmlcontent/bar.bogus", "");
  // Load it normally.
  ResourcePtr resource2(rewrite_driver()->CreateInputResourceAbsoluteUnchecked(
      "http://www.example.com/static/bar.bogus"));
  EXPECT_TRUE(resource2.get() != NULL);
  EXPECT_TRUE(NULL == resource2->type());
}

TEST_F(RewriteDriverTest, ResolveAnchorUrl) {
  rewrite_driver()->AddFilters();
  ASSERT_TRUE(rewrite_driver()->StartParse("http://example.com/index.html"));
  GoogleUrl resolved(rewrite_driver()->base_url(), "#anchor");
  EXPECT_EQ("http://example.com/index.html#anchor", resolved.Spec());
  rewrite_driver()->FinishParse();
}

// A rewrite context that's not actually capable of rewriting -- we just need
// one to pass in to InfoAt in test below.
class MockRewriteContext : public SingleRewriteContext {
 public:
  explicit MockRewriteContext(RewriteDriver* driver) :
      SingleRewriteContext(driver, NULL, NULL) {}

  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output) {}
  virtual const char* id() const { return "mock"; }
  virtual OutputResourceKind kind() const { return kOnTheFlyResource; }
};

TEST_F(RewriteDriverTest, DiagnosticsWithPercent) {
  // Regression test for crash in InfoAt where location has %stuff in it.
  // (make sure it actually shows up first, though).
  int prev_log_level = logging::GetMinLogLevel();
  logging::SetMinLogLevel(logging::LOG_INFO);
  rewrite_driver()->AddFilters();
  MockRewriteContext context(rewrite_driver());
  ResourcePtr resource(rewrite_driver()->CreateInputResourceAbsoluteUnchecked(
      "http://www.example.com/%s%s%s%d%f"));
  ResourceSlotPtr slot(new FetchResourceSlot(resource));
  context.AddSlot(slot);
  rewrite_driver()->InfoAt(&context, "Just a test");
  logging::SetMinLogLevel(prev_log_level);
}

// Tests that we reject https URLs quickly.
TEST_F(RewriteDriverTest, RejectHttpsQuickly) {
  // Need to expressly authorize https even though we don't support it.
  options()->WriteableDomainLawyer()->AddDomain("https://*/",
                                                message_handler());
  AddFilter(RewriteOptions::kRewriteJavascript);

  // When we don't support https then we fail quickly and cleanly.
  factory()->mock_url_async_fetcher()->set_fetcher_supports_https(false);
  ValidateNoChanges("reject_https_quickly",
                    "<script src='https://example.com/a.js'></script>");
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // When we do support https the fetcher fails to find the resource.
  factory()->mock_url_async_fetcher()->set_fetcher_supports_https(true);
  SetFetchResponse404("https://example.com/a.js");
  ValidateNoChanges("reject_https_quickly",
                    "<script src='https://example.com/a.js'></script>");
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->failure_count());
}

// Test that CreateInputResource doesn't crash when handed a data url.
// This was causing a query of death in some circumstances.
TEST_F(RewriteDriverTest, RejectDataResourceGracefully) {
  MockRewriteContext context(rewrite_driver());
  GoogleUrl dataUrl("data:");
  ResourcePtr resource(rewrite_driver()->CreateInputResource(dataUrl));
  EXPECT_TRUE(resource.get() == NULL);
}

class ResponseHeadersCheckingFilter : public EmptyHtmlFilter {
 public:
  explicit ResponseHeadersCheckingFilter(RewriteDriver* driver)
      : driver_(driver),
        flush_occurred_(false) {
  }

  void CheckAccess() {
    EXPECT_TRUE(driver_->response_headers() != NULL);
    if (flush_occurred_) {
      EXPECT_TRUE(driver_->mutable_response_headers() == NULL);
    } else {
      EXPECT_EQ(driver_->mutable_response_headers(),
                driver_->response_headers());
    }
  }

  virtual void StartDocument() {
    flush_occurred_ = false;
    CheckAccess();
  }

  virtual void Flush() {
    CheckAccess();  // We still can access the mutable headers during Flush.
    flush_occurred_ = true;
  }

  virtual void StartElement(HtmlElement* element) { CheckAccess(); }
  virtual void EndElement(HtmlElement* element) { CheckAccess(); }
  virtual void EndDocument() { CheckAccess(); }

  virtual const char* Name() const { return "ResponseHeadersCheckingFilter"; }

 private:
  RewriteDriver* driver_;
  bool flush_occurred_;
};

class DetermineEnabledCheckingFilter : public EmptyHtmlFilter {
 public:
  DetermineEnabledCheckingFilter() :
    start_document_called_(false),
    enabled_value_(false) {}

  virtual void StartDocument() {
    start_document_called_ = true;
  }

  virtual void DetermineEnabled() {
    set_is_enabled(enabled_value_);
  }

  void SetEnabled(bool enabled_value) {
    enabled_value_ = enabled_value;
  }

  bool start_document_called() {
    return start_document_called_;
  }

  virtual const char* Name() const { return "DetermineEnabledCheckingFilter"; }

 private:
  bool start_document_called_;
  bool enabled_value_;
};

TEST_F(RewriteDriverTest, DetermineEnabledTest) {
  RewriteDriver* driver = rewrite_driver();
  DetermineEnabledCheckingFilter* filter =
      new DetermineEnabledCheckingFilter();
  driver->AddOwnedEarlyPreRenderFilter(filter);
  driver->StartParse("http://example.com/index.html");
  rewrite_driver()->ParseText("<div>");
  driver->Flush();
  EXPECT_FALSE(filter->start_document_called());
  rewrite_driver()->ParseText("</div>");
  driver->FinishParse();

  filter = new DetermineEnabledCheckingFilter();
  filter->SetEnabled(true);
  driver->AddOwnedEarlyPreRenderFilter(filter);
  driver->StartParse("http://example.com/index.html");
  rewrite_driver()->ParseText("<div>");
  driver->Flush();
  EXPECT_TRUE(filter->start_document_called());
  rewrite_driver()->ParseText("</div>");
  driver->FinishParse();
}

// Tests that we access driver->response_headers() before/after Flush(),
// and driver->mutable_response_headers() at only before Flush().
TEST_F(RewriteDriverTest, ResponseHeadersAccess) {
  RewriteDriver* driver = rewrite_driver();
  ResponseHeaders headers;
  driver->set_response_headers_ptr(&headers);
  driver->AddOwnedEarlyPreRenderFilter(new ResponseHeadersCheckingFilter(
      driver));
  driver->AddOwnedPostRenderFilter(new ResponseHeadersCheckingFilter(driver));

  // Starting the parse, the base-tag will be derived from the html url.
  ASSERT_TRUE(driver->StartParse("http://example.com/index.html"));
  rewrite_driver()->ParseText("<div>");
  driver->Flush();
  rewrite_driver()->ParseText("</div>");
  driver->FinishParse();
}

TEST_F(RewriteDriverTest, SetSessionFetcherTest) {
  AddFilter(RewriteOptions::kExtendCacheCss);

  const char kFetcher1Css[] = "Fetcher #1";
  const char kFetcher2Css[] = "Fetcher #2";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kFetcher1Css, 100);

  GoogleString url = Encode(kTestDomain, RewriteOptions::kCacheExtenderId,
                            hasher()->Hash(kFetcher1Css), "a.css", "css");

  // Fetch from default.
  GoogleString output;
  ResponseHeaders response_headers;
  EXPECT_TRUE(FetchResourceUrl(url, &output, &response_headers));
  EXPECT_STREQ(kFetcher1Css, output);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Load up a different file into a second fetcher.
  // We misappropriate the response_headers from previous fetch for simplicity.
  scoped_ptr<MockUrlFetcher> mock2(new MockUrlFetcher);
  mock2->SetResponse(AbsolutifyUrl("a.css"), response_headers, kFetcher2Css);

  // Switch over to new fetcher, making sure to set two of them to exercise
  // memory management. Note the synchronous mock fetcher we still have to
  // manage ourselves (as the RewriteDriver API is for async ones only).
  RewriteDriver* driver = rewrite_driver();
  driver->SetSessionFetcher(mock2.release());
  CountingUrlAsyncFetcher* counter =
      new CountingUrlAsyncFetcher(driver->async_fetcher());
  driver->SetSessionFetcher(counter);
  EXPECT_EQ(counter, driver->async_fetcher());

  // Note that FetchResourceUrl will call driver->Clear() so we cannot
  // access 'counter' past this point.
  lru_cache()->Clear();  // get rid of cached version of input
  EXPECT_TRUE(FetchResourceUrl(url, &output, &response_headers));
  EXPECT_STREQ(kFetcher2Css, output);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // As FetchResourceUrl has cleared the driver, further fetcher should
  // grab fetcher 1 version.
  lru_cache()->Clear();  // get rid of cached version of input
  EXPECT_TRUE(FetchResourceUrl(url, &output, &response_headers));
  EXPECT_STREQ(kFetcher1Css, output);
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

class WaitAsyncFetch : public StringAsyncFetch {
 public:
  WaitAsyncFetch(const RequestContextPtr& req, GoogleString* content,
                 ThreadSystem* thread_system)
      : StringAsyncFetch(req, content),
        sync_(thread_system) {
  }
  virtual ~WaitAsyncFetch() {}

  virtual void HandleDone(bool status) {
    StringAsyncFetch::HandleDone(status);
    sync_.Notify();
  }
  void Wait() { sync_.Wait(); }

 private:
  WorkerTestBase::SyncPoint sync_;
};

class InPlaceTest : public RewriteTestBase {
 protected:
  InPlaceTest() {}
  virtual ~InPlaceTest() {}

  bool FetchInPlaceResource(const StringPiece& url,
                            bool proxy_mode,
                            GoogleString* content,
                            ResponseHeaders* response) {
    GoogleUrl gurl(url);
    content->clear();
    WaitAsyncFetch async_fetch(CreateRequestContext(), content,
                               server_context()->thread_system());
    async_fetch.set_response_headers(response);
    rewrite_driver_->FetchInPlaceResource(gurl, proxy_mode,
                                          &async_fetch);
    async_fetch.Wait();

    // Make sure we let the rewrite complete, and also wait for the driver to be
    // idle so we can reuse it safely.
    rewrite_driver_->WaitForShutDown();
    rewrite_driver_->Clear();

    EXPECT_TRUE(async_fetch.done());
    return async_fetch.done() && async_fetch.success();
  }

  bool TryFetchInPlaceResource(const StringPiece& url,
                               bool proxy_mode) {
    GoogleString contents;
    ResponseHeaders response;
    return FetchInPlaceResource(url, proxy_mode, &contents, &response);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InPlaceTest);
};

TEST_F(InPlaceTest, FetchInPlaceResource) {
  AddFilter(RewriteOptions::kRewriteCss);

  GoogleString url = "http://example.com/foo.css";
  SetResponseWithDefaultHeaders(url, kContentTypeCss,
                                ".a { color: red; }", 100);

  // This will fail because cache is empty and we are not allowing HTTP fetch.
  EXPECT_FALSE(TryFetchInPlaceResource(url, false /* proxy_mode */));
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // Now we allow HTTP fetches and we expect success.
  EXPECT_TRUE(TryFetchInPlaceResource(url, true /* proxy_mode */));
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  // We insert both original and rewritten resources.
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // Now that we've loaded the resource into cache, we expect success.
  EXPECT_TRUE(TryFetchInPlaceResource(url, false /* proxy_mode */));
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();
}

TEST_F(RewriteDriverTest, CachePollutionWithWrongEncodingCharacter) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  SetResponseWithDefaultHeaders("dir/a.css", kContentTypeCss, kCss, 100);

  GoogleString css_wrong_url =
      "http://test.com/dir/B.a.css.pagespeed.cf.0.css";

  GoogleString correct_url = Encode(
      "dir/", RewriteOptions::kCssFilterId, hasher()->Hash(kCss),
      "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_wrong_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  EXPECT_EQ(HTTPCache::kFound,
            HttpBlockingFindStatus(StrCat(kTestDomain, correct_url),
                                   http_cache()));

  GoogleString input_html(CssLinkHref("dir/a.css"));
  GoogleString output_html(CssLinkHref(correct_url));
  ValidateExpected("wrong_encoding", input_html, output_html);
}

TEST_F(RewriteDriverTest, CachePollutionWithLowerCasedncodingCharacter) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  SetResponseWithDefaultHeaders("dir/a.css", kContentTypeCss, kCss, 100);

  GoogleString css_wrong_url =
      "http://test.com/dir/a.a.css.pagespeed.cf.0.css";

  GoogleString correct_url = Encode(
      "dir/", RewriteOptions::kCssFilterId, hasher()->Hash(kCss),
      "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_wrong_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  EXPECT_EQ(HTTPCache::kFound,
            HttpBlockingFindStatus(StrCat(kTestDomain, correct_url),
                                   http_cache()));

  GoogleString input_html(CssLinkHref("dir/a.css"));
  GoogleString output_html(CssLinkHref(correct_url));
  ValidateExpected("wrong_encoding", input_html, output_html);
}

TEST_F(RewriteDriverTest, CachePollutionWithExperimentId) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  SetResponseWithDefaultHeaders("dir/a.css", kContentTypeCss, kCss, 100);

  GoogleString css_wrong_url =
      "http://test.com/dir/A.a.css.pagespeed.b.cf.0.css";

  GoogleString correct_url = Encode(
      "dir/", RewriteOptions::kCssFilterId, hasher()->Hash(kCss),
      "a.css", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_wrong_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  EXPECT_EQ(HTTPCache::kFound,
            HttpBlockingFindStatus(StrCat(kTestDomain, correct_url),
                                   http_cache()));

  GoogleString input_html(CssLinkHref("dir/a.css"));
  GoogleString output_html(CssLinkHref(correct_url));
  ValidateExpected("wrong_encoding", input_html, output_html);
}

TEST_F(RewriteDriverTest, CachePollutionWithQueryParams) {
  AddFilter(RewriteOptions::kRewriteCss);

  const char kCss[] = "* { display: none; }";
  SetResponseWithDefaultHeaders("dir/a.css?ver=3", kContentTypeCss, kCss, 100);

  GoogleString css_wrong_url =
      "http://test.com/dir/A.a.css,qver%3D3.pagespeed.cf.0.css";

  GoogleString correct_url = Encode(
      "dir/", RewriteOptions::kCssFilterId, hasher()->Hash(kCss),
      "a.css?ver=3", "css");

  // Cold load.
  EXPECT_TRUE(TryFetchResource(css_wrong_url));

  // We should have 3 things inserted:
  // 1) the source data
  // 2) the result
  // 3) the rname entry for the result
  int cold_num_inserts = lru_cache()->num_inserts();
  EXPECT_EQ(3, cold_num_inserts);

  EXPECT_EQ(HTTPCache::kFound,
            HttpBlockingFindStatus(StrCat(kTestDomain, correct_url),
                                   http_cache()));

  GoogleString input_html(CssLinkHref("dir/a.css?ver=3"));
  GoogleString output_html(CssLinkHref(correct_url));
  ValidateExpected("wrong_encoding", input_html, output_html);
}

TEST_F(RewriteDriverTest, NoLoggingForImagesRewrittenInsideCss) {
  options()->set_image_inline_max_bytes(100000);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kExtendCacheImages);
  options()->EnableFilter(RewriteOptions::kRecompressPng);
  options()->set_always_rewrite_css(true);
  rewrite_driver_->AddFilters();

  GoogleString contents = "#a {background:url(1.png) ;}";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, contents, 100);
  AddFileToMockFetcher(StrCat(kTestDomain, "1.png"), kBikePngFile,
                       kContentTypePng, 100);

  GoogleString correct_url = Encode(
        "", RewriteOptions::kCssFilterId, hasher()->Hash(contents),
        "a.css", "css");

  GoogleString input_html(CssLinkHref("a.css"));
  GoogleString output_html(CssLinkHref(correct_url));

  ValidateExpected("no_logging_images_inside_css", input_html, output_html);
  LoggingInfo* logging_info = rewrite_driver_->log_record()->logging_info();
  ASSERT_EQ(1, logging_info->rewriter_info_size());
  EXPECT_EQ("cf", logging_info->rewriter_info(0).id());
}

TEST_F(RewriteDriverTest, DecodeMultiUrlsEncodesCorrectly) {
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kCombineCss);
  rewrite_driver()->AddFilters();

  const char kCss[] = "* { display: none; }";
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCss, 100);
  SetResponseWithDefaultHeaders("test/b.css", kContentTypeCss, kCss, 100);

  // Combine filters
  GoogleString multi_url = Encode(
      "", RewriteOptions::kCssFilterId, hasher()->Hash(kCss),
      "a.css+test,_b.css.pagespeed.cc.0.css", "css");
  EXPECT_TRUE(TryFetchResource(StrCat(kTestDomain, multi_url)));

  GoogleString input_html(
      StrCat(CssLinkHref("a.css"), CssLinkHref("test/b.css")));
  ParseUrl(kTestDomain, input_html);
  StringVector css_urls;
  CollectCssLinks("multi", output_buffer_, &css_urls);
  EXPECT_EQ(1UL, css_urls.size());
  EXPECT_EQ(multi_url, css_urls[0]);
}

// Records URL of the last img element it sees at point of RenderDone().
class RenderDoneCheckingFilter : public EmptyHtmlFilter {
 public:
  RenderDoneCheckingFilter() : element_(NULL) {}
  virtual ~RenderDoneCheckingFilter() {}
  const GoogleString& src() const { return src_; }

 protected:
  virtual void StartElement(HtmlElement* element) {
    if (element->keyword() == HtmlName::kImg) {
      element_ = element;
    }
  }

  virtual void RenderDone() {
    if (element_ != NULL) {
      const char* val = element_->AttributeValue(HtmlName::kSrc);
      src_ = (val != NULL ? val : "");
    }
  }

  virtual const char* Name() const { return "RenderDoneCheckingFilter"; }

 private:
  HtmlElement* element_;
  GoogleString src_;
  DISALLOW_COPY_AND_ASSIGN(RenderDoneCheckingFilter);
};

TEST_F(RewriteDriverTest, RenderDoneTest) {
  // Test to make sure RenderDone sees output of a pre-render filter.
  RewriteDriver* driver = rewrite_driver();
  RenderDoneCheckingFilter* filter =
      new RenderDoneCheckingFilter();
  driver->AddOwnedEarlyPreRenderFilter(filter);
  SetResponseWithDefaultHeaders("a.png", kContentTypePng, "PNGkinda", 100);
  AddFilter(RewriteOptions::kExtendCacheImages);

  driver->StartParse(kTestDomain);
  rewrite_driver()->ParseText("<img src=\"a.png\">");
  driver->FinishParse();
  EXPECT_EQ(Encode("", RewriteOptions::kCacheExtenderId, "0", "a.png", "png"),
            filter->src());
}

TEST_F(RewriteDriverTest, BlockingRewriteFlagTest) {
  RequestHeaders request_headers;
  RewriteDriver* driver = rewrite_driver();
  options()->ClearSignatureForTesting();
  options()->set_blocking_rewrite_key("blocking");
  options()->ComputeSignature();

  // case 1.
  TestBlockingRewrite(&request_headers, false, true);

  // case 2.
  request_headers.Add(HttpAttributes::kXPsaBlockingRewrite, "not-blocking");
  TestBlockingRewrite(&request_headers, false, true);

  // case 3.
  request_headers.Add(HttpAttributes::kXPsaBlockingRewrite, "blocking");
  TestBlockingRewrite(&request_headers, true, true);

  // case 4.
  request_headers.Add(HttpAttributes::kXPsaBlockingRewrite, "blocking");
  request_headers.Add(HttpAttributes::kXPsaBlockingRewriteMode, "junk");
  TestBlockingRewrite(&request_headers, true, true);

  // case 5.
  request_headers.Add(HttpAttributes::kXPsaBlockingRewrite, "blocking");
  request_headers.Add(HttpAttributes::kXPsaBlockingRewriteMode, "slow");
  TestBlockingRewrite(&request_headers, true, false);

  options()->ClearSignatureForTesting();
  options()->EnableBlockingRewriteForRefererUrlPattern("http://example.com");
  options()->ComputeSignature();

  // case 6.
  request_headers.Add(HttpAttributes::kReferer, "http://junk.com/");
  driver->EnableBlockingRewrite(&request_headers);
  TestBlockingRewrite(&request_headers, false, true);

  // case 7.
  request_headers.RemoveAll(HttpAttributes::kReferer);
  request_headers.Add(HttpAttributes::kReferer, "http://example.com");
  request_headers.Add(HttpAttributes::kXPsaBlockingRewriteMode, "junk");
  TestBlockingRewrite(&request_headers, true, true);

  // case 8.
  request_headers.RemoveAll(HttpAttributes::kReferer);
  request_headers.Add(HttpAttributes::kReferer, "http://example.com");
  request_headers.Add(HttpAttributes::kXPsaBlockingRewriteMode, "slow");
  TestBlockingRewrite(&request_headers, true, false);
}

TEST_F(RewriteDriverTest, PendingAsyncEventsTest) {
  RewriteDriver* driver = rewrite_driver();

  driver->set_fully_rewrite_on_flush(true);
  driver->set_fast_blocking_rewrite(true);
  TestPendingEventsIsDone(true);

  // Only when we are doing a slow blocking rewrite (waiting for async events),
  // IsDone() returns false for kWaitForCompletion.
  driver->set_fully_rewrite_on_flush(true);
  driver->set_fast_blocking_rewrite(false);
  TestPendingEventsIsDone(false);

  driver->set_fully_rewrite_on_flush(false);
  driver->set_fast_blocking_rewrite(true);
  TestPendingEventsIsDone(true);

  driver->set_fully_rewrite_on_flush(false);
  driver->set_fast_blocking_rewrite(false);
  TestPendingEventsIsDone(true);

  // Make sure we properly cleanup as well.
  TestPendingEventsDriverCleanup(false, false);
  TestPendingEventsDriverCleanup(false, true);
  TestPendingEventsDriverCleanup(true, false);
  TestPendingEventsDriverCleanup(true, true);
}

// Test classes created for using a managed rewrite driver, so that downstream
// caching behavior (especially cache purging) can be tested. Since managed
// rewrite drivers need their filters to be setup before the custom rewrite
// driver is constructed, we need these classes with specific SetUp methods
// for configuring the options.

// This class has ExtendCacheCss filter enabled and has possibility of
// purge requests for the html because of the resources not being
// rewritten in the very first go.
class DownstreamCacheWithPossiblePurgeTest : public RewriteDriverTest {
 protected:
  void SetUp() {
    options()->EnableFilter(RewriteOptions::kExtendCacheCss);
    SetUseManagedRewriteDrivers(true);
    RewriteDriverTest::SetUp();
  }
};

// This class has CollapseWhitespace filter enabled and has no possibility of
// purge requests for the html because the html will always get fully rewritten
// in the very first go.
class DownstreamCacheWithNoPossiblePurgeTest : public RewriteDriverTest {
 protected:
  void SetUp() {
    options()->EnableFilter(RewriteOptions::kCollapseWhitespace);
    SetUseManagedRewriteDrivers(true);
    RewriteDriverTest::SetUp();
  }
};

TEST_F(DownstreamCacheWithPossiblePurgeTest, DownstreamCacheEnabled) {
  SetUpOptionsForDownstreamCacheTesting("GET", "http://localhost:1234/purge");
  // Use a wait fetcher so that the response does not get a chance to get
  // rewritten.
  SetupWaitFetcher();
  // Since we want to call both FinishParse() and WaitForCompletion() (it's
  // inside CallFetcherCallbacksForDriver) on a managed rewrite driver,
  // we have to pin it, since otherwise FinishParse will drop our last
  // reference.
  rewrite_driver()->AddUserReference();
  SetupResponsesForDownstreamCacheTesting();
  // Setup request headers since the subsequent purge request needs this.
  RequestHeaders request_headers;
  rewrite_driver()->SetRequestHeaders(request_headers);
  ProcessHtmlForDownstreamCacheTesting();
  EXPECT_STREQ(kNonRewrittenCachableHtml, output_buffer_);
  // Since the response would now have been generated (without any rewriting,
  // because neither of the 2 resource fetches for a.css and b.css
  // would have completed), we allow the fetches to complete now.
  factory()->CallFetcherCallbacksForDriver(rewrite_driver());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // Now we want to permit fetches to go ahead once we let purge happen
  factory()->wait_url_async_fetcher()->SetPassThroughMode(true);
  rewrite_driver()->Cleanup();  // Drop our ref, to let purge go ahead.

  // We can actually check the result of flush already because
  // our fetcher is immediate.
  EXPECT_EQ(3, counting_url_async_fetcher()->fetch_count());
  EXPECT_STREQ("http://localhost:1234/purge/",
               counting_url_async_fetcher()->most_recent_fetched_url());
  EXPECT_EQ(1, factory()->rewrite_stats()->
                   downstream_cache_purge_attempts()->Get());
}

TEST_F(DownstreamCacheWithPossiblePurgeTest, DownstreamCacheDisabled) {
  SetUpOptionsForDownstreamCacheTesting("GET", "");
  // Use a wait fetcher so that the response does not get a chance to get
  // rewritten.
  SetupWaitFetcher();
  // Since we want to call both FinishParse() and WaitForCompletion() (it's
  // inside CallFetcherCallbacksForDriver) on a managed rewrite driver,
  // we have to pin it, since otherwise FinishParse will drop our last
  // reference.
  rewrite_driver()->AddUserReference();
  SetupResponsesForDownstreamCacheTesting();
  // Setup request headers since the subsequent purge request needs this.
  RequestHeaders request_headers;
  rewrite_driver()->SetRequestHeaders(request_headers);
  ProcessHtmlForDownstreamCacheTesting();
  EXPECT_STREQ(kNonRewrittenCachableHtml, output_buffer_);
  // Since the response would now have been generated (without any rewriting,
  // because neither of the 2 resource fetches for a.css and b.css
  // would have completed), we allow the fetches to complete now.
  factory()->CallFetcherCallbacksForDriver(rewrite_driver());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // The purge-request-fetch can be allowed to complete without any waiting.
  // Hence, we set the pass-through-mode to true.
  factory()->wait_url_async_fetcher()->SetPassThroughMode(true);
  rewrite_driver()->Cleanup();  // Drop our ref, to let any purge go ahead.

  // We expect no purges in this flow.
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  EXPECT_STREQ("http://test.com/test/b.css",
               counting_url_async_fetcher()->most_recent_fetched_url());
  EXPECT_EQ(0, factory()->rewrite_stats()->
                   downstream_cache_purge_attempts()->Get());
}

TEST_F(DownstreamCacheWithPossiblePurgeTest,
       DownstreamCache100PercentRewritten) {
  SetUpOptionsForDownstreamCacheTesting("GET", "http://localhost:1234/purge");
  // Do not use a wait fetcher here because we want both the fetches (for a.css
  // and b.css) and their rewrites to finish before the response is served out.
  SetupResponsesForDownstreamCacheTesting();
  // Setup request headers since the subsequent purge request needs this.
  RequestHeaders request_headers;
  rewrite_driver()->SetRequestHeaders(request_headers);
  ProcessHtmlForDownstreamCacheTesting();
  EXPECT_STREQ(kRewrittenCachableHtmlWithCacheExtension, output_buffer_);
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // We expect no purges in this flow.
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  EXPECT_STREQ("http://test.com/test/b.css",
               counting_url_async_fetcher()->most_recent_fetched_url());
  EXPECT_EQ(0, factory()->rewrite_stats()->
                   downstream_cache_purge_attempts()->Get());
}

TEST_F(DownstreamCacheWithNoPossiblePurgeTest, DownstreamCacheNoInitRewrites) {
  SetUpOptionsForDownstreamCacheTesting("GET", "http://localhost:1234/purge");
  // Use a wait fetcher so that the response does not get a chance to get
  // rewritten.
  SetupWaitFetcher();
  rewrite_driver()->AddUserReference();
  SetupResponsesForDownstreamCacheTesting();
  // Setup request headers since the subsequent purge request needs this.
  RequestHeaders request_headers;
  rewrite_driver()->SetRequestHeaders(request_headers);
  ProcessHtmlForDownstreamCacheTesting();
  EXPECT_STREQ(kRewrittenCachableHtmlWithCollapseWhitespace, output_buffer_);

  // Since only collapse-whitespace is enabled in this test, we do not expect
  // any fetches (or fetch callbacks for the wait fetcher) here.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // Release RewriteDriver and trigger any purge.
  rewrite_driver()->Cleanup();
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, factory()->rewrite_stats()->
                   downstream_cache_purge_attempts()->Get());
}

}  // namespace

}  // namespace net_instaweb
