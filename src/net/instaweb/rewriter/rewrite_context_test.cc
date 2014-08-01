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

#include "net/instaweb/rewriter/public/rewrite_context.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"  // for Code::kOK
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/rate_controller.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "net/instaweb/http/public/write_through_http_cache.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/fake_filter.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"  // for ResourcePtr, etc
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_context_test_base.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/simple_text_filter.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/worker_test_base.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/semantic_type.h"

namespace net_instaweb {

namespace {

// This value needs to be bigger than rewrite driver timeout;
// and it's useful while debugging for it to not be the driver
// timeout's multiple (so one can easily tell its occurrences
// from repetitions of the driver's timeout).
const int64 kRewriteDelayMs = 47;

class RewriteContextTest : public RewriteContextTestBase {
 protected:
  RewriteContextTest() {
    fetch_failures_ = statistics()->GetVariable(
        RewriteStats::kNumResourceFetchFailures);
    fetch_successes_ = statistics()->GetVariable(
        RewriteStats::kNumResourceFetchSuccesses);
  }

  void InitTrimFiltersSync(OutputResourceKind kind) {
    rewrite_driver()->AppendRewriteFilter(
        new TrimWhitespaceSyncFilter(kind, rewrite_driver()));
    rewrite_driver()->AddFilters();

    other_rewrite_driver()->AppendRewriteFilter(
        new TrimWhitespaceSyncFilter(kind, rewrite_driver()));
    other_rewrite_driver()->AddFilters();

    EnableDebug();
  }

  void InitTwoFilters(OutputResourceKind kind) {
    InitUpperFilter(kind, rewrite_driver());
    InitUpperFilter(kind, other_rewrite_driver());
    InitTrimFilters(kind);
  }

  void TrimOnTheFlyStart(GoogleString* input_html, GoogleString* output_html) {
    InitTrimFilters(kOnTheFlyResource);
    InitResources();

    // The first rewrite was successful because we got an 'instant' url
    // fetch, not because we did any cache lookups.
    *input_html = CssLinkHref("a.css");
    *output_html =
        StrCat(CssLinkHref(Encode("", "tw", "0", "a.css", "css")),
               DebugMessage("a.css"));
    ValidateExpected("trimmable", *input_html, *output_html);
    EXPECT_EQ(0, lru_cache()->num_hits());
    EXPECT_EQ(2, lru_cache()->num_misses());   // Metadata + input-resource.
    // We expect 2 inserts because it's an kOnTheFlyResource.
    EXPECT_EQ(2, lru_cache()->num_inserts());  // Metadata + input-resource.
    EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
    EXPECT_EQ(1, fetch_successes_->Get());
    EXPECT_EQ(0, fetch_failures_->Get());
    EXPECT_EQ(0, metadata_cache_info().num_repeated_rewrites());
    EXPECT_EQ(0, metadata_cache_info().num_disabled_rewrites());
    EXPECT_EQ(1, metadata_cache_info().num_misses());
    EXPECT_EQ(0, metadata_cache_info().num_revalidates());
    EXPECT_EQ(0, metadata_cache_info().num_hits());
    EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
    EXPECT_EQ(1, metadata_cache_info().num_successful_rewrites_on_miss());
    EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
    EXPECT_EQ(1, metadata_cache_info().num_rewrites_completed());
    ClearStats();

    // The second time we request this URL, we should find no additional
    // cache inserts or fetches.  The rewrite should complete using a
    // single cache hit for the metadata.  No cache misses will occur.
    ValidateExpected("trimmable", *input_html, *output_html);
    EXPECT_EQ(1, lru_cache()->num_hits());
    EXPECT_EQ(0, lru_cache()->num_misses());
    EXPECT_EQ(0, lru_cache()->num_inserts());
    EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
    EXPECT_EQ(0, metadata_cache_info().num_repeated_rewrites());
    EXPECT_EQ(0, metadata_cache_info().num_disabled_rewrites());
    EXPECT_EQ(0, metadata_cache_info().num_misses());
    EXPECT_EQ(0, metadata_cache_info().num_revalidates());
    EXPECT_EQ(1, metadata_cache_info().num_hits());
    EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
    EXPECT_EQ(0, metadata_cache_info().num_successful_rewrites_on_miss());
    EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
    EXPECT_EQ(1, metadata_cache_info().num_rewrites_completed());
    EXPECT_EQ(0, fetch_successes_->Get());  // no more fetches.
    EXPECT_EQ(0, fetch_failures_->Get());
    ClearStats();
  }

  int RewriteAndCountUnrewrittenCss(const GoogleString& id,
                                    const GoogleString& input_html) {
    Parse(id, input_html);
    CssLink::Vector css_links;
    GoogleString rewritten_html = output_buffer_;
    CollectCssLinks("collecting_links", rewritten_html, &css_links);
    int num_unrewritten_css = 0;
    for (int i = 0, n = css_links.size(); i < n; ++i) {
      if (css_links[i]->url_.find(".pagespeed.") == GoogleString::npos) {
        ++num_unrewritten_css;
      }
    }
    return num_unrewritten_css;
  }

  Variable* fetch_failures_;
  Variable* fetch_successes_;
};

}  // namespace

TEST_F(RewriteContextTest, TrimOnTheFlyOptimizable) {
  GoogleString input_html, output_html;
  TrimOnTheFlyStart(&input_html, &output_html);

  // The third time we request this URL, we've advanced time so that the origin
  // resource TTL has expired.  The data will be re-fetched, and the Date
  // corrected.   See url_input_resource.cc, AddToCache().  The http cache will
  // miss, but we'll re-insert.  We won't need to do any more rewrites because
  // the data did not actually change.
  AdvanceTimeMs(2 * kOriginTtlMs);
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(2, lru_cache()->num_hits());     // 1 expired hit, 1 valid hit.
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // re-inserts after expiration.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_expirations()->Get());
  EXPECT_EQ(0, metadata_cache_info().num_repeated_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_disabled_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_misses());
  EXPECT_EQ(1, metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, metadata_cache_info().num_hits());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(1, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(1, metadata_cache_info().num_rewrites_completed());
  EXPECT_EQ(1, fetch_successes_->Get());  // Must freshen.
  EXPECT_EQ(0, fetch_failures_->Get());
  ClearStats();

  // The fourth time we request this URL, the cache is in good shape despite
  // the expired date header from the origin.

  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
  EXPECT_EQ(0, metadata_cache_info().num_repeated_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_disabled_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(1, metadata_cache_info().num_hits());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(1, metadata_cache_info().num_rewrites_completed());
  EXPECT_EQ(0, fetch_successes_->Get());  // no more fetches.
  EXPECT_EQ(0, fetch_failures_->Get());
  ClearStats();

  // Induce a metadata cache flush by tweaking the options in way that
  // happens to be irrelevant for the filter applied.  We will
  // successfully rewrite, but we will not need to re-fetch.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kInlineImages);
  options()->ComputeSignature();
  ValidateExpected("trimmable_flushed_metadata", input_html, output_html);
  EXPECT_EQ(1, lru_cache()->num_hits());     // resource
  EXPECT_EQ(1, lru_cache()->num_misses());   // metadata
  EXPECT_EQ(1, lru_cache()->num_inserts());  // metadata
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
  EXPECT_EQ(0, metadata_cache_info().num_repeated_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_disabled_rewrites());
  EXPECT_EQ(1, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, metadata_cache_info().num_hits());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(1, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(1, metadata_cache_info().num_rewrites_completed());
  ClearStats();
  EXPECT_EQ(0, fetch_successes_->Get());  // no more fetches.
  EXPECT_EQ(0, fetch_failures_->Get());
}

TEST_F(RewriteContextTest, TrimOnTheFlyWithVaryCookie) {
  InitTrimFilters(kOnTheFlyResource);
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &response_headers);
  response_headers.Add(HttpAttributes::kVary, HttpAttributes::kCookie);
  SetFetchResponse(AbsolutifyUrl("a.css"), response_headers, " a ");

  // We cannot rewrite resources with Vary:Cookie in the response,
  // even if there was no cookie in the request.  It is conceivable to
  // implement a policy where Vary:Cookie is tolerated in the response
  // as long as there are no cookies in the request.  We would have to
  // ensure that we emitted the Vary:Cookie when serving the response
  // for the benefit of any other proxy caches.  The real challenge is
  // that the original domain of the resources might not be the same
  // as the domain of the HTML, so when serving HTML we would not know
  // whether the client had clear cookies for the resource fetch.  So
  // we could only do that if we knew the mapped resource domain was
  // cookieless, or the domain was the same as the HTML domain.
  //
  // Since the number of resources this affects on the internet is
  // very small -- less than 1% we will not be trying to tackle If we
  // do, this test will have to change to ValidateExpected against
  // CssLinkHref(Encode("", "tw", "0", "a.css", "css"), and we'd have
  // to also test that we didn't do the rewrite when there were
  // cookies on the HTML request.
  GoogleString input_html = CssLinkHref("a.css");
  ValidateNoChanges("vary_cookie", input_html);
}

TEST_F(RewriteContextTest, UnhealthyCacheNoHtmlRewrites) {
  lru_cache()->set_is_healthy(false);
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // We expect no changes in the HTML because the system gives up without
  // a healthy cache.  No cache lookups or fetches are attempted in this
  // flow, though if we need to handle a request for a .pagespeed. url
  // then we'll have to do fetches for that.
  GoogleString input_html(CssLinkHref("a.css"));
  ValidateNoChanges("trimmable", input_html);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_misses());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_hits());
}


TEST_F(RewriteContextTest, TrimOnTheFlyOptimizableCacheInvalidation) {
  GoogleString input_html, output_html;
  TrimOnTheFlyStart(&input_html, &output_html);

  // The third time we invalidate the cache and then request the URL.
  SetCacheInvalidationTimestamp();
  ValidateExpected("trimmable", input_html, output_html);

  rewrite_driver()->WaitForShutDown();
  // Setting the cache invalidation timestamp causes the partition key to change
  // and hence we get a cache miss (and insert) on the metadata.  The HTTPCache
  // is also invalidated and hence we have a fetch + insert of a.css.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, CacheInvalidatingOneOfTwoCssFiles) {
  EnableCachePurge();
  GoogleString input_html, output_html;
  TrimOnTheFlyStart(&input_html, &output_html);

  // Also include 'b.css' to input & output HTML.
  StrAppend(&input_html, CssLinkHref("b.css"));
  StrAppend(&output_html, CssLinkHref("b.css"));  // 'b.css' is not optimizable.

  // Invalidate the whole cache & re-run, generating metadata cache entries for
  // a.css and b.css.
  SetCacheInvalidationTimestamp();
  ValidateExpected("trimmable", input_html, output_html);
  ClearStats();

  // Wipe out a.css, but b.css's metadata stays intact.
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("a.css"),
                                      false /* ignores_metadata_and_pcache */);
  ValidateExpected("trimmable", input_html, output_html);

  // The invalidation of a.css does not actually change the cache key or remove
  // it from the cache; the metadata is invalidated after the cache hit.  Then
  // we must re-fetch a.css, which results in an cache hit HTTP cache hit.
  // There are no physical cache misses, but we do re-insert the same value
  // in the lru-cache after re-fetching a.css and seeing it didn't change.
  EXPECT_EQ(3, lru_cache()->num_hits());  // a.css, b.css, re-fetch of a.css.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());  // a.css was invalidated.
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Zooming into the metadata cache, we see a miss at this level due to
  // the invalidation record we wrote.
  EXPECT_EQ(0, metadata_cache_info().num_repeated_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_disabled_rewrites());
  EXPECT_EQ(1, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(1, metadata_cache_info().num_hits());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(1, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(2, metadata_cache_info().num_rewrites_completed());
}

TEST_F(RewriteContextTest,
       TrimOnTheFlyOptimizableThisUrlCacheInvalidationIgnoringMetadataCache) {
  EnableCachePurge();
  GoogleString input_html, output_html;
  TrimOnTheFlyStart(&input_html, &output_html);

  // The third time we do a 'strict' invalidation of cache for some other URL
  // and then request the URL.  This means we do not invalidate the metadata,
  // nor the HTTP cache entry for 'a.css'.
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("foo.bar"),
                                      true /* ignores_metadata_and_pcache */);
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  // We get a cache hit on the metadata.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The fourth time we do a 'strict' invalidation of cache for 'a.css' and then
  // request the URL.  This means we do not invalidate the metadata, but HTTP
  // cache entry for 'a.css' is invalidated.
  // Note:  Strict invalidation does not make sense for resources, since one
  // almost always wants to invalidate metadata for resources.  This test is for
  // completeness.
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("a.css"),
                                      true /* ignores_metadata_and_pcache */);
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  // We get a cache hit on the metadata.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimOnTheFlyOptimizableThisUrlCacheInvalidation) {
  EnableCachePurge();
  GoogleString input_html, output_html;
  TrimOnTheFlyStart(&input_html, &output_html);

  // The third time we do a 'strict' invalidation of cache for some other URL
  // and then request the URL.  This means we do not invalidate the metadata,
  // nor the HTTP cache entry for 'a.css'.
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("foo.bar"), true);
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  // We get a cache hit on the metadata.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The fourth time we do a 'complete' invalidation of cache for 'a.css' and
  // then request the URL.  This means in addition to invalidating the HTTP
  // cache entry for 'a.css', the metadata for that item is also invalidated,
  // though the metadata for 'b.css' is not disturbed.
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  int64 now_ms = http_cache()->timer()->NowMs();
  default_css_header.SetDateAndCaching(now_ms, kOriginTtlMs);
  default_css_header.ComputeCaching();
  SetFetchResponse(StrCat(kTestDomain, "a.css"), default_css_header, " new_a ");
  AdvanceTimeMs(1);
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("a.css"), false);
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  // The above invalidation did not cause the partition key to change, and so
  // we get an LRU cache hit.  However, the InputInfo is invalid because we
  // purged the cache, so we'll do a fetch, rewrite, and -reinsert.
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // metadata & http
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimOnTheFlyOptimizableUrlCacheInvalidation) {
  EnableCachePurge();
  GoogleString input_html, output_html;
  TrimOnTheFlyStart(&input_html, &output_html);

  // The third time we do a 'complete' invalidation of cache for some other URL
  // and then request the URL.  This means all metadata is invalidated, but the
  // HTTP cache entry for 'a.css' is not.
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("foo.bar*"), false);
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  // The above invalidation causes the partition key to change and hence
  // we get a cache miss (and insert) on the metadata.  The HTTPCache is not
  // invalidated and hence we get a hit there (and not fetch).
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimOnTheFlyNonOptimizable) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimOnTheFlyNonOptimizableCacheInvalidation) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The third time we invalidate the cache and then request the URL.
  SetCacheInvalidationTimestamp();
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  // Setting the cache invalidation timestamp causes the partition key to change
  // and hence we get a cache miss (and insert) on the metadata.  The HTTPCache
  // is also invalidated and hence we have a fetch, and re-insert of b.css
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest,
       TrimOnTheFlyNonOptimizableThisStrictUrlCacheInvalidation) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The third time we do a 'strict' invalidation of the cache for some URL
  // other than 'b.css' and then request the URL.  This means that metdata (and
  // in fact also HTTP cache for 'b.css') are not invalidated.
  // Note:  This is realistic since strict invalidation is what makes sense for
  // html.
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("foo.bar"), true);
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The fourth time we do a 'strict' invalidation of the caches for 'b.css' and
  // then request the URL.  This means we do not invalidate the metadata (but
  // HTTP cache is invalidated) and hence we get the cached failed rewrite from
  // metadata cache.
  // Note:  Strict invalidation does not make sense for resources, since one
  // almost always wants to invalidate metadata for resources.  This test is for
  // completeness.
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("b.css"), true);
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest,
       TrimOnTheFlyNonOptimizableThisRefUrlCacheInvalidation) {
  EnableCachePurge();
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The third time we do a 'strict' invalidation of the cache for some URL
  // other than 'b.css' and then request the URL.  This means that metdata (and
  // in fact also HTTP cache for 'b.css') are not invalidated.
  // Note:  This is realistic since strict invalidation is what makes sense for
  // html.
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("foo.bar"), true);
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The fourth time we invalidate the caches for 'b.css' and all metadata and
  // then request the URL.
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("b.css"), false);
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  // The above invalidation does not cause the partition key to
  // change, so we get an LRU cache hit, but we detect that it's
  // invalid and then re-insert the metadata.  The HTTPCache is also
  // invalidated and hence we have a fetch and new insert of b.css
  EXPECT_EQ(2, lru_cache()->num_hits());  // metadata (invalid) + b.css.
  EXPECT_EQ(1, metadata_cache_info().num_misses());
  EXPECT_EQ(1, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(1, metadata_cache_info().num_rewrites_completed());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimOnTheFlyNonOptimizableUrlCacheInvalidation) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was optimizable.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The third time we do a 'non-strict' (includes metadata) invalidation of
  // the cache for some URL other than 'b.css', invalidating just the
  // metadata for foo.bar, which has no effect.
  SetCacheInvalidationTimestampForUrl(AbsolutifyUrl("foo.bar"), false);
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  // Since enable_cache_purge is not true, the above invalidation results in a
  // signature change for metadata cache key.  Hence metadata is invalidated.
  EXPECT_EQ(1, lru_cache()->num_hits());     // http cache
  EXPECT_EQ(1, lru_cache()->num_misses());   // metadata
  EXPECT_EQ(1, lru_cache()->num_inserts());  // metadata
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// In this variant, we use the same whitespace trimmer, but we pretend that this
// is an expensive operation, so we want to cache the output resource.  This
// means we will do an extra cache insert on the first iteration for each input.
TEST_F(RewriteContextTest, TrimRewrittenOptimizable) {
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // The first rewrite was successful because we got an 'instant' url
  // fetch, not because we did any cache lookups. We'll have 2 cache
  // misses: one for the OutputPartitions, one for the fetch.  We
  // should need three items in the cache: the element, the resource
  // mapping (OutputPartitions) and the output resource.
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());  // 3 cause it's kRewrittenResource
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata (or output?).  No cache misses will occur.
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimRewrittenNonOptimizable) {
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // In this case, the resource is not optimizable.  The cache pattern is
  // exactly the same as when the resource was on-the-fly and optimizable.
  // We'll cache the successfully fetched resource, and the OutputPartitions
  // which indicates the unsuccessful optimization.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // partition
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimRepeatedOptimizable) {
  // Make sure two instances of the same link are handled properly,
  // when optimization succeeds.
  InitTrimFilters(kRewrittenResource);
  InitResources();
  ValidateExpected(
        "trimmable2", StrCat(CssLinkHref("a.css"), CssLinkHref("a.css")),
        StrCat(CssLinkHref(Encode("", "tw", "0", "a.css", "css")),
               CssLinkHref(Encode("", "tw", "0", "a.css", "css"))));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
}

TEST_F(RewriteContextTest, TrimRepeatedOptimizableDelayed) {
  // Make sure two instances of the same link are handled properly,
  // when optimization succeeds --- but fetches are slow.
  SetupWaitFetcher();
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // First time nothing happens by deadline.
  ValidateNoChanges("trimable2_notyet",
                    StrCat(CssLinkHref("a.css"), CssLinkHref("a.css")));
  EXPECT_EQ(0, metadata_cache_info().num_disabled_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_repeated_rewrites());
  EXPECT_EQ(1, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, metadata_cache_info().num_hits());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(0, metadata_cache_info().num_rewrites_completed());

  CallFetcherCallbacks();
  // Second time we get both rewritten right.
  ValidateExpected(
      "trimmable2_now",
      StrCat(CssLinkHref("a.css"), CssLinkHref("a.css")),
      StrCat(CssLinkHref(Encode("", "tw", "0", "a.css", "css")),
             CssLinkHref(Encode("", "tw", "0", "a.css", "css"))));
  EXPECT_EQ(0, metadata_cache_info().num_disabled_rewrites());
  // It's not deterministic whether the 2nd rewrite will get handled as
  // a hit or repeated rewrite of same content.
  EXPECT_EQ(2, (metadata_cache_info().num_repeated_rewrites() +
                metadata_cache_info().num_hits()));
  EXPECT_EQ(0, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(2, metadata_cache_info().num_rewrites_completed());

  EXPECT_EQ(1, trim_filter_->num_rewrites());
}

TEST_F(RewriteContextTest, TrimRepeatedNonOptimizable) {
  // Make sure two instances of the same link are handled properly --
  // when optimization fails.
  InitTrimFilters(kRewrittenResource);
  InitResources();
  ValidateNoChanges("notrimmable2",
                    StrCat(CssLinkHref("b.css"), CssLinkHref("b.css")));
}

TEST_F(RewriteContextTest, TrimRepeated404) {
  // Make sure two instances of the same link are handled properly --
  // when fetch fails.
  InitTrimFilters(kRewrittenResource);
  SetFetchResponse404("404.css");
  ValidateNoChanges("repeat404",
                    StrCat(CssLinkHref("404.css"), CssLinkHref("404.css")));
}

TEST_F(RewriteContextTest, FetchNonOptimizable) {
  options()->set_implicit_cache_ttl_ms(kOriginTtlMs + 100 * Timer::kSecondMs);
  InitTrimFilters(kRewrittenResource);
  InitResources();
  // We use MD5 hasher instead of mock hasher so that the we get the actual hash
  // of the content and not hash 0 always.
  UseMd5Hasher();

  // Fetching a resource that's not optimizable under the rewritten URL
  // should still work in a single-input case. This is important to be more
  // robust against JS URL manipulation.
  GoogleString output;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResourceUrl(Encode(kTestDomain, "tw", "0", "b.css", "css"),
                               &output, &headers));
  EXPECT_EQ("b", output);

  // Since this resource URL has a zero hash in it, this turns out to be a hash
  // mismatch. So, cache TTL should be short and the result should be marked
  // private.
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsBrowserCacheable());
  EXPECT_EQ(kOriginTtlMs + 0,
            headers.CacheExpirationTimeMs() - timer()->NowMs());

  // After 100 seconds, we'll only have 200 seconds left in the cache.
  headers.Clear();
  output.clear();
  AdvanceTimeMs(200 * Timer::kSecondMs);
  EXPECT_TRUE(FetchResourceUrl(Encode(kTestDomain, "tw", "0", "b.css", "css"),
                               &output, &headers));
  EXPECT_EQ("b", output);
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsBrowserCacheable());
  EXPECT_EQ(kOriginTtlMs - 200 * Timer::kSecondMs,
            headers.CacheExpirationTimeMs() - timer()->NowMs());
}

TEST_F(RewriteContextTest, FetchNonOptimizableWithPublicCaching) {
  options()->set_implicit_cache_ttl_ms(kOriginTtlMs + 100 * Timer::kSecondMs);
  options()->set_publicly_cache_mismatched_hashes_experimental(true);
  InitTrimFilters(kRewrittenResource);
  InitResources();
  // We use MD5 hasher instead of mock hasher so that the we get the actual hash
  // of the content and not hash 0 always.
  UseMd5Hasher();

  // Fetching a resource that's not optimizable under the rewritten URL
  // should still work in a single-input case. This is important to be more
  // robust against JS URL manipulation.
  GoogleString output;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResourceUrl(Encode(kTestDomain, "tw", "0", "b.css", "css"),
                               &output, &headers));
  EXPECT_EQ("b", output);

  // Since this resource URL has a zero hash in it, this turns out to be a hash
  // mismatch. However, the result should be proxy-cacheable and match the
  // origin TTL, because we have specified
  // set_publicly_cache_mismatched_hashes_experimental(true).
  EXPECT_TRUE(headers.IsProxyCacheable());
  EXPECT_EQ(kOriginTtlMs + 0,
            headers.CacheExpirationTimeMs() - timer()->NowMs());

  // After 200 seconds, we'll only have 200 seconds left in the cache.
  headers.Clear();
  output.clear();
  AdvanceTimeMs(200 * Timer::kSecondMs);
  EXPECT_TRUE(FetchResourceUrl(Encode(kTestDomain, "tw", "0", "b.css", "css"),
                               &output, &headers));
  EXPECT_EQ("b", output);
  EXPECT_TRUE(headers.IsProxyCacheable());
  // We really want this to be (kOriginTtlMs + 0), not to have the TTL decay
  // with elapased time.
  EXPECT_EQ(kOriginTtlMs - 200 * Timer::kSecondMs,
            headers.CacheExpirationTimeMs() - timer()->NowMs());
}

TEST_F(RewriteContextTest, FetchNonOptimizableLowTtl) {
  InitTrimFilters(kRewrittenResource);
  InitResources();
  // We use MD5 hasher instead of mock hasher so that the we get the actual hash
  // of the content and not hash 0 always.
  UseMd5Hasher();

  // Fetching a resource that's not optimizable under the rewritten URL
  // should still work in a single-input case. This is important to be more
  // robust against JS URL manipulation.
  GoogleString output;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResourceUrl(Encode(kTestDomain, "tw", "0", "e.css", "css"),
                               &output, &headers));
  EXPECT_EQ("e", output);

  ConstStringStarVector values;
  headers.Lookup(HttpAttributes::kCacheControl, &values);
  ASSERT_EQ(2, values.size());
  EXPECT_STREQ("max-age=5", *values[0]);
  EXPECT_STREQ("private", *values[1]);
  // Miss for request URL in http cache, metadata, and input resource. Insert
  // metadata, and input and output resource with correct hash in http cache.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // We do a second fetch to trigger the case where the output resource for the
  // URL from meta-data cache is found in http cache and hence we do not have
  // the original input at the time we try to fix headers.
  ClearStats();
  GoogleString output2;
  ResponseHeaders headers2;
  EXPECT_TRUE(FetchResourceUrl(Encode(kTestDomain, "tw", "0", "e.css", "css"),
                               &output2, &headers2));
  EXPECT_EQ("e", output2);

  ConstStringStarVector values2;
  headers2.Lookup(HttpAttributes::kCacheControl, &values2);
  ASSERT_EQ(2, values2.size());
  EXPECT_STREQ("max-age=5", *values2[0]);
  EXPECT_STREQ("private", *values2[1]);
  // Miss for request URL. Hit for metadata and output resource with correct
  // hash.
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, FetchNoSource) {
  InitTrimFilters(kRewrittenResource);
  SetFetchFailOnUnexpected(false);
  EXPECT_FALSE(
      TryFetchResource(Encode(kTestDomain, "tw", "0", "b.css", "css")));
}

// In the above tests, our URL fetcher called its callback directly, allowing
// the Rewrite to occur while the RewriteDriver was still attached.  In this
// run, we will delay the URL fetcher's callback so that the initial Rewrite
// will not take place until after the HTML has been flushed.
TEST_F(RewriteContextTest, TrimDelayed) {
  SetupWaitFetcher();
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  ValidateNoChanges("trimmable", CssLinkHref("a.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // Now we'll let the fetcher call its callbacks -- we'll see the
  // cache-inserts now, and the next rewrite will succeed.
  CallFetcherCallbacks();
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // 2 because it's kOnTheFlyResource
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();
}

TEST_F(RewriteContextTest, TrimFetchOnTheFly) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // The input URL is not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(FetchResource(kTestDomain, TrimWhitespaceRewriter::kFilterId,
                            "a.css", "css", &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());   // 1 because output is not saved
  EXPECT_EQ(2, lru_cache()->num_inserts());  // input, metadata
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();
  content.clear();

  // Now fetch it again.  This time the input URL is cached.
  EXPECT_TRUE(FetchResource(
      kTestDomain, TrimWhitespaceRewriter::kFilterId, "a.css", "css",
      &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TrimFetchRewritten) {
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // The input URL is not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(FetchResource(
      kTestDomain, TrimWhitespaceRewriter::kFilterId, "a.css", "css",
      &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(
      0, server_context()->rewrite_stats()->cached_resource_fetches()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  // TODO(jmarantz): have the lock-code return whether it had to wait to
  // get the lock or was able to acquire it immediately to avoid the
  // second cache lookup.
  EXPECT_EQ(3, lru_cache()->num_misses());  // output, metadata, input
  EXPECT_EQ(3, lru_cache()->num_inserts());  // output resource, input, metadata
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();
  content.clear();

  // Now fetch it again: the output URL is cached.
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResource(
      kTestDomain, TrimWhitespaceRewriter::kFilterId, "a.css", "css", &content,
      &headers));
  EXPECT_EQ("a", content);
  EXPECT_EQ(
      1, server_context()->rewrite_stats()->cached_resource_fetches()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // Make sure headers are nice and long.
  EXPECT_EQ(Timer::kYearMs, headers.cache_ttl_ms());
  EXPECT_TRUE(headers.IsProxyCacheable());
}

TEST_F(RewriteContextTest, TrimFetchSeedsCache) {
  // Make sure that rewriting on resource request also caches it for
  // future use for HTML.
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // The input URL is not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(FetchResource(
      kTestDomain, TrimWhitespaceRewriter::kFilterId, "a.css", "css",
      &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // output, metadata, input
  EXPECT_EQ(3, lru_cache()->num_inserts());  // output resource, input, metadata
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  ClearStats();

  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, lru_cache()->num_hits());  // Just metadata
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, trim_filter_->num_rewrites());  // cached.
}

TEST_F(RewriteContextTest, TrimFetchRewriteFailureSeedsCache) {
  // Make sure that rewriting on resource request also caches it for
  // future use for HTML, in the case where the rewrite fails.
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // The input URL is not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(FetchResource(
      kTestDomain, TrimWhitespaceRewriter::kFilterId, "b.css", "css",
      &content));
  EXPECT_EQ("b", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // output, metadata, input
  EXPECT_EQ(2, lru_cache()->num_inserts());  // input, metadata
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  ClearStats();

  ValidateNoChanges("nontrimmable", CssLinkHref("b.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());  // Just metadata
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, trim_filter_->num_rewrites());  // cached.
}

TEST_F(RewriteContextTest, TrimFetch404SeedsCache) {
  // Check that we cache a 404, and cache it for a reasonable amount of time.
  InitTrimFilters(kRewrittenResource);
  SetFetchResponse404("404.css");

  GoogleString content;
  EXPECT_FALSE(FetchResource(
      kTestDomain, TrimWhitespaceRewriter::kFilterId, "404.css", "css",
      &content));
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Should cache immediately...
  ValidateNoChanges("404", CssLinkHref("404.css"));
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // ... but not for too long.
  AdvanceTimeMs(Timer::kDayMs);
  ValidateNoChanges("404", CssLinkHref("404.css"));
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

// Verifies that rewriters can replace resource URLs without kicking off any
// fetching or caching.
TEST_F(RewriteContextTest, ClobberResourceUrlSync) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();
  GoogleString input_html(CssLinkHref("a_private.css"));
  // TODO(sligocki): Why is this an absolute URL?
  GoogleString output_html(CssLinkHref(
      Encode(kTestDomain, "ts", "0", "a_private.css", "css")));
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
}

// Verifies that when an HTML document references an uncacheable resource, that
// reference does not get modified.
TEST_F(RewriteContextTest, DoNotModifyReferencesToUncacheableResources) {
  InitTrimFilters(kRewrittenResource);
  InitResources();
  GoogleString input_html(CssLinkHref("a_private.css"));

  ValidateExpected("trimmable_but_private", input_html, input_html);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());  // partition, resource
  EXPECT_EQ(2, lru_cache()->num_inserts());  // partition, not-cacheable memo
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());  // the resource
  ClearStats();

  ValidateExpected("trimmable_but_private", input_html, input_html);
  EXPECT_EQ(1, lru_cache()->num_hits());  // not-cacheable memo
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  ValidateExpected("trimmable_but_private", input_html, input_html);
  EXPECT_EQ(1, lru_cache()->num_hits());  // not-cacheable memo
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// Verifies that when an HTML document references an uncacheable resource, that
// reference does get modified if cache ttl overriding is enabled.
TEST_F(RewriteContextTest, CacheTtlOverridingForPrivateResources) {
  FetcherUpdateDateHeaders();
  int64 ttl_ms = 600 * 1000;
  // Start with overriding caching for a wildcard pattern that does not match
  // the css url.
  options()->AddOverrideCacheTtl("*b_private*");
  options()->set_override_caching_ttl_ms(ttl_ms);
  InitTrimFilters(kRewrittenResource);
  InitResources();

  GoogleString input_html(CssLinkHref("a_private.css"));
  ValidateNoChanges("trimmable_not_overridden", input_html);
  ClearStats();

  // Now override caching for a pattern that matches the css url.
  options()->ClearSignatureForTesting();
  options()->AddOverrideCacheTtl("*a_private*");
  server_context()->ComputeSignature(options());

  GoogleString output_html(CssLinkHref(
      Encode("", "tw", "0", "a_private.css", "css")));

  // The private resource gets rewritten.
  ValidateExpected("trimmable_but_private", input_html, output_html);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  AdvanceTimeMs(5 * 1000);

  // Advance the timer by 5 seconds. Gets rewritten again with no extra fetches.
  ValidateExpected("trimmable_but_private", input_html, output_html);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  AdvanceTimeMs((ttl_ms * 4) / 5);
  // Advance past the freshening threshold. The resource gets freshened and we
  // update the metadata cache.
  ValidateExpected("trimmable_but_private", input_html, output_html);
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // Send another request after freshening. Succeeds without any extra fetches.
  ValidateExpected("trimmable_but_private", input_html, output_html);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  AdvanceTimeMs(2 * ttl_ms);
  // Advance past expiry. We fetch the resource and update the HTTPCache and
  // metadata cache.
  ValidateExpected("trimmable_but_private", input_html, output_html);
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  SetupWaitFetcher();
  AdvanceTimeMs(2 * ttl_ms);
  // Advance past expiry. We fetch the resource and update the HTTPCache and
  // metadata cache.
  ValidateExpected("trimmable_but_private", input_html, input_html);
  CallFetcherCallbacks();
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

// Make sure that cache-control: no-transform is honored.
TEST_F(RewriteContextTest, HonorNoTransform) {
  InitTrimFilters(kRewrittenResource);
  InitResources();
  GoogleString content;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResource(kTestDomain, TrimWhitespaceRewriter::kFilterId,
                            "a_no_transform.css", "css", &content, &headers));
  EXPECT_EQ(" a ", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  // Lookup the output resource, input resource, and metadata.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // meta data & original
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  ClearStats();
  EXPECT_TRUE(FetchResource(kTestDomain, TrimWhitespaceRewriter::kFilterId,
                            "a_no_transform.css", "css", &content, &headers));
  EXPECT_EQ(" a ", content);
  EXPECT_EQ(2, lru_cache()->num_hits());     // meta data & original
  EXPECT_EQ(1, lru_cache()->num_misses());   // output resource
  EXPECT_EQ(0, lru_cache()->num_inserts());  // name mapping & original
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // Now with the option set to false, no-transform shall NOT be honored and
  // resource is rewritten.
  ClearStats();
  options()->ClearSignatureForTesting();
  options()->set_disable_rewrite_on_no_transform(false);
  options()->ComputeSignature();
  EXPECT_TRUE(FetchResource(kTestDomain, TrimWhitespaceRewriter::kFilterId,
                            "a_no_transform.css", "css", &content, &headers));
  EXPECT_EQ("a", content);
  // TODO(mpalem): Verify the following comments are accurate.
  EXPECT_EQ(1, lru_cache()->num_hits());     // original
  // output resource and metadata
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // metadata & output resource
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// Verifies that we can rewrite uncacheable resources without caching them.
TEST_F(RewriteContextTest, FetchUncacheableWithRewritesInLineOfServing) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  GoogleString content;

  // The first time we serve the resource, we insert a memo that it is
  // uncacheable, and a name mapping.
  EXPECT_TRUE(FetchResource(
      kTestDomain, TrimWhitespaceSyncFilter::kFilterId,
                            "a_private.css",
                            "css",
                            &content));
  EXPECT_EQ("a", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // name mapping & uncacheable memo
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Each subsequent time we serve the resource, we should experience a cache
  // hit for the notation that the resource is uncacheable, and then we should
  // perform an origin fetch anyway.
  for (int i = 0; i < 3; ++i) {
    ClearStats();
    EXPECT_TRUE(FetchResource(kTestDomain,
                              TrimWhitespaceSyncFilter::kFilterId,
                              "a_private.css",
                              "css",
                              &content));
    EXPECT_EQ("a", content);
    EXPECT_EQ(1, lru_cache()->num_hits());
    EXPECT_EQ(0, lru_cache()->num_misses());
    EXPECT_EQ(0, lru_cache()->num_inserts());
    EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  }

  // Now, we change the resource.
  ResponseHeaders private_css_header;
  private_css_header.set_major_version(1);
  private_css_header.set_minor_version(1);
  private_css_header.SetStatusAndReason(HttpStatus::kOK);
  private_css_header.SetDateAndCaching(http_cache()->timer()->NowMs(),
                                       kOriginTtlMs,
                                       ", private");
  private_css_header.ComputeCaching();

  SetFetchResponse("http://test.com/a_private.css",
                   private_css_header,
                   " b ");

  // We should continue to experience cache hits, and continue to fetch from
  // the origin.
  for (int i = 0; i < 3; ++i) {
    ClearStats();
    EXPECT_TRUE(FetchResource(kTestDomain,
                              TrimWhitespaceSyncFilter::kFilterId,
                              "a_private.css",
                              "css",
                              &content));
    EXPECT_EQ("b", content);
    EXPECT_EQ(1, lru_cache()->num_hits());
    EXPECT_EQ(0, lru_cache()->num_misses());
    EXPECT_EQ(0, lru_cache()->num_inserts());
    EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  }
  ClearStats();

  // After advancing the time, we should see new cache inserts.  Note that we
  // also get a cache hit because the out-of-date entries are still there.
  AdvanceTimeMs(Timer::kMinuteMs * 50);
  EXPECT_TRUE(FetchResource(kTestDomain,
                            TrimWhitespaceSyncFilter::kFilterId,
                            "a_private.css",
                            "css",
                            &content));
  EXPECT_EQ("b", content);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

// Verifies that we preserve cache-control when rewriting a no-cache resource.
TEST_F(RewriteContextTest, PreserveNoCacheWithRewrites) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(FetchResource(kTestDomain,
                              TrimWhitespaceSyncFilter::kFilterId,
                              "a_no_cache.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ("a", content);
    ConstStringStarVector values;
    headers.Lookup(HttpAttributes::kCacheControl, &values);
    ASSERT_EQ(2, values.size());
    EXPECT_STREQ("max-age=0", *values[0]);
    EXPECT_STREQ("no-cache", *values[1]);
  }
}

TEST_F(RewriteContextTest, PreserveNoCacheWithFailedRewrites) {
  // Make sure propagation of non-cacheability works in case when
  // rewrite failed. (This relies on cache extender explicitly
  // rejecting to rewrite non-cacheable things).
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();

  InitResources();

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    GoogleString content;
    ResponseHeaders headers;

    EXPECT_TRUE(FetchResource(kTestDomain,
                              "ce",
                              "a_no_cache.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ(" a ", content);
    ConstStringStarVector values;
    headers.Lookup(HttpAttributes::kCacheControl, &values);
    ASSERT_EQ(2, values.size());
    EXPECT_STREQ("max-age=0", *values[0]);
    EXPECT_STREQ("no-cache", *values[1]);
  }
}

TEST_F(RewriteContextTest, TestRewritesOnEmptyPublicResources) {
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();
  EnableDebug();

  const int kTtlMs = RewriteOptions::kDefaultImplicitCacheTtlMs;
  const char kPath[] = "test.css";
  const char kDataIn[] = "";

  SetResponseWithDefaultHeaders(kPath, kContentTypeCss, kDataIn,
                                kTtlMs / Timer::kSecondMs);
  for (int i = 0; i < 2; i++) {
    GoogleString content;
    ResponseHeaders headers;

    EXPECT_TRUE(FetchResource(
        kTestDomain, "ce", "test.css", "css", &content, &headers));
    EXPECT_EQ("", content);
    EXPECT_STREQ("max-age=31536000",
                 headers.Lookup1(HttpAttributes::kCacheControl));
  }
}

TEST_F(RewriteContextTest, TestRewritesOnEmptyPrivateResources) {
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();
  EnableDebug();

  const char kPath[] = "test.css";
  ResponseHeaders no_store_css_header;
  int64 now_ms = timer()->NowMs();
  no_store_css_header.set_major_version(1);
  no_store_css_header.set_minor_version(1);
  no_store_css_header.SetStatusAndReason(HttpStatus::kOK);
  no_store_css_header.SetDateAndCaching(now_ms, 0, ",no-store");
  no_store_css_header.ComputeCaching();

  SetFetchResponse(AbsolutifyUrl(kPath), no_store_css_header, "");

  for (int i = 0; i < 2; i++) {
    GoogleString content;
    ResponseHeaders headers;

    EXPECT_TRUE(FetchResource(
        kTestDomain, "ce", "test.css", "css", &content, &headers));
    EXPECT_EQ("", content);
    ConstStringStarVector values;
    headers.Lookup(HttpAttributes::kCacheControl, &values);
    ASSERT_EQ(3, values.size());
    EXPECT_STREQ("max-age=0", *values[0]);
    EXPECT_STREQ("no-cache", *values[1]);
    EXPECT_STREQ("no-store", *values[2]);
  }
}

// Verifies that we preserve cache-control when rewriting a no-cache resource
// with a non-on-the-fly filter
TEST_F(RewriteContextTest, PrivateNotCached) {
  InitTrimFiltersSync(kRewrittenResource);
  InitResources();

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    GoogleString content;
    ResponseHeaders headers;

    // There are two possible secure outcomes here: either the fetch fails
    // entirely here, or we serve it as cache-control: private.
    EXPECT_TRUE(FetchResource(kTestDomain,
                              TrimWhitespaceSyncFilter::kFilterId,
                              "a_private.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "private"));
  }

  // Now make sure that fetching with an invalid hash doesn't work when
  // the original is not available. This is significant since if it this fails
  // an attacker may get access to resources without access to an actual hash.
  GoogleString output;
  mock_url_fetcher()->Disable();
  EXPECT_FALSE(FetchResourceUrl(
      Encode(kTestDomain, TrimWhitespaceSyncFilter::kFilterId, "1",
             "a_private.css", "css"),
      &output));
}

TEST_F(RewriteContextTest, PrivateNotCachedOnTheFly) {
  // Variant of the above for on-the-fly, as that relies on completely
  // different code paths to be safe. (It is also covered by earlier tests,
  // but this is included here to be thorough).
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    GoogleString content;
    ResponseHeaders headers;

    EXPECT_TRUE(FetchResource(kTestDomain,
                              TrimWhitespaceSyncFilter::kFilterId,
                              "a_private.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ("a", content);
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "private"))
        << " Not private on fetch #" << i << " " << headers.ToString();
  }

  // Now make sure that fetching with an invalid hash doesn't work when
  // the original is not available. This is significant since if it this fails
  // an attacker may get access to resources without access to an actual hash.
  GoogleString output;
  mock_url_fetcher()->Disable();
  EXPECT_FALSE(FetchResourceUrl(
      Encode(kTestDomain, TrimWhitespaceSyncFilter::kFilterId, "1",
             "a_private.css", "css"),
      &output));
}

// Verifies that we preserve cache-control when rewriting a no-store resource.
TEST_F(RewriteContextTest, PreserveNoStoreWithRewrites) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(FetchResource(kTestDomain,
                              TrimWhitespaceSyncFilter::kFilterId,
                              "a_no_store.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ("a", content);
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "max-age=0"));
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "no-cache"));
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "no-store"));
  }
}

// Verifies that we preserve cache-control when rewriting a private resource.
TEST_F(RewriteContextTest, PreservePrivateWithRewrites) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  // Even on sequential requests, the resource does not become cache-extended.
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(FetchResource(kTestDomain,
                              TrimWhitespaceSyncFilter::kFilterId,
                              "a_private.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ("a", content);
    ConstStringStarVector values;
    headers.Lookup(HttpAttributes::kCacheControl, &values);
    ASSERT_EQ(2, values.size());
    EXPECT_STREQ(OriginTtlMaxAge(), *values[0]);
    EXPECT_STREQ("private", *values[1]);
  }
}

// Verifies that we intersect cache-control when there are multiple input
// resources.
TEST_F(RewriteContextTest, CacheControlWithMultipleInputResources) {
  InitCombiningFilter(0);
  EnableDebug();
  combining_filter_->set_on_the_fly(true);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  GoogleString combined_url =
      Encode(kTestDomain, CombiningFilter::kFilterId, "0",
             MultiUrl("a.css",
                      "b.css",
                      "a_private.css"), "css");

  FetchResourceUrl(combined_url, &content, &headers);
  EXPECT_EQ(" a b a ", content);

  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // 3 inputs.
  EXPECT_EQ(4, lru_cache()->num_inserts()) <<
      "partition, 2 inputs, 1 non-cacheability note";
  EXPECT_EQ(3, counting_url_async_fetcher()->fetch_count());

  ConstStringStarVector values;
  headers.Lookup(HttpAttributes::kCacheControl, &values);
  ASSERT_EQ(2, values.size());
  EXPECT_STREQ(OriginTtlMaxAge(), *values[0]);
  EXPECT_STREQ("private", *values[1]);
}

// Fetching & reconstructing a combined resource with a healthy cache.
TEST_F(RewriteContextTest, CombineFetchHealthyCache) {
  InitCombiningFilter(0);
  EnableDebug();
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  GoogleString combined_url =
      Encode(kTestDomain, CombiningFilter::kFilterId, "0",
             MultiUrl("a.css", "b.css"), "css");
  FetchResourceUrl(combined_url, &content, &headers);
  EXPECT_EQ(" a b", content);

  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses())
      << "output, metadata, 2 inputs";
  EXPECT_EQ(4, lru_cache()->num_inserts()) << "ouptput, metadata, 2 inputs";
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // Now do the fetch again and we will get everything we need in one
  // cache lookup.
  ClearStats();
  content.clear();
  FetchResourceUrl(combined_url, &content, &headers);
  EXPECT_EQ(" a b", content);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// Fetching & reconstructing a combined resource with an unhealthy cache.
TEST_F(RewriteContextTest, CombineFetchUnhealthyCache) {
  lru_cache()->set_is_healthy(false);
  InitCombiningFilter(0);
  EnableDebug();
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  GoogleString combined_url =
      Encode(kTestDomain, CombiningFilter::kFilterId, "0",
             MultiUrl("a.css",
                      "b.css"), "css");
  FetchResourceUrl(combined_url, &content, &headers);
  EXPECT_EQ(" a b", content);

  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // Now do the fetch again.  Because we have no cache, we must fetch
  // the inputs & recombine them, so the stats are exactly the same.
  ClearStats();
  content.clear();
  FetchResourceUrl(combined_url, &content, &headers);
  EXPECT_EQ(" a b", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}


// Verifies that we intersect cache-control when there are multiple input
// resources.
TEST_F(RewriteContextTest, CacheControlWithMultipleInputResourcesAndNoStore) {
  InitCombiningFilter(0);
  EnableDebug();
  combining_filter_->set_on_the_fly(true);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  GoogleString combined_url =
      Encode(kTestDomain, CombiningFilter::kFilterId, "0",
             MultiUrl("a.css",
                      "b.css",
                      "a_private.css",
                      "a_no_store.css"), "css");

  FetchResourceUrl(combined_url, &content, &headers);
  EXPECT_EQ(" a b a  a ", content);

  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses());   // 4 inputs.
  EXPECT_EQ(5, lru_cache()->num_inserts())
      << "partition, 2 inputs, 2 non-cacheability notes";
  EXPECT_EQ(4, counting_url_async_fetcher()->fetch_count());

  EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "max-age=0"));
  EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "no-cache"));
  EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "no-store"));
}

// Verifies that we cache-extend when rewriting a cacheable resource.
TEST_F(RewriteContextTest, CacheExtendCacheableResource) {
  InitTrimFiltersSync(kOnTheFlyResource);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(FetchResource(kTestDomain,
                              TrimWhitespaceSyncFilter::kFilterId,
                              "a.css",
                              "css",
                              &content,
                              &headers));
    EXPECT_EQ("a", content);
    EXPECT_STREQ(StringPrintf("max-age=%lld",
                              static_cast<long long int>(
                                  ServerContext::kGeneratedMaxAgeMs / 1000)),
                 headers.Lookup1(HttpAttributes::kCacheControl));
  }
}

// Make sure we preserve the charset properly.
TEST_F(RewriteContextTest, PreserveCharsetRewritten) {
  InitResources();
  InitTrimFiltersSync(kRewrittenResource);

  GoogleString content;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResource(kTestDomain,
                            TrimWhitespaceSyncFilter::kFilterId,
                            "a_ru.css",
                            "css",
                            &content,
                            &headers));
  EXPECT_STREQ("text/css; charset=koi8-r",
               headers.Lookup1(HttpAttributes::kContentType));
}

TEST_F(RewriteContextTest, PreserveCharsetOnTheFly) {
  InitResources();
  InitTrimFiltersSync(kOnTheFlyResource);

  GoogleString content;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResource(kTestDomain,
                            TrimWhitespaceSyncFilter::kFilterId,
                            "a_ru.css",
                            "css",
                            &content,
                            &headers));
  EXPECT_STREQ("text/css; charset=koi8-r",
               headers.Lookup1(HttpAttributes::kContentType));
}

TEST_F(RewriteContextTest, PreserveCharsetNone) {
  // Null test -- make sure we don't invent a charset when there is none.
  InitResources();
  InitTrimFiltersSync(kRewrittenResource);

  GoogleString content;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResource(
      kTestDomain, TrimWhitespaceSyncFilter::kFilterId, "a.css", "css",
      &content, &headers));
  EXPECT_STREQ("text/css", headers.Lookup1(HttpAttributes::kContentType));
}

// Make sure we preserve charset across 2 filters.
TEST_F(RewriteContextTest, CharsetTwoFilters) {
  InitTwoFilters(kRewrittenResource);
  InitResources();

  GoogleString content;
  ResponseHeaders headers;

  GoogleString url = Encode("", "tw", "0",
                            Encode("", "uc", "0", "a_ru.css", "css"), "css");
  // Need to rewrite HTML first as our test filters aren't registered and hence
  // can't reconstruct.
  ValidateExpected("two_filters",
                   CssLinkHref("a_ru.css"),
                   CssLinkHref(url));

  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, url), &content, &headers));
  EXPECT_STREQ("text/css; charset=koi8-r",
               headers.Lookup1(HttpAttributes::kContentType));
}

TEST_F(RewriteContextTest, FetchColdCacheOnTheFly) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  ClearStats();
  TestServeFiles(&kContentTypeCss, TrimWhitespaceRewriter::kFilterId, "css",
                 "a.css", " a ",
                 "a.css", "a");
}

TEST_F(RewriteContextTest, TrimFetchWrongHash) {
  // Test to see that fetches from wrong hash can fallback to the
  // correct one mentioned in metadata correctly.
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // First rewrite a page to get the right hash remembered
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  ClearStats();

  // Now try fetching it with the wrong hash)
  GoogleString contents;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResourceUrl(Encode(kTestDomain, "tw", "1", "a.css", "css"),
                               &contents, &headers));
  EXPECT_STREQ("a", contents);
  // Should not need any rewrites or fetches.
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // Should have 2 hits: metadata and .0., and 2 misses on wrong-hash version
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  // Make sure the TTL is correct, and the result is private.
  EXPECT_EQ(RewriteOptions::kDefaultImplicitCacheTtlMs,
            headers.cache_ttl_ms());
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsBrowserCacheable());
}

TEST_F(RewriteContextTest, TrimFetchWrongHashColdCache) {
  // Tests fetch with wrong hash when we did not originally create
  // the version with the right hash.
  InitTrimFilters(kRewrittenResource);
  InitResources();

  GoogleString contents;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResourceUrl(Encode(kTestDomain, "tw", "1", "a.css", "css"),
                               &contents, &headers));
  EXPECT_STREQ("a", contents);

  // Make sure the TTL is correct (short), and the result is private.
  EXPECT_EQ(RewriteOptions::kDefaultImplicitCacheTtlMs,
            headers.cache_ttl_ms());
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsBrowserCacheable());
}

TEST_F(RewriteContextTest, TrimFetchHashFailed) {
  // Test to see that if we try to fetch a rewritten version (with a pagespeed
  // resource URL) when metadata cache indicates rewrite of original failed
  // that we will quickly fallback to original without attempting rewrite.
  InitTrimFilters(kRewrittenResource);
  InitResources();
  ValidateNoChanges("no_trimmable", CssLinkHref("b.css"));
  ClearStats();

  GoogleString contents;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResourceUrl(Encode(kTestDomain, "tw", "1", "b.css", "css"),
                               &contents, &headers));
  EXPECT_STREQ("b", contents);
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // Should have 2 hits: metadata and .0., and 2 misses on wrong-hash version
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());

  // Make sure the TTL is correct, and the result is private.
  EXPECT_EQ(RewriteOptions::kDefaultImplicitCacheTtlMs,
            headers.cache_ttl_ms());
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsBrowserCacheable());
}

TEST_F(RewriteContextTest, TrimFetchHashFailedShortTtl) {
  // Variation of TrimFetchHashFailed, where the input's TTL is very short.
  InitTrimFilters(kRewrittenResource);
  InitResources();
  ValidateNoChanges("no_trimmable", CssLinkHref("d.css"));
  ClearStats();

  GoogleString contents;
  ResponseHeaders headers;
  EXPECT_TRUE(FetchResourceUrl(Encode(kTestDomain, "tw", "1", "d.css", "css"),
                               &contents, &headers));
  EXPECT_STREQ("d", contents);
  EXPECT_EQ(kLowOriginTtlMs, headers.cache_ttl_ms());
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_TRUE(headers.IsBrowserCacheable());
}

TEST_F(RewriteContextTest, FetchColdCacheRewritten) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, fetch_successes_->Get());
  EXPECT_EQ(0, fetch_failures_->Get());
  ClearStats();
  TestServeFiles(&kContentTypeCss, TrimWhitespaceRewriter::kFilterId, "css",
                 "a.css", " a ",
                 "a.css", "a");
  // TestServeFiles clears cache so we need to re-fetch.
  EXPECT_EQ(1, fetch_successes_->Get());
  EXPECT_EQ(0, fetch_failures_->Get());
}

TEST_F(RewriteContextTest, OnTheFlyNotFound) {
  InitTrimFilters(kOnTheFlyResource);

  // note: no InitResources so we'll get a file-not-found.
  SetFetchFailOnUnexpected(false);

  // In this case, the resource is optimizable but we'll fail to fetch it.
  ValidateNoChanges("no_trimmable", CssLinkHref("a.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, fetch_failures_->Get());
  EXPECT_EQ(0, fetch_successes_->Get());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("a.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, fetch_failures_->Get());
  EXPECT_EQ(0, fetch_successes_->Get());
}

TEST_F(RewriteContextTest, RewrittenNotFound) {
  InitTrimFilters(kRewrittenResource);

  // note: no InitResources so we'll get a file-not found.
  SetFetchFailOnUnexpected(false);

  // In this case, the resource is optimizable but we'll fail to fetch it.
  ValidateNoChanges("no_trimmable", CssLinkHref("a.css"));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // We should have cached the failed rewrite, no misses, fetches, or inserts.
  ValidateNoChanges("no_trimmable", CssLinkHref("a.css"));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// In this testcase we'll attempt to serve a rewritten resource, but having
// failed to call InitResources we will not be able to do the on-the-fly
// rewrite.
TEST_F(RewriteContextTest, FetchColdCacheOnTheFlyNotFound) {
  InitTrimFilters(kOnTheFlyResource);

  // note: no InitResources so we'll get a file-not found.
  SetFetchFailOnUnexpected(false);

  GoogleString content;
  EXPECT_FALSE(FetchResource(kTestDomain, TrimWhitespaceRewriter::kFilterId,
                             "a.css", "css", &content));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // fetch failure, metadata.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // Try it again with a warm cache.  We'll get a 'hit' which will inform us
  // that this resource is not fetchable.
  EXPECT_FALSE(FetchResource(kTestDomain, TrimWhitespaceRewriter::kFilterId,
                             "a.css", "css", &content));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());  // We "remember" the fetch failure
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// Same testcase, but with a non-on-the-fly resource.
TEST_F(RewriteContextTest, FetchColdCacheRewrittenNotFound) {
  InitTrimFilters(kRewrittenResource);

  // note: no InitResources so we'll get a file-not found.
  SetFetchFailOnUnexpected(false);

  GoogleString content;
  EXPECT_FALSE(FetchResource(kTestDomain, TrimWhitespaceRewriter::kFilterId,
                             "a.css", "css", &content));
  EXPECT_EQ(0, lru_cache()->num_hits());

  // We lookup the output resource plus the inputs and metadata.
  EXPECT_EQ(3, lru_cache()->num_misses());

  // We remember the fetch failure, and the failed rewrite.
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  // Try it again with a warm cache.  We'll get a number of hits which will
  // inform us that this resource is not fetchable:
  // - a metadata entry stating there is no successful rewrite.
  // - HTTP cache entry for resource fetch of original failing
  // - 2nd access of it when we give up on fast path.
  // TODO(morlovich): Should we propagate the 404 directly?
  EXPECT_FALSE(FetchResource(kTestDomain, TrimWhitespaceRewriter::kFilterId,
                             "a.css", "css", &content));
  EXPECT_EQ(3, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TwoFilters) {
  InitTwoFilters(kOnTheFlyResource);
  InitResources();

  ValidateExpected("two_filters",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0",
                                      Encode("", "uc", "0", "a.css", "css"),
                                      "css")));
}

TEST_F(RewriteContextTest, TwoFiltersDelayedFetches) {
  SetupWaitFetcher();
  InitTwoFilters(kOnTheFlyResource);
  InitResources();

  ValidateNoChanges("trimmable1", CssLinkHref("a.css"));
  CallFetcherCallbacks();
  ValidateExpected("delayed_fetches",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0",
                                      Encode("", "uc", "0", "a.css", "css"),
                                      "css")));
}

TEST_F(RewriteContextTest, RepeatedTwoFilters) {
  // Make sure if we have repeated URLs and chaining, it still works right.
  InitTwoFilters(kRewrittenResource);
  InitResources();

  ValidateExpected(
    "two_filters2", StrCat(CssLinkHref("a.css"), CssLinkHref("a.css")),
    StrCat(CssLinkHref(Encode("", "tw", "0",
                              Encode("", "uc", "0", "a.css", "css"), "css")),
           CssLinkHref(Encode("", "tw", "0",
                              Encode("", "uc", "0", "a.css", "css"), "css"))));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
}

TEST_F(RewriteContextTest, ReconstructChainedWrongHash) {
  // Make sure that we don't have problems with repeated reconstruction
  // of chained rewrites where the hash is incorrect. (We used to screw
  // up the response code if two different wrong inner hashes were used,
  // leading to failure at outer level). Also make sure we always propagate
  // short TTL as well, since that could also be screwed up.

  // Need normal filters since cloned RewriteDriver instances wouldn't
  // know about test-only stuff.
  options()->EnableFilter(RewriteOptions::kCombineCss);
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  rewrite_driver()->AddFilters();

  SetResponseWithDefaultHeaders(
      "a.css", kContentTypeCss, " div { display: block;  }", 100);

  GoogleString url = Encode(kTestDomain, "cc", "0",
                            Encode("", "cf", "1", "a.css", "css"), "css");
  GoogleString url2 = Encode(kTestDomain, "cc", "0",
                             Encode("", "cf", "2", "a.css", "css"), "css");

  for (int run = 0; run < 3; ++run) {
    GoogleString content;
    ResponseHeaders headers;

    FetchResourceUrl(url, &content, &headers);
    // Note that this works only because the combiner fails and passes
    // through its input, which is the private cache-controlled output
    // of rewrite_css
    EXPECT_EQ(HttpStatus::kOK, headers.status_code());
    EXPECT_STREQ("div{display:block}", content);
    EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "private"))
        << headers.ToString();
  }

  // Now also try the second version.
  GoogleString content;
  ResponseHeaders headers;
  FetchResourceUrl(url2, &content, &headers);
  EXPECT_EQ(HttpStatus::kOK, headers.status_code());
  EXPECT_STREQ("div{display:block}", content);
  EXPECT_TRUE(headers.HasValue(HttpAttributes::kCacheControl, "private"))
      << headers.ToString();
}

TEST_F(RewriteContextTest, NestedRewriteWith404) {
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  int64 now_ms = http_cache()->timer()->NowMs();
  default_css_header.SetDateAndCaching(now_ms, 3 * kOriginTtlMs);
  default_css_header.ComputeCaching();
  SetFetchResponse(StrCat(kTestDomain, "x.css"), default_css_header,
                   "a.css\n404.css\n");
  SetFetchResponse404("404.css");
  options()->set_implicit_cache_ttl_ms(kOriginTtlMs / 4);
  options()->set_metadata_input_errors_cache_ttl_ms(kOriginTtlMs / 2);

  const GoogleString kRewrittenUrl = Encode(
      "", NestedFilter::kFilterId, "0", "x.css", "css");
  InitNestedFilter(NestedFilter::kExpectNestedRewritesSucceed);
  nested_filter_->set_check_nested_rewrite_result(false);
  InitResources();
  ValidateExpected("async3", CssLinkHref("x.css"), CssLinkHref(kRewrittenUrl));

  // Cache misses for the 3 resources, and 3 metadata lookups. We insert the 6
  // items above, and the rewritten resource into cache.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(6, lru_cache()->num_misses());
  EXPECT_EQ(7, lru_cache()->num_inserts());
  EXPECT_EQ(3, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
  ClearStats();

  AdvanceTimeMs(kOriginTtlMs / 4);
  ValidateExpected("async3", CssLinkHref("x.css"), CssLinkHref(kRewrittenUrl));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());

  ClearStats();
  AdvanceTimeMs(kOriginTtlMs / 4);

  SetupWaitFetcher();
  ValidateNoChanges("async3", CssLinkHref("x.css"));
  CallFetcherCallbacks();
  // metadata (3). x.css and 404.css in the HTTP cache.
  EXPECT_EQ(5, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  // Inserts in both metadata cache and HTTP cache for x.css and 404.css.
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_expirations()->Get());
  ClearStats();
}

TEST_F(RewriteContextTest, NestedLogging) {
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  int64 now_ms = http_cache()->timer()->NowMs();
  default_css_header.SetDateAndCaching(now_ms, 3 * kOriginTtlMs);
  default_css_header.ComputeCaching();
  SetFetchResponse(StrCat(kTestDomain, "x.css"), default_css_header,
                   "a.css\nb.css\n");

  const GoogleString kRewrittenUrl = Encode(
      "", NestedFilter::kFilterId, "0", "x.css", "css");
  InitNestedFilter(NestedFilter::kExpectNestedRewritesSucceed);
  InitResources();
  ValidateExpected("async3", CssLinkHref("x.css"), CssLinkHref(kRewrittenUrl));

  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(6, lru_cache()->num_misses());
  EXPECT_EQ(7, lru_cache()->num_inserts());
  EXPECT_EQ(3, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
  // The following would be 3 if we also logged for nested rewrites.
  EXPECT_EQ(1, logging_info()->metadata_cache_info().num_misses());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_hits());
  ClearStats();

  GoogleString rewritten_contents;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, kRewrittenUrl),
                               &rewritten_contents));
  // Note: These tests do not use HtmlResourceSlots and thus they do not
  // preserve URL relativity.
  EXPECT_EQ(StrCat(Encode(kTestDomain, "uc", "0", "a.css", "css"), "\n",
                   Encode(kTestDomain, "uc", "0", "b.css", "css"), "\n"),
            rewritten_contents);
  // HTTP cache hit for rewritten URL.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_hits());
  ClearStats();

  ValidateExpected("async3", CssLinkHref("x.css"), CssLinkHref(kRewrittenUrl));
  // Completes with a single hit for meta-data.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_expirations()->Get());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_misses());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_revalidates());
  EXPECT_EQ(1, logging_info()->metadata_cache_info().num_hits());
  ClearStats();

  AdvanceTimeMs(2 * kOriginTtlMs);
  ValidateExpected("async3", CssLinkHref("x.css"), CssLinkHref(kRewrittenUrl));
  // Expired meta-data (3).  expired HTTP cache for a.css and b.css, fresh in
  // HTTP cache for x.css.
  EXPECT_EQ(6, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  // Inserts for all meta-data, a.css and b.css in HTTPcache and rewritten URL
  // in HTTP cache.
  EXPECT_EQ(6, lru_cache()->num_inserts());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(2, http_cache()->cache_expirations()->Get());
  // We log only for x.css metadata miss.
  EXPECT_EQ(1, logging_info()->metadata_cache_info().num_misses());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_hits());
  ClearStats();
}

TEST_F(RewriteContextTest, Nested) {
  const GoogleString kRewrittenUrl = Encode("", "nf", "0", "c.css", "css");
  InitNestedFilter(NestedFilter::kExpectNestedRewritesSucceed);
  InitResources();
  ValidateExpected("async3", CssLinkHref("c.css"), CssLinkHref(kRewrittenUrl));
  GoogleString rewritten_contents;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, kRewrittenUrl),
                               &rewritten_contents));
  // Note: These tests do not use HtmlResourceSlots and thus they do not
  // preserve URL relativity.
  EXPECT_EQ(StrCat(Encode(kTestDomain, "uc", "0", "a.css", "css"), "\n",
                   Encode(kTestDomain, "uc", "0", "b.css", "css"), "\n"),
            rewritten_contents);
}

TEST_F(RewriteContextTest, NestedFailed) {
  // Make sure that the was_optimized() bit is not set when the nested
  // rewrite fails (which it will since it's already all caps)
  const GoogleString kRewrittenUrl = Encode("", "nf", "0", "t.css", "css");
  InitNestedFilter(NestedFilter::kExpectNestedRewritesFail);
  InitResources();
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse("http://test.com/u.css", default_css_header,
                     "UPPERCASE");
  SetFetchResponse("http://test.com/t.css", default_css_header,
                     "u.css");
  ValidateExpected("nested-noop", CssLinkHref("t.css"),
                   CssLinkHref(kRewrittenUrl));
}

TEST_F(RewriteContextTest, NestedChained) {
  const GoogleString kRewrittenUrl = Encode("", "nf", "0", "c.css", "css");

  InitNestedFilter(NestedFilter::kExpectNestedRewritesSucceed);
  nested_filter_->set_chain(true);
  InitResources();
  ValidateExpected(
      "async_nest_chain", CssLinkHref("c.css"), CssLinkHref(kRewrittenUrl));
  GoogleString rewritten_contents;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, kRewrittenUrl),
                               &rewritten_contents));
  // We expect each URL twice since we have two nested jobs for it, and the
  // Harvest() just dumps each nested rewrites' slots.
  // Note: These tests do not use HtmlResourceSlots and thus they do not
  // preserve URL relativity.
  EXPECT_EQ(StrCat(Encode(kTestDomain, "uc", "0", "a.css", "css"), "\n",
                   Encode(kTestDomain, "uc", "0", "a.css", "css"), "\n",
                   Encode(kTestDomain, "uc", "0", "b.css", "css"), "\n",
                   Encode(kTestDomain, "uc", "0", "b.css", "css"), "\n"),
            rewritten_contents);
}

TEST_F(RewriteContextTest, Cancel) {
  // Make sure Cancel is called properly when disable_further_processing()
  // is invoked.
  RewriteDriver* driver = rewrite_driver();
  CombiningFilter* combining_filter1 =
      new CombiningFilter(driver, mock_scheduler(), 0 /* no delay*/);
  CombiningFilter* combining_filter2 =
      new CombiningFilter(driver, mock_scheduler(), 0 /* no delay*/);
  driver->AppendRewriteFilter(combining_filter1);
  driver->AppendRewriteFilter(combining_filter2);
  server_context()->ComputeSignature(options());
  InitResources();
  GoogleString combined_url =
      Encode("", CombiningFilter::kFilterId, "0",
             MultiUrl("a.css", "b.css"), "css");

  ValidateExpected("cancel", StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
                   CssLinkHref(combined_url));
  EXPECT_EQ(0, combining_filter1->num_cancel());
  // Element getting deleted disables further processing.
  EXPECT_EQ(1, combining_filter2->num_cancel());
}

TEST_F(RewriteContextTest, WillNotRewrite) {
  // Make sure WillNotRewrite is called properly when filter misses the
  // deadline.
  RewriteDriver* driver = rewrite_driver();
  TrimWhitespaceRewriter* trimmer =
      new TrimWhitespaceRewriter(kRewrittenResource);
  rewrite_driver()->AppendRewriteFilter(
      new SimpleTextFilter(trimmer, rewrite_driver()));
  CombiningFilter* combining_filter =
      new CombiningFilter(driver, mock_scheduler(), 100 /* delay, ms */);
  driver->AppendRewriteFilter(combining_filter);
  server_context()->ComputeSignature(options());

  GoogleString out_url_a = Encode("", TrimWhitespaceRewriter::kFilterId,
                                  "0", "a.css", "css");
  GoogleString out_url_c = Encode("", TrimWhitespaceRewriter::kFilterId,
                                  "0", "c.css", "css");
  InitResources();
  ValidateExpected(
      "will_not_rewrite",
      StrCat(CssLinkHref("a.css"), CssLinkHref("c.css")),
      StrCat(CssLinkHref(out_url_a), CssLinkHref(out_url_c)));
  EXPECT_EQ(0, combining_filter->num_render());
  EXPECT_EQ(1, combining_filter->num_will_not_render());
}

TEST_F(RewriteContextTest, RewritePartitionFailed) {
  // PartitionFailed still calls Rewrite., as documented.
  RewriteDriver* driver = rewrite_driver();
  CombiningFilter* combining_filter =
      new CombiningFilter(driver, mock_scheduler(), 0 /* delay, ms */);
  driver->AppendRewriteFilter(combining_filter);
  server_context()->ComputeSignature(options());
  SetFetchResponse404("404.css");
  ValidateNoChanges("will_not_rewrite_partition_failed",
                    StrCat(CssLinkHref("404.css"), CssLinkHref("404.css")));
  EXPECT_EQ(1, combining_filter->num_render());
  EXPECT_EQ(0, combining_filter->num_will_not_render());
}

TEST_F(RewriteContextTest, DisableFurtherProcessing) {
  // Make sure that set_disable_further_processing() done in the combiner
  // prevents later rewrites from running. To test this, we add the combiner
  // before the trimmer.
  RewriteDriver* driver = rewrite_driver();
  CombiningFilter* combining_filter =
      new CombiningFilter(driver, mock_scheduler(), 0 /* no delay */);
  driver->AppendRewriteFilter(combining_filter);
  TrimWhitespaceRewriter* trimmer =
      new TrimWhitespaceRewriter(kRewrittenResource);
  rewrite_driver()->AppendRewriteFilter(
      new SimpleTextFilter(trimmer, rewrite_driver()));
  driver->AddFilters();

  InitResources();
  GoogleString combined_leaf = Encode("", CombiningFilter::kFilterId, "0",
                                      MultiUrl("a.css", "b.css"), "css");
  GoogleString trimmed_url = Encode(
      "", TrimWhitespaceRewriter::kFilterId, "0", combined_leaf, "css");
  ValidateExpected(
      "combine_then_trim",
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(trimmed_url));

  // Should only be 1 rewrite: on the actual combined link, not the slot
  // that used to have b.css. Note that this doesn't really cover
  // disable_further_processing, since the framework may avoid
  // the issue by reusing the rewrite.
  EXPECT_EQ(1, trimmer->num_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_repeated_rewrites());
  EXPECT_EQ(1, metadata_cache_info().num_disabled_rewrites());
  EXPECT_EQ(2, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, metadata_cache_info().num_hits());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(2, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(3, metadata_cache_info().num_rewrites_completed());
  ClearStats();

  // Now prevent trim from running. Should not see it in the URL.
  combining_filter->set_disable_successors(true);
  GoogleString combined_url = Encode(
      "", CombiningFilter::kFilterId, "0", MultiUrl("a.css", "b.css"), "css");
  ValidateExpected(
      "combine_then_block_trim",
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));

  EXPECT_EQ(1, trimmer->num_rewrites());  // unchanged.
  EXPECT_EQ(0, metadata_cache_info().num_repeated_rewrites());
  EXPECT_EQ(2, metadata_cache_info().num_disabled_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(1, metadata_cache_info().num_hits());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(3, metadata_cache_info().num_rewrites_completed());
  ClearStats();

  // Cached, too.
  ValidateExpected(
      "combine_then_block_trim",
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));

  EXPECT_EQ(1, trimmer->num_rewrites());  // unchanged.
  EXPECT_EQ(0, metadata_cache_info().num_repeated_rewrites());
  EXPECT_EQ(2, metadata_cache_info().num_disabled_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(1, metadata_cache_info().num_hits());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(3, metadata_cache_info().num_rewrites_completed());
  ClearStats();
}

TEST_F(RewriteContextTest, CombinationRewrite) {
  InitCombiningFilter(0);
  EnableDebug();
  InitResources();
  GoogleString combined_url = Encode("", CombiningFilter::kFilterId,
                                     "0", MultiUrl("a.css", "b.css"), "css");
  ValidateExpected(
      "combination_rewrite", StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // partition, and 2 inputs.
  EXPECT_EQ(4, lru_cache()->num_inserts());  // partition, output, and 2 inputs.
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  ValidateExpected(
      "combination_rewrite2",
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(1, lru_cache()->num_hits());     // the output is all we need
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// Proof-of-concept simulation of a Rewriter where delay is injected into
// the Rewrite flow.
TEST_F(RewriteContextTest, CombinationRewriteWithDelay) {
  InitCombiningFilter(kRewriteDelayMs);
  DebugWithMessage("<!--deadline_exceeded for filter Combining-->");
  InitResources();
  GoogleString combined_url = Encode("", CombiningFilter::kFilterId,
                                     "0", MultiUrl("a.css", "b.css"), "css");
  ValidateExpected(
      "xx",
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      StrCat(CssLinkHref("a.css"), DebugMessage(""),
             CssLinkHref("b.css"), DebugMessage("")));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // partition, and 2 inputs.
  EXPECT_EQ(3, lru_cache()->num_inserts());  // partition+2 in, output not ready
  ClearStats();

  // The delay was too large so we were not able to complete the
  // Rewrite.  Now give it more time so it will complete.

  rewrite_driver()->BoundedWaitFor(
      RewriteDriver::kWaitForCompletion, kRewriteDelayMs);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());  // finally we cache the output.
  ClearStats();

  ValidateExpected(
      "combination_rewrite", StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());   // partition, and 2 inputs.
  EXPECT_EQ(0, lru_cache()->num_inserts());  // partition, output, and 2 inputs.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  ClearStats();

  ValidateExpected(
      "combination_rewrite2",
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(1, lru_cache()->num_hits());     // the output is all we need
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// This is the same test as the first stanza of CombinationRewriteWithDelay, but
// includes the Debug filter so we get DeadlineExceeded debug messages injected.
TEST_F(RewriteContextTest, CombinationRewriteWithDelayAndDebug) {
  InitCombiningFilter(kRewriteDelayMs);
  EnableDebug();
  InitResources();
  Parse("xx", StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")));
  GoogleString kDeadlineExceededComment(StrCat(
      "<!--", RewriteDriver::DeadlineExceededMessage("Combining"), "-->"));
  EXPECT_TRUE(output_buffer_.find(
      StrCat(CssLinkHref("a.css"), kDeadlineExceededComment,
             CssLinkHref("b.css"), kDeadlineExceededComment))
              != GoogleString::npos);
}

TEST_F(RewriteContextTest, CombinationFetch) {
  InitCombiningFilter(0);
  EnableDebug();
  InitResources();

  GoogleString combined_url = Encode(kTestDomain, CombiningFilter::kFilterId,
                                     "0", MultiUrl("a.css", "b.css"), "css");

  // The input URLs are not in cache, but the fetch should work.
  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(combined_url, &content));
  EXPECT_EQ(" a b", content);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses())
      << "1 miss for the output.  1 before we acquire the lock, "
      << "and one after we acquire the lock.  Then we miss on the metadata "
      << "and the two inputs.";

  EXPECT_EQ(4, lru_cache()->num_inserts()) << "2 inputs, 1 output, 1 metadata.";
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  Variable* v = statistics()->GetVariable(
      RewriteContext::kNumDeadlineAlarmInvocations);
  EXPECT_EQ(0, v->Get());
  ClearStats();
  content.clear();

  // Now fetch it again.  This time the output resource is cached.
  EXPECT_TRUE(FetchResourceUrl(combined_url, &content));
  EXPECT_EQ(" a b", content);
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// FYI: Takes ~70000 ms to run under Valgrind.
TEST_F(RewriteContextTest, FetchDeadlineTest) {
  // This tests that deadlines on fetches are functional.
  // This uses a combining filter with one input, as it has the needed delay
  // functionality.
  InitCombiningFilter(Timer::kMonthMs);
  EnableDebug();
  InitResources();
  combining_filter_->set_prefix("|");

  GoogleString combined_url = Encode(kTestDomain, CombiningFilter::kFilterId,
                                     "0", "a.css", "css");

  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(combined_url, &content));
  // Should not get a |, as 1 month is way bigger than the rendering deadline.
  EXPECT_EQ(" a ", content);
  EXPECT_EQ(3, lru_cache()->num_inserts());  // input, output, metadata

  // However, due to mock scheduler auto-advance, it should finish
  // everything now, and be able to do it from cache.
  content.clear();
  Variable* v = statistics()->GetVariable(
      RewriteContext::kNumDeadlineAlarmInvocations);
  EXPECT_EQ(1, v->Get());

  ClearStats();
  EXPECT_TRUE(FetchResourceUrl(combined_url, &content));
  EXPECT_EQ("| a ", content);

  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, FetchDeadlineMandatoryTest) {
  // Version of FetchDeadlineTest where the filter is marked as not being
  // an optimization only. This effectively disables the deadline.
  InitCombiningFilter(Timer::kMonthMs);
  EnableDebug();
  InitResources();
  combining_filter_->set_optimization_only(false);
  combining_filter_->set_prefix("|");

  GoogleString combined_url = Encode(kTestDomain, CombiningFilter::kFilterId,
                                     "0", "a.css", "css");

  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(combined_url, &content));
  // Should get a |, despite 1 month simulated delay inside the combine filter
  // being way bigger than the rendering deadline.
  EXPECT_EQ("| a ", content);
  EXPECT_EQ(3, lru_cache()->num_inserts());  // input, output, metadata
}

TEST_F(RewriteContextTest, FetchDeadlineTestBeforeDeadline) {
  // As above, but rewrite finishes quickly. This time we should see the |
  // immediately
  InitCombiningFilter(1 /*ms*/);
  EnableDebug();
  InitResources();
  combining_filter_->set_prefix("|");

  GoogleString combined_url = Encode(kTestDomain, CombiningFilter::kFilterId,
                                     "0", "a.css", "css");

  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(combined_url, &content));
  // Should get a |, as 1 ms is smaller than the rendering deadline.
  EXPECT_EQ("| a ", content);

  // And of course it's nicely cached.
  content.clear();
  ClearStats();
  EXPECT_TRUE(FetchResourceUrl(combined_url, &content));
  EXPECT_EQ("| a ", content);

  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, LoadSheddingTest) {
  const int kThresh = 20;
  server_context()->low_priority_rewrite_workers()->
      SetLoadSheddingThreshold(kThresh);

  const char kCss[] = " * { display: none; } ";
  const char kMinifiedCss[] = "*{display:none}";

  InitResources();
  for (int i = 0; i < 2 * kThresh; ++i) {
    GoogleString file_name = IntegerToString(i);
    SetResponseWithDefaultHeaders(
        file_name, kContentTypeCss, kCss, Timer::kYearMs / Timer::kSecondMs);
  }

  // We use a sync point here to wedge the combining filter, and then have
  // other filters behind it accumulate lots of work and get load-shed.
  InitCombiningFilter(0);
  EnableDebug();
  combining_filter_->set_prefix("|");
  WorkerTestBase::SyncPoint rewrite_reached(
      server_context()->thread_system());
  WorkerTestBase::SyncPoint resume_rewrite(server_context()->thread_system());
  combining_filter_->set_rewrite_signal_on(&rewrite_reached);
  combining_filter_->set_rewrite_block_on(&resume_rewrite);

  GoogleString combined_url = Encode(kTestDomain, CombiningFilter::kFilterId,
                                     "0", "a.css", "css");

  GoogleString out_combine;
  StringAsyncFetch async_fetch(CreateRequestContext(), &out_combine);
  rewrite_driver()->FetchResource(combined_url, &async_fetch);
  rewrite_reached.Wait();

  // We need separate rewrite drivers, strings, and callbacks for each of the
  // other requests..
  std::vector<GoogleString*> outputs;
  std::vector<StringAsyncFetch*> fetchers;
  std::vector<RewriteDriver*> drivers;

  for (int i = 0; i < 2 * kThresh; ++i) {
    GoogleString file_name = IntegerToString(i);
    GoogleString* out = new GoogleString;
    RequestContextPtr ctx =
        RequestContext::NewTestRequestContext(
            server_context()->thread_system());
    StringAsyncFetch* fetch = new StringAsyncFetch(ctx, out);
    RewriteDriver* driver = server_context()->NewRewriteDriver(ctx);
    GoogleString out_url =
        Encode(kTestDomain, "cf", "0", file_name, "css");
    driver->FetchResource(out_url, fetch);

    outputs.push_back(out);
    fetchers.push_back(fetch);
    drivers.push_back(driver);
  }

  // Note that we know that we're stuck in the middle of combining filter's
  // rewrite, as it signaled us on rewrite_reached, but we didn't yet
  // signal on resume_rewrite. This means that once the 2 * kThresh rewrites
  // will get queued up, we will be forced to load-shed kThresh of them
  // (with combiner not canceled since it's already "running"), and so the
  // rewrites 0 ... kThresh - 1 can actually complete via shedding now.
  for (int i = 0; i < kThresh; ++i) {
    drivers[i]->WaitForCompletion();
    drivers[i]->Cleanup();
    // Since this got load-shed, we expect output to be unoptimized,
    // and private cache-control.
    EXPECT_STREQ(kCss, *outputs[i]) << "rewrite:" << i;
    EXPECT_TRUE(fetchers[i]->response_headers()->HasValue(
                    HttpAttributes::kCacheControl, "private"));
  }

  // Unwedge the combiner, then collect other rewrites.
  resume_rewrite.Notify();

  for (int i = kThresh; i < 2 * kThresh; ++i) {
    drivers[i]->WaitForCompletion();
    drivers[i]->Cleanup();
    // These should be optimized.
    EXPECT_STREQ(kMinifiedCss, *outputs[i]) << "rewrite:" << i;
    EXPECT_FALSE(fetchers[i]->response_headers()->HasValue(
                     HttpAttributes::kCacheControl, "private"));
  }

  STLDeleteElements(&outputs);
  STLDeleteElements(&fetchers);

  rewrite_driver()->WaitForShutDown();
}

TEST_F(RewriteContextTest, CombinationFetchMissing) {
  InitCombiningFilter(0);
  EnableDebug();
  SetFetchFailOnUnexpected(false);
  GoogleString combined_url = Encode(kTestDomain, CombiningFilter::kFilterId,
                                     "0", MultiUrl("a.css", "b.css"), "css");
  EXPECT_FALSE(TryFetchResource(combined_url));
}

TEST_F(RewriteContextTest, CombinationFetchNestedMalformed) {
  // Fetch of a combination where nested URLs look like they were pagespeed-
  // produced, but actually have invalid filter ids.
  InitCombiningFilter(0);
  EnableDebug();
  SetFetchFailOnUnexpected(false);
  GoogleString combined_url = Encode(
      kTestDomain, CombiningFilter::kFilterId, "0",
      MultiUrl("a.pagespeed.nosuchfilter.0.css",
               "b.pagespeed.nosuchfilter.0.css"), "css");
  EXPECT_FALSE(TryFetchResource(combined_url));
}

TEST_F(RewriteContextTest, CombinationFetchSeedsCache) {
  // Make sure that fetching a combination seeds cache for future rewrites
  // properly.
  InitCombiningFilter(0 /* no rewrite delay*/);
  EnableDebug();
  InitResources();

  // First fetch it..
  GoogleString combined_url = Encode("", CombiningFilter::kFilterId,
                                     "0", MultiUrl("a.css", "b.css"), "css");
  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, combined_url), &content));
  EXPECT_EQ(" a b", content);
  ClearStats();

  // Then use from HTML.
  ValidateExpected(
      "hopefully_hashed",
      StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// Test that rewriting works correctly when input resource is loaded from disk.

TEST_F(RewriteContextTest, LoadFromFileOnTheFly) {
  options()->file_load_policy()->Associate(kTestDomain, "/test/");
  InitTrimFilters(kOnTheFlyResource);

  // Init file resources.
  WriteFile("/test/a.css", " foo b ar ");

  // The first rewrite was successful because we block for reading from
  // filesystem, not because we did any cache lookups.
  ClearStats();
  ValidateExpected("trimmable", CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(0, lru_cache()->num_hits());
  // 1 cache miss for the OutputPartitions.  The input resource does not
  // induce a cache check as it's loaded from the file system.
  EXPECT_EQ(1, lru_cache()->num_misses());
  // 1 cache insertion: resource mapping (CachedResult).
  // Output resource not stored in cache (because it's an on-the-fly resource).
  EXPECT_EQ(1, lru_cache()->num_inserts());
  // No fetches because it's loaded from file.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ClearStats();
  ValidateExpected("trimmable",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // Note: We do not load the resource again until the fetch.
}

TEST_F(RewriteContextTest, LoadFromFileRewritten) {
  options()->file_load_policy()->Associate(kTestDomain, "/test/");
  InitTrimFilters(kRewrittenResource);

  // Init file resources.
  WriteFile("/test/a.css", " foo b ar ");

  // The first rewrite was successful because we block for reading from
  // filesystem, not because we did any cache lookups.
  ClearStats();
  ValidateExpected("trimmable",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(0, lru_cache()->num_hits());
  // 1 cache miss for the OutputPartitions.  No cache lookup is
  // done for the input resource since it is loaded from the file system.
  EXPECT_EQ(1, lru_cache()->num_misses());
  // 2 cache insertion: resource mapping (CachedResult) and output resource.
  EXPECT_EQ(2, lru_cache()->num_inserts());
  // No fetches because it's loaded from file.
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, file_system()->num_input_file_opens());

  // The second cache time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a single
  // cache hit for the metadata.  No cache misses will occur.
  ClearStats();
  ValidateExpected("trimmable",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, file_system()->num_input_file_opens());
  // Note: We do not load the resource again until the fetch.
}

namespace {

// Filter that blocks on Flush() in order to let an actual rewrite succeed
// while we are still 'parsing'.
class TestWaitFilter : public CommonFilter {
 public:
  TestWaitFilter(RewriteDriver* driver,
                 WorkerTestBase::SyncPoint* sync)
      : CommonFilter(driver), sync_(sync) {}
  virtual ~TestWaitFilter() {}

  virtual const char* Name() const { return "TestWait"; }
  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(net_instaweb::HtmlElement*) {}
  virtual void EndElementImpl(net_instaweb::HtmlElement*) {}

  virtual void Flush() {
    sync_->Wait();
    driver()->set_externally_managed(true);
    CommonFilter::Flush();
  }

 private:
  WorkerTestBase::SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(TestWaitFilter);
};

// Filter that wakes up a given sync point once its rewrite context is
// getting destroyed.
class TestNotifyFilter : public CommonFilter {
 public:
  class Context : public SingleRewriteContext {
   public:
    Context(RewriteDriver* driver, WorkerTestBase::SyncPoint* sync)
        : SingleRewriteContext(driver, NULL /* parent */,
                               NULL /* resource context*/),
          sync_(sync) {}

    virtual ~Context() {
      sync_->Notify();
    }

   protected:
    virtual void RewriteSingle(
        const ResourcePtr& input, const OutputResourcePtr& output) {
      RewriteDone(kRewriteFailed, 0);
    }

    virtual const char* id() const { return "testnotify"; }
    virtual OutputResourceKind kind() const { return kRewrittenResource; }

   private:
    WorkerTestBase::SyncPoint* sync_;
    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  TestNotifyFilter(RewriteDriver* driver, WorkerTestBase::SyncPoint* sync)
      : CommonFilter(driver), sync_(sync)  {}
  virtual ~TestNotifyFilter() {}

  virtual const char* Name() const {
    return "Notify";
  }

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(net_instaweb::HtmlElement* element) {
    HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
    if (href != NULL) {
      bool unused;
      ResourcePtr input_resource(CreateInputResource(
          href->DecodedValueOrNull(), &unused));
      ResourceSlotPtr slot(driver()->GetSlot(input_resource, element, href));
      Context* context = new Context(driver(), sync_);
      context->AddSlot(slot);
      driver()->InitiateRewrite(context);
    }
  }

  virtual void EndElementImpl(net_instaweb::HtmlElement*) {}

 private:
  WorkerTestBase::SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(TestNotifyFilter);
};

}  // namespace

// Test to make sure we don't crash/delete a RewriteContext when it's completed
// while we're still writing. Not 100% guaranteed to crash, however, as
// we notice in ~TestNotifyFilter::Context and not when context is fully
// destroyed.
TEST_F(RewriteContextTest, UltraQuickRewrite) {
  // Turn on automatic memory management for now, to see if it tries to
  // auto-delete while still parsing. We turn it off inside
  // TestWaitFilter::Flush.
  rewrite_driver()->set_externally_managed(false);
  InitResources();

  WorkerTestBase::SyncPoint sync(server_context()->thread_system());
  rewrite_driver()->AppendOwnedPreRenderFilter(
      new TestNotifyFilter(rewrite_driver(), &sync));
  rewrite_driver()->AddOwnedPostRenderFilter(
      new TestWaitFilter(rewrite_driver(), &sync));
  server_context()->ComputeSignature(options());

  ValidateExpected("trimmable.quick", CssLinkHref("a.css"),
                   CssLinkHref("a.css"));
}

TEST_F(RewriteContextTest, RenderCompletesCacheAsync) {
  // Make sure we finish rendering fully even when cache is ultra-slow.
  SetCacheDelayUs(50 * kRewriteDeadlineMs * 1000);
  InitTrimFilters(kRewrittenResource);
  InitResources();

  // First time we're fetching, so we don't know.
  Parse("trimmable_async", CssLinkHref("a.css"));
  rewrite_driver()->WaitForCompletion();

  ValidateExpected("trimmable_async",
                   CssLinkHref("a.css"),
                   CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
}

TEST_F(RewriteContextTest, TestDisableBackgroundRewritesForBots) {
  InitTrimFilters(kRewrittenResource);
  InitResources();
  options()->ClearSignatureForTesting();
  options()->set_disable_background_fetches_for_bots(true);
  options()->ComputeSignature();

  // Bot user agent. No fetches triggered.
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kGooglebotUserAgent);
  ValidateNoChanges("initial", CssLinkHref("a.css"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());

  ClearStats();
  // Non-bot user agent. Fetch and rewrite triggered.
  rewrite_driver()->SetUserAgent("new");
  ValidateExpected(
      "initial",
      CssLinkHref("a.css"),
      CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(3, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());

  ClearStats();
  // Bot user agent. HTML is rewritten.
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kGooglebotUserAgent);
  ValidateExpected(
      "initial",
      CssLinkHref("a.css"),
      CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());

  // Advance close to expiry, so that freshen is triggered.
  AdvanceTimeMs(kOriginTtlMs * 9 / 10);

  ClearStats();
  // Bot user agent. HTML is rewritten, but no fetches are triggered.
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kGooglebotUserAgent);
  ValidateExpected(
      "initial",
      CssLinkHref("a.css"),
      CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());

  ClearStats();
  // Non-bot user agent. Freshen triggers a fetch.
  rewrite_driver()->SetUserAgent("new");
  ValidateExpected(
      "initial",
      CssLinkHref("a.css"),
      CssLinkHref(Encode("", "tw", "0", "a.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());

  // Advance beyond expiry.
  AdvanceTimeMs(kOriginTtlMs * 2);

  ClearStats();
  // Bot user agent. No fetches are triggered.
  rewrite_driver()->SetUserAgent(UserAgentMatcherTestBase::kGooglebotUserAgent);
  ValidateNoChanges("initial", CssLinkHref("a.css"));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(3, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
}

TEST_F(RewriteContextTest, TestFreshen) {
  FetcherUpdateDateHeaders();

  // Note that this must be >= kDefaultImplicitCacheTtlMs for freshening.
  const int kTtlMs = RewriteOptions::kDefaultImplicitCacheTtlMs * 10;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";

  // Start with non-zero time, and init our resource..
  AdvanceTimeMs(kTtlMs / 2);
  InitTrimFilters(kRewrittenResource);

  ResponseHeaders response_headers;
  response_headers.Add(HttpAttributes::kContentType,
                       kContentTypeCss.mime_type());
  response_headers.SetDateAndCaching(timer()->NowMs(), kTtlMs);
  response_headers.Add(HttpAttributes::kEtag, "etag");
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  response_headers.ComputeCaching();
  mock_url_fetcher()->SetConditionalResponse(
      "http://test.com/test.css", -1, "etag", response_headers, kDataIn);

  // First fetch + rewrite
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // Note that this only measures the number of bytes in the response body.
  EXPECT_EQ(9, counting_url_async_fetcher()->byte_count());
  // Cache miss for the original. The original and rewritten resource, as well
  // as the metadata are inserted into cache.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());

  ClearStats();
  // Advance halfway from TTL. This should be an entire cache hit.
  AdvanceTimeMs(kTtlMs / 2);
  ValidateExpected("fully_hit",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  // No HTTPCache lookups or writes. One metadata cache hit while rewriting.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());

  ClearStats();
  response_headers.FixDateHeaders(timer()->NowMs());
  // Advance close to TTL and rewrite. We should see an extra fetch.
  // Also upload a version with a newer timestamp.
  mock_url_fetcher()->SetConditionalResponse(
      "http://test.com/test.css", -1, "etag", response_headers, kDataIn);
  AdvanceTimeMs(kTtlMs / 2 - 3 * Timer::kMinuteMs);
  ValidateExpected("freshen",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(
      1,
      server_context()->rewrite_stats()->num_conditional_refreshes()->Get());
  // No bytes are downloaded since we conditionally refresh the resource.
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  // Miss for the original since it is within a minute of its expiration time.
  // The newly fetched resource is inserted into the cache, and the metadata is
  // updated.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());

  ClearStats();
  // Advance again closer to the TTL. This shouldn't trigger any fetches since
  // the last freshen updated the cache. Also, no freshens are triggered here
  // since the last freshen updated the metadata cache.
  AdvanceTimeMs(2 * Timer::kMinuteMs);
  ValidateExpected("freshen",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());

  ClearStats();
  // Now advance past original expiration. Note that we don't require any extra
  // fetches since the resource was freshened by the previous fetch.
  SetupWaitFetcher();
  AdvanceTimeMs(kTtlMs * 4 / 10);
  ValidateExpected("freshen2",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  // Make sure we do this or it will leak.
  CallFetcherCallbacks();
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

TEST_F(RewriteContextTest, TestFreshenForMultipleResourceRewrites) {
  FetcherUpdateDateHeaders();
  InitCombiningFilter(0 /* no rewrite delay */);
  EnableDebug();
  // We use MD5 hasher instead of mock hasher so that the rewritten url changes
  // when its content gets updated.
  UseMd5Hasher();

  // Note that this must be >= kDefaultImplicitCacheTtlMs for freshening.
  const int kTtlMs1 = RewriteOptions::kDefaultImplicitCacheTtlMs * 10;
  const char kPath1[] = "first.css";
  const char kDataIn1[] = " first ";
  const char kDataNew1[] = " new first ";

  const int kTtlMs2 = RewriteOptions::kDefaultImplicitCacheTtlMs * 5;
  const char kPath2[] = "second.css";
  const char kDataIn2[] = " second ";

  // Start with non-zero time, and init our resources.
  AdvanceTimeMs(kTtlMs2 / 2);
  SetResponseWithDefaultHeaders(kPath1, kContentTypeCss, kDataIn1,
                                kTtlMs1 / Timer::kSecondMs);
  SetResponseWithDefaultHeaders(kPath2, kContentTypeCss, kDataIn2,
                                kTtlMs2 / Timer::kSecondMs);

  // First fetch + rewrite
  GoogleString combined_url = Encode(
      "", CombiningFilter::kFilterId, "V3iNJlBg52",
      MultiUrl("first.css", "second.css"), "css");

  ValidateExpected("initial",
                   StrCat(CssLinkHref("first.css"), CssLinkHref("second.css")),
                   CssLinkHref(combined_url));
  EXPECT_EQ(1, combining_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  // Cache misses for both the css files. The original resources, the combined
  // css file and the metadata is inserted into cache.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, http_cache()->cache_misses()->Get());
  EXPECT_EQ(3, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());

  ClearStats();
  // Advance halfway from TTL. This should be an entire cache hit.
  AdvanceTimeMs(kTtlMs2 / 2);
  ValidateExpected("fully_hit",
                   StrCat(CssLinkHref("first.css"), CssLinkHref("second.css")),
                   CssLinkHref(combined_url));
  EXPECT_EQ(0, combining_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // No HTTPCache lookups or writes. One metadata cache hit while rewriting.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());

  ClearStats();
  // Advance close to TTL and rewrite. We should see an extra fetch.
  // Also upload a version with a newer timestamp.
  SetResponseWithDefaultHeaders(kPath2, kContentTypeCss, kDataNew1,
                                kTtlMs2 / Timer::kSecondMs);
  AdvanceTimeMs(kTtlMs2 / 2 - 3 * Timer::kMinuteMs);

  // Grab a lock for the resource that we are trying to freshen, preventing
  // that flow from working.
  scoped_ptr<NamedLock> lock(server_context()->MakeInputLock(
      StrCat(kTestDomain, kPath2)));
  ASSERT_TRUE(lock->TryLock());
  ValidateExpected("freshen",
                   StrCat(CssLinkHref("first.css"), CssLinkHref("second.css")),
                   CssLinkHref(combined_url));

  // We do the cache lookups before acquiring the lock.  Based on the TTL in
  // the resource, we decide we want to freshen, we attempt to grab the lock
  // to initiate a new fetch, but fail.  No fetch is made, metadata cache is
  // not cleared.  Nothing is inserted into the cache.
  EXPECT_EQ(0, combining_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits()) << "metadata&soon-to-expire second.css";
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());
  lock->Unlock();

  ClearStats();
  ValidateExpected("freshen",
                   StrCat(CssLinkHref("first.css"), CssLinkHref("second.css")),
                   CssLinkHref(combined_url));
  EXPECT_EQ(0, combining_filter_->num_rewrites());
  // One fetch while freshening the second resource.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // Cache miss for the original since it is within a minute of its expiration
  // time. The newly fetched resource is inserted into the cache. The metadata
  // is deleted since one of the resources changed.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  // Two deletes. One for the metadata. The replacement of the second resource
  // in the HTTPCache is counted both as a delete, and an insert.
  EXPECT_EQ(2, lru_cache()->num_deletes());

  ClearStats();
  // Advance again closer to the TTL. This shouldn't trigger any fetches since
  // the last freshen updated the cache.
  AdvanceTimeMs(2 * Timer::kMinuteMs);

  combined_url = Encode("", CombiningFilter::kFilterId, "YosxgdTZiZ",
                        MultiUrl("first.css", "second.css"), "css");

  ValidateExpected("freshen",
                   StrCat(CssLinkHref("first.css"), CssLinkHref("second.css")),
                   CssLinkHref(combined_url));
  EXPECT_EQ(1, combining_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(2, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());

  ClearStats();
  // Now advance past original expiration. Note that we don't require any extra
  // fetches since the resource was freshened by the previous fetch.
  SetupWaitFetcher();
  AdvanceTimeMs(kTtlMs2 * 4 / 10);
  ValidateExpected("freshen2",
                   StrCat(CssLinkHref("first.css"), CssLinkHref("second.css")),
                   CssLinkHref(combined_url));
  // Make sure we do this or it will leak.
  CallFetcherCallbacks();
  EXPECT_EQ(0, combining_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

TEST_F(RewriteContextTest, TestFreshenForLowTtl) {
  FetcherUpdateDateHeaders();

  // Note that this must be >= kDefaultImplicitCacheTtlMs for freshening.
  const int kTtlMs = 400 * Timer::kSecondMs;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";

  // Start with non-zero time, and init our resource..
  AdvanceTimeMs(kTtlMs / 2);
  InitTrimFilters(kRewrittenResource);
  SetResponseWithDefaultHeaders(kPath, kContentTypeCss, kDataIn,
                                kTtlMs / Timer::kSecondMs);

  // First fetch + rewrite
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // Cache miss for the original. Both original and rewritten are inserted into
  // cache.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());

  ClearStats();
  // Advance halfway from TTL. This should be an entire cache hit.
  AdvanceTimeMs(kTtlMs / 2);
  ValidateExpected("fully_hit",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // No HTTPCache lookups or writes.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());

  ClearStats();
  // Advance close to TTL and rewrite. We should see an extra fetch.
  // Also upload a version with a newer timestamp.
  SetResponseWithDefaultHeaders(kPath, kContentTypeCss, kDataIn,
                                kTtlMs / Timer::kSecondMs);
  // Move to 85% of expiry.
  AdvanceTimeMs((kTtlMs * 7) / 20);
  ValidateExpected("freshen",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // Miss for the original since it is within a minute of its expiration time.
  // The newly fetched resource is inserted into the cache. The updated
  // metadata is also inserted into cache.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());

  ClearStats();
  // Advance again closer to the TTL. This shouldn't trigger any fetches since
  // the last freshen updated the cache.
  // Move to 95% of expiry.
  AdvanceTimeMs(kTtlMs / 10);
  ValidateExpected("freshen",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // We don't freshen again here since the last freshen updated the cache.
  // One metadata cache hit while rewriting.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());

  ClearStats();
  // Advance past expiry.
  AdvanceTimeMs(kTtlMs * 2);
  SetupWaitFetcher();
  ValidateNoChanges("freshen", CssLinkHref(kPath));
  CallFetcherCallbacks();
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // We don't rewrite here since the metadata expired. We revalidate the
  // metadata, and insert the newly fetched resource and updated metadata into
  // the cache.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
}

TEST_F(RewriteContextTest, TestFreshenWithTwoLevelCache) {
  // Note that this must be >= kDefaultImplicitCacheTtlMs for freshening.
  const int kTtlMs = RewriteOptions::kDefaultImplicitCacheTtlMs * 10;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";

  // Set up a WriteThroughHTTPCache.
  LRUCache l2_cache(1000);
  WriteThroughHTTPCache* two_level_cache = new WriteThroughHTTPCache(
      lru_cache(), &l2_cache, timer(), hasher(), statistics());
  server_context()->set_http_cache(two_level_cache);

  // Start with non-zero time, and init our resource.
  AdvanceTimeMs(kTtlMs / 2);
  InitTrimFilters(kRewrittenResource);
  ResponseHeaders response_headers;
  response_headers.Add(HttpAttributes::kContentType,
                       kContentTypeCss.mime_type());
  response_headers.SetDateAndCaching(timer()->NowMs(), kTtlMs);
  response_headers.Add(HttpAttributes::kEtag, "etag");
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  response_headers.ComputeCaching();
  mock_url_fetcher()->SetConditionalResponse(
      "http://test.com/test.css", -1, "etag", response_headers, kDataIn);

  // First fetch + rewrite.
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(9, counting_url_async_fetcher()->byte_count());
  // Cache miss for the original. Both original and rewritten are inserted into
  // cache. Besides this, the metadata lookup fails and new metadata is inserted
  // into cache. Note that the metadata cache is L1 only.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(0, l2_cache.num_hits());
  EXPECT_EQ(1, l2_cache.num_misses());
  EXPECT_EQ(2, l2_cache.num_inserts());

  ClearStats();
  l2_cache.ClearStats();
  // Advance halfway from TTL. This should be an entire cache hit.
  AdvanceTimeMs(kTtlMs / 2);
  ValidateExpected("fully_hit",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  // L1 cache hit for the metadata.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, l2_cache.num_hits());
  EXPECT_EQ(0, l2_cache.num_misses());
  EXPECT_EQ(0, l2_cache.num_inserts());

  // Create a new fresh response and insert into the L2 cache. Do this by
  // creating a temporary HTTPCache with the L2 cache since we don't want to
  // alter the state of the L1 cache whose response is no longer fresh.
  response_headers.FixDateHeaders(timer()->NowMs());
  mock_url_fetcher()->SetConditionalResponse(
      "http://test.com/test.css", -1, "etag", response_headers, kDataIn);
  HTTPCache* l2_only_cache = new HTTPCache(&l2_cache, timer(), hasher(),
                                           statistics());
  l2_only_cache->Put(
      AbsolutifyUrl(kPath), rewrite_driver_->CacheFragment(),
      RequestHeaders::Properties(),
      ResponseHeaders::GetVaryOption(options()->respect_vary()),
      &response_headers, kDataIn, message_handler());
  delete l2_only_cache;

  ClearStats();
  l2_cache.ClearStats();
  // Advance close to TTL and rewrite. No extra fetches here since we find the
  // response in the L2 cache.
  AdvanceTimeMs(kTtlMs / 2 - 30 * Timer::kSecondMs);
  ValidateExpected("freshen",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  // We find a fresh response in the L2 cache and insert it into the L1 cache.
  // We also update the metadata.
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, l2_cache.num_hits());
  EXPECT_EQ(0, l2_cache.num_misses());
  EXPECT_EQ(0, l2_cache.num_inserts());

  ClearStats();
  l2_cache.ClearStats();
  // Advance again closer to the TTL. This shouldn't trigger any fetches since
  // the last freshen updated the metadata.
  AdvanceTimeMs(15 * Timer::kSecondMs);
  ValidateExpected("freshen",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  // L1 cache hit for the metadata.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, l2_cache.num_hits());
  EXPECT_EQ(0, l2_cache.num_misses());
  EXPECT_EQ(0, l2_cache.num_inserts());

  ClearStats();
  l2_cache.ClearStats();
  // Now, advance past original expiration. Since the metadata has expired we go
  // through the OutputCacheRevalidate flow which looks up cache and finds that
  // the result in cache is valid and calls OutputCacheDone resulting in a
  // successful rewrite. Note that it also sees that the resource is close to
  // expiry and triggers a freshen. The OutputCacheHit flow then triggers
  // another freshen since it observes that the resource refernced in its
  // metadata is close to expiry.
  // Note that only one of these freshens actually trigger a fetch because of
  // the locking mechanism in UrlInputResource to prevent parallel fetches of
  // the same resource.
  // As we are also reusing rewrite results when contents did not change,
  // there is no second rewrite.
  SetupWaitFetcher();
  AdvanceTimeMs(kTtlMs / 2 - 30 * Timer::kSecondMs);
  ValidateExpected("freshen2",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  CallFetcherCallbacks();
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  // The original resource gets refetched and inserted into cache.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());
  EXPECT_EQ(
      1,
      server_context()->rewrite_stats()->num_conditional_refreshes()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  // The entries in both the caches are not within the freshness threshold, and
  // are hence counted as misses.
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, l2_cache.num_hits());
  EXPECT_EQ(0, l2_cache.num_misses());
  EXPECT_EQ(1, l2_cache.num_inserts());
}

TEST_F(RewriteContextTest, TestFreshenForExtendCache) {
  FetcherUpdateDateHeaders();
  UseMd5Hasher();

  // Note that this must be >= kDefaultImplicitCacheTtlMs for freshening.
  const int kTtlMs = RewriteOptions::kDefaultImplicitCacheTtlMs * 10;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";
  const char kHash[] = "mmVFI7stDo";

  // Start with non-zero time, and init our resource..
  AdvanceTimeMs(kTtlMs / 2);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();
  SetResponseWithDefaultHeaders(kPath, kContentTypeCss, kDataIn,
                                kTtlMs / Timer::kSecondMs);

  // First fetch + rewrite
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "ce", kHash, "test.css", "css")));
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // Cache miss for the original. The original resource and the metadata is
  // inserted into cache.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());

  ClearStats();
  // Advance halfway from TTL. This should be an entire cache hit.
  AdvanceTimeMs(kTtlMs / 2);
  ValidateExpected("fully_hit",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "ce", kHash, "test.css", "css")));
  EXPECT_EQ(1, statistics()->GetVariable("cache_extensions")->Get());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // No HTTPCache lookups or writes. One metadata cache hit while rewriting.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());

  ClearStats();
  SetupWaitFetcher();
  // Advance close to TTL and rewrite. We should see an extra fetch.
  // Also upload a version with a newer timestamp.
  SetResponseWithDefaultHeaders(kPath, kContentTypeCss, kDataIn,
                                kTtlMs / Timer::kSecondMs);
  AdvanceTimeMs(kTtlMs / 2 - 3 * Timer::kMinuteMs);
  ValidateExpected("freshen",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "ce", kHash, "test.css", "css")));
  CallFetcherCallbacks();

  EXPECT_EQ(1, statistics()->GetVariable("cache_extensions")->Get());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // Miss for the original since it is past 75% of its expiration time. The
  // newly fetched resource is inserted into the cache. The metadata is also
  // updated and inserted into cache.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  // The insert of the updated resource is counted as both a delete and an
  // insert. The same goes for the metadata.
  EXPECT_EQ(2, lru_cache()->num_deletes());

  ClearStats();
  // Advance again closer to the TTL. This doesn't trigger another freshen
  // since the last freshen updated the metadata.
  AdvanceTimeMs(2 * Timer::kMinuteMs);
  ValidateExpected("freshen",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "ce", kHash, "test.css", "css")));
  EXPECT_EQ(1, statistics()->GetVariable("cache_extensions")->Get());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  // We don't freshen again here since the last freshen updated the cache.
  // One metadata cache hit while rewriting.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
}

TEST_F(RewriteContextTest, TestFreshenForEmbeddedDependency) {
  FetcherUpdateDateHeaders();
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  // proactive_resource_freshening is off by default, so turn it on.
  options()->set_proactive_resource_freshening(true);
  options()->ComputeSignature();
  rewrite_driver()->AddFilters();

  // Set up the resources and ttl. Ttl should be bigger than default implicit
  // cache ttl.
  const int kImageTtl = RewriteOptions::kDefaultImplicitCacheTtlMs * 5;
  const int kCssTtl = RewriteOptions::kDefaultImplicitCacheTtlMs * 10;
  const char kImageContent[] = "image1";
  const char kImagePath[] = "1.jpg";
  const char kCssPath[] = "text.css";
  GoogleString css_content = StrCat("{background:url(\"",
                                    AbsolutifyUrl("1.jpg"), "\")}");

  // Start with non-zero time and init the resources.
  AdvanceTimeMs(kImageTtl / 2);
  SetResponseWithDefaultHeaders(kImagePath, kContentTypeJpeg, kImageContent,
                                kImageTtl / Timer::kSecondMs);
  SetResponseWithDefaultHeaders(kCssPath, kContentTypeCss, css_content,
                                kCssTtl / Timer::kSecondMs);
  GoogleString css_url = AbsolutifyUrl("text.css");
  // Note: Output is absolute, because input is absolute.
  GoogleString rewritten_url =
      Encode(kTestDomain, "cf", "0", "text.css", "css");

  // First fetch misses cache and resources are inserted into the cache.
  ClearStats();
  ValidateExpected("first_fetch", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses());  // cf, ic, 1.jpg, original text.css
  EXPECT_EQ(5, lru_cache()->num_inserts());  // above + rewritten text.css
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, http_cache()->cache_misses()->Get());
  // text.css, 1.jpg, rewritten text.css get inserted in http cache.
  EXPECT_EQ(3, http_cache()->cache_inserts()->Get());

  // The ttl of the resource is the min of all its dependencies and hence
  // kImageTtl in this case. Advance halfway and it should be a hit.
  ClearStats();
  AdvanceTimeMs(kImageTtl / 2);
  ValidateExpected("fully hit", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());

  // Advance time close to the ttl of the image. This should cause a freshen
  // and a fetch of the expiring image url.
  ClearStats();
  AdvanceTimeMs((kImageTtl / 2) - 2 * Timer::kMinuteMs);
  ValidateExpected("freshen", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(2, lru_cache()->num_hits());  // cf metadata, 1.jpg
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // cf metadata, 1.jpg
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());  // 1.jpg expiring soon
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());  // 1.jpg

  // Advance past the original TTL. There should be no cache miss and no
  // additional fetches as the resource is already freshened.
  ClearStats();
  AdvanceTimeMs(3 * Timer::kMinuteMs);
  ValidateExpected("past original ttl", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());

  // Advance time to Css ttl - 2 minutes. This should cause freshen of
  // both the resources.
  ClearStats();
  AdvanceTimeMs(kCssTtl - kImageTtl - 3 * Timer::kMinuteMs);
  ValidateExpected("past highest ttl", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(5, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(5, lru_cache()->num_inserts());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());  // old rewritten css
  EXPECT_EQ(2, http_cache()->cache_misses()->Get());
  EXPECT_EQ(3, http_cache()->cache_inserts()->Get());
}

TEST_F(RewriteContextTest, TestNoFreshenForEmbeddedDependency) {
  FetcherUpdateDateHeaders();
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  // proactive resource freshening is off by default so no need to disable it.
  EXPECT_FALSE(options()->proactive_resource_freshening());
  options()->set_proactive_resource_freshening(false);
  options()->ComputeSignature();
  rewrite_driver()->AddFilters();

  // Set up the resources and ttl. Ttl should be bigger than default implicit
  // cache ttl.
  const int kImageTtl = RewriteOptions::kDefaultImplicitCacheTtlMs * 5;
  const int kCssTtl = RewriteOptions::kDefaultImplicitCacheTtlMs * 10;
  const char kImageContent[] = "image1";
  const char kImagePath[] = "1.jpg";
  const char kCssPath[] = "text.css";
  GoogleString css_content = StrCat("{background:url(\"",
                                    AbsolutifyUrl("1.jpg"), "\")}");

  // Start with non-zero time and init the resources.
  AdvanceTimeMs(kImageTtl / 2);
  SetResponseWithDefaultHeaders(kImagePath, kContentTypeJpeg, kImageContent,
                                kImageTtl / Timer::kSecondMs);
  SetResponseWithDefaultHeaders(kCssPath, kContentTypeCss, css_content,
                                kCssTtl / Timer::kSecondMs);
  GoogleString css_url = AbsolutifyUrl("text.css");
  // Note: Output is absolute, because input is absolute.
  GoogleString rewritten_url =
      Encode(kTestDomain, "cf", "0", "text.css", "css");

  // First fetch misses cache and resources are inserted into the cache.
  ClearStats();
  ValidateExpected("first_fetch", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses());  // cf, ic, 1.jpg, original text.css
  EXPECT_EQ(5, lru_cache()->num_inserts());  // above + rewritten text.css
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, http_cache()->cache_misses()->Get());
  // text.css, 1.jpg, rewritten text.css get inserted in http cache.
  EXPECT_EQ(3, http_cache()->cache_inserts()->Get());

  // The ttl of the resource is the min of all its dependencies and hence
  // kImageTtl in this case. Advance halfway and it should be a hit.
  ClearStats();
  AdvanceTimeMs(kImageTtl / 2);
  ValidateExpected("fully hit", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());

  // Advance time close to the ttl of the image. This will not cause any
  // proactive freshening since we turned it off.
  ClearStats();
  AdvanceTimeMs((kImageTtl / 2) - 2 * Timer::kMinuteMs);
  ValidateExpected("freshen", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(1, lru_cache()->num_hits());  // test.css metadata
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());

  // Advance past the original TTL.  We weren't proactively freshening
  // the individual images that expired, but now all the resources
  // need to be re-fetched the cache entries updated.
  ClearStats();
  AdvanceTimeMs(3 * Timer::kMinuteMs);
  ValidateExpected("past original ttl", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(4, lru_cache()->num_hits());  // test.css MD/http, 1.jpg MD/http
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());

  // Advance time to Css ttl - 2 minutes. This will again cause no proactive
  // freshening since we turned that off, but test.css will be expired so we
  // will need to re-fetch it.  1.jpg will not have expired so we will not
  // re-fetch it or check its cache entry.
  ClearStats();
  AdvanceTimeMs(kCssTtl - kImageTtl - 3 * Timer::kMinuteMs);
  ValidateExpected("past highest ttl", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(2, lru_cache()->num_hits());  // test.css MD/http
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());  // old rewritten css
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
}

TEST_F(RewriteContextTest, TestReuse) {
  FetcherUpdateDateHeaders();

  // Test to make sure we are able to avoid rewrites when inputs don't
  // change even when they expire.

  const int kTtlMs = RewriteOptions::kDefaultImplicitCacheTtlMs;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";

  // Start with non-zero time, and init our resource..
  AdvanceTimeMs(kTtlMs / 2);
  InitTrimFilters(kRewrittenResource);
  SetResponseWithDefaultHeaders(kPath, kContentTypeCss, kDataIn,
                                kTtlMs / Timer::kSecondMs);

  // First fetch + rewrite.
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance time way past when it was expired, or even when it'd live with
  // freshening.
  AdvanceTimeMs(kTtlMs * 10);

  // This should fetch, but can avoid calling the filter's Rewrite function.
  ValidateExpected("forward",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // Advance some more --- make sure we fully hit from cache now (which
  // requires the previous operation to have updated it).
  AdvanceTimeMs(kTtlMs / 2);
  ValidateExpected("forward2",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TestFallbackOnFetchFails) {
  FetcherUpdateDateHeaders();
  InitTrimFilters(kRewrittenResource);
  EnableDebug();

  // Test to make sure we are able to serve stale resources if available when
  // the fetch fails.
  const int kTtlMs = RewriteOptions::kDefaultImplicitCacheTtlMs;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";
  const char kDataOut[] = "data";
  GoogleString rewritten_url = Encode("", "tw", "0", "test.css", "css");
  GoogleString abs_rewritten_url = StrCat(kTestDomain, rewritten_url);
  GoogleString response_content;
  ResponseHeaders response_headers;

  // Serve a 500 for the CSS file.
  ResponseHeaders bad_headers;
  bad_headers.set_first_line(1, 1, 500, "Internal Server Error");
  mock_url_fetcher()->SetResponse(AbsolutifyUrl(kPath), bad_headers, "");

  // First fetch. No rewriting happens since the fetch fails. We cache that the
  // fetch failed for kDefaultImplicitCacheTtlMs.
  GoogleString input_html = CssLinkHref(kPath);
  GoogleString fetch_failure_html =
      StrCat(input_html, "<!--Fetch failure, preventing rewriting of ",
             kTestDomain, kPath, "-->");
  ValidateExpected("initial_500", input_html, fetch_failure_html);
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(
      0,
      server_context()->rewrite_stats()->fallback_responses_served()->Get());

  ClearStats();
  // Advance the timer by less than kDefaultImplicitCacheTtlMs. Since we
  // remembered that the fetch failed, we don't trigger a fetch for the CSS and
  // don't rewrite it either.
  AdvanceTimeMs(kTtlMs / 2);
  ValidateExpected("forward_500", input_html, fetch_failure_html);
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(
      0,
      server_context()->rewrite_stats()->fallback_responses_served()->Get());

  ClearStats();

  // Advance the timer again so that the fetch failed is stale and update the
  // css response to a valid 200.
  AdvanceTimeMs(kTtlMs);
  SetResponseWithDefaultHeaders(kPath, kContentTypeCss, kDataIn,
                                kTtlMs / Timer::kSecondMs);

  // The resource is rewritten successfully.
  ValidateExpected("forward_200",
                   input_html,
                   CssLinkHref(rewritten_url));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // Two cache inserts for the original and rewritten resource.
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());
  EXPECT_TRUE(FetchResourceUrl(abs_rewritten_url, &response_content,
                               &response_headers));
  EXPECT_STREQ(kDataOut, response_content);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
  EXPECT_EQ(
      0,
      server_context()->rewrite_stats()->fallback_responses_served()->Get());

  ClearStats();

  // Advance time way past when it was expired. Set the css response to a 500
  // again and delete the rewritten url from cache. We don't rewrite the html.
  // Note that we don't overwrite the stale response for the css and serve a
  // valid 200 response to the rewrriten resource.
  AdvanceTimeMs(kTtlMs * 10);
  lru_cache()->Delete(HttpCacheKey(abs_rewritten_url));
  mock_url_fetcher()->SetResponse(AbsolutifyUrl(kPath), bad_headers, "");
  GoogleString expired_html =
      StrCat(input_html, "<!--Cached content expired, preventing rewriting of ",
             kTestDomain, kPath, "-->");

  ValidateExpected("forward_500_fallback_served", input_html, expired_html);
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(
      1,
      server_context()->rewrite_stats()->fallback_responses_served()->Get());

  response_headers.Clear();
  response_content.clear();
  EXPECT_TRUE(FetchResourceUrl(abs_rewritten_url, &response_content,
                               &response_headers));
  EXPECT_STREQ(kDataOut, response_content);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());

  // Disable serving of stale resources and delete the rewritten resource from
  // cache. We don't rewrite the html. We insert the fetch failure into cache
  // and are unable to serve the rewritten resource.
  options()->ClearSignatureForTesting();
  options()->set_serve_stale_if_fetch_error(false);
  options()->ComputeSignature();

  ClearStats();
  lru_cache()->Delete(HttpCacheKey(abs_rewritten_url));
  ValidateExpected("forward_500_no_fallback", input_html, fetch_failure_html);
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());
  EXPECT_EQ(
      0,
      server_context()->rewrite_stats()->fallback_responses_served()->Get());

  response_headers.Clear();
  response_content.clear();
  EXPECT_FALSE(FetchResourceUrl(abs_rewritten_url, &response_content,
                                &response_headers));
}

TEST_F(RewriteContextTest, TestOriginalImplicitCacheTtl) {
  options()->ClearSignatureForTesting();
  options()->set_metadata_cache_staleness_threshold_ms(0);
  options()->ComputeSignature();

  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";
  const GoogleString kOriginalRewriteUrl(Encode("", "tw", "0",
                                                "test.css", "css"));
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  // Do not call ComputeCaching before calling SetFetchResponse because it will
  // add an explicit max-age=300 cache control header. We do not want that
  // header in this test.
  SetFetchResponse(AbsolutifyUrl(kPath), headers, kDataIn);

  // Start with non-zero time, and init our resource..
  AdvanceTimeMs(100 * Timer::kSecondMs);
  InitTrimFilters(kRewrittenResource);
  // First fetch + rewrite.
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Resource should be in cache.
  ClearStats();
  AdvanceTimeMs(100 * Timer::kSecondMs);
  ValidateExpected("200sec",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // Advance time past original implicit cache ttl (300sec).
  SetupWaitFetcher();
  ClearStats();
  AdvanceTimeMs(200 * Timer::kSecondMs);
  // Resource is stale now.
  ValidateNoChanges("400sec", CssLinkHref(kPath));
  CallFetcherCallbacks();
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TestModifiedImplicitCacheTtl) {
  options()->ClearSignatureForTesting();
  options()->set_implicit_cache_ttl_ms(500 * Timer::kSecondMs);
  options()->set_metadata_cache_staleness_threshold_ms(0);
  options()->ComputeSignature();

  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";
  const GoogleString kOriginalRewriteUrl(Encode("", "tw", "0",
                                                "test.css", "css"));
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  // Do not call ComputeCaching before calling SetFetchResponse because it will
  // add an explicit max-age=300 cache control header. We do not want that
  // header in this test.
  SetFetchResponse(AbsolutifyUrl(kPath), headers, kDataIn);

  // Start with non-zero time, and init our resource..
  AdvanceTimeMs(100 * Timer::kSecondMs);
  InitTrimFilters(kRewrittenResource);
  // First fetch + rewrite.
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Resource should be in cache.
  ClearStats();
  AdvanceTimeMs(100 * Timer::kSecondMs);
  ValidateExpected("200sec",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // Advance time past original implicit cache ttl (300sec).
  ClearStats();
  AdvanceTimeMs(200 * Timer::kSecondMs);
  // Resource should still be in cache.
  ValidateExpected("400sec",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  FetcherUpdateDateHeaders();
  SetupWaitFetcher();
  ClearStats();
  AdvanceTimeMs(200 * Timer::kSecondMs);
  // Resource is stale now.
  ValidateNoChanges("600sec", CssLinkHref(kPath));
  CallFetcherCallbacks();
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  SetupWaitFetcher();
  ClearStats();
  AdvanceTimeMs(600 * Timer::kSecondMs);
  // Resource is still stale.
  ValidateNoChanges("1200sec", CssLinkHref(kPath));
  CallFetcherCallbacks();
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TestModifiedImplicitCacheTtlWith304) {
  options()->ClearSignatureForTesting();
  options()->set_implicit_cache_ttl_ms(500 * Timer::kSecondMs);
  options()->set_metadata_cache_staleness_threshold_ms(0);
  options()->ComputeSignature();

  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";
  const GoogleString kOriginalRewriteUrl(Encode("", "tw", "0",
                                                "test.css", "css"));
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.Add(HttpAttributes::kEtag, "new");
  headers.SetStatusAndReason(HttpStatus::kOK);
  // Do not call ComputeCaching before calling SetFetchResponse because it will
  // add an explicit max-age=300 cache control header. We do not want that
  // header in this test.
  mock_url_fetcher()->SetConditionalResponse(
      AbsolutifyUrl(kPath), -1, "new", headers, kDataIn);

  // Start with non-zero time, and init our resource..
  AdvanceTimeMs(100 * Timer::kSecondMs);
  InitTrimFilters(kRewrittenResource);
  // First fetch + rewrite.
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Resource should be in cache.
  ClearStats();
  AdvanceTimeMs(100 * Timer::kSecondMs);
  ValidateExpected("200sec",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // Advance time past original implicit cache ttl (300sec).
  ClearStats();
  AdvanceTimeMs(200 * Timer::kSecondMs);
  // Resource should still be in cache.
  ValidateExpected("400sec",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // Modify the implicit cache ttl.
  options()->ClearSignatureForTesting();
  options()->set_implicit_cache_ttl_ms(1000 * Timer::kSecondMs);
  options()->ComputeSignature();
  FetcherUpdateDateHeaders();

  SetupWaitFetcher();
  ClearStats();
  AdvanceTimeMs(200 * Timer::kSecondMs);
  // Resource is stale now. We got a 304 this time.
  ValidateNoChanges("600sec", CssLinkHref(kPath));
  CallFetcherCallbacks();
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, counting_url_async_fetcher()->byte_count());

  SetupWaitFetcher();
  ClearStats();
  AdvanceTimeMs(600 * Timer::kSecondMs);
  // Resource is fresh this time.
  ValidateExpected("1200sec",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  CallFetcherCallbacks();
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TestReuseNotFastEnough) {
  // Make sure we handle deadline passing when trying to reuse properly.
  FetcherUpdateDateHeaders();

  const int kTtlMs = RewriteOptions::kDefaultImplicitCacheTtlMs;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";

  // Start with non-zero time, and init our resource..
  AdvanceTimeMs(kTtlMs / 2);
  InitTrimFilters(kRewrittenResource);
  SetResponseWithDefaultHeaders(kPath, kContentTypeCss, kDataIn,
                                kTtlMs / Timer::kSecondMs);

  // First fetch + rewrite.
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance time way past when it was expired, or even when it'd live with
  // freshening.
  AdvanceTimeMs(kTtlMs * 10);

  // Make sure we can't check for freshening fast enough...
  SetupWaitFetcher();
  ValidateNoChanges("forward2.slow_fetch", CssLinkHref(kPath));

  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  CallFetcherCallbacks();
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  // Next time should be fine again, though.
  AdvanceTimeMs(kTtlMs / 2);
  ValidateExpected("forward2",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "0", "test.css", "css")));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, TestStaleRewriting) {
  FetcherUpdateDateHeaders();
  // We use MD5 hasher instead of mock hasher so that the rewritten url changes
  // when its content gets updated.
  UseMd5Hasher();

  const int kTtlMs = RewriteOptions::kDefaultImplicitCacheTtlMs;
  const char kPath[] = "test.css";
  const char kDataIn[] = "   data  ";
  const char kNewDataIn[] = "   newdata  ";
  const GoogleString kOriginalRewriteUrl(Encode("", "tw", "jXd_OF09_s",
                                                "test.css", "css"));

  options()->ClearSignatureForTesting();
  options()->set_metadata_cache_staleness_threshold_ms(kTtlMs / 2);
  options()->ComputeSignature();

  // Start with non-zero time, and init our resource..
  AdvanceTimeMs(kTtlMs / 2);
  InitTrimFilters(kRewrittenResource);
  SetResponseWithDefaultHeaders(kPath, kContentTypeCss, kDataIn,
                                kTtlMs / Timer::kSecondMs);

  // First fetch + rewrite.
  ValidateExpected("initial",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, metadata_cache_info().num_hits());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(1, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(1, metadata_cache_info().num_rewrites_completed());

  // Change the resource.
  SetResponseWithDefaultHeaders(kPath, kContentTypeCss, kNewDataIn,
                                kTtlMs / Timer::kSecondMs);

  // Advance time past when it was expired, but within the staleness threshold.
  AdvanceTimeMs((kTtlMs * 5)/4);

  ClearStats();
  // We continue to serve the stale resource.
  SetupWaitFetcher();
  // We continue to rewrite the resource with the old hash. However, we noticed
  // that the resource has changed, store it in cache and delete the old
  // metadata.
  ValidateExpected("stale",
                   CssLinkHref(kPath),
                   CssLinkHref(kOriginalRewriteUrl));
  EXPECT_EQ(0, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(1, metadata_cache_info().num_hits());
  EXPECT_EQ(1, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(0, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(1, metadata_cache_info().num_rewrites_completed());

  CallFetcherCallbacks();
  EXPECT_EQ(0, trim_filter_->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // Replacing the old resource with the new resource is also considered a cache
  // delete. The other delete is for the metadata.
  EXPECT_EQ(2, lru_cache()->num_deletes());

  ClearStats();
  // Next time, we serve the html with the new resource hash.
  ValidateExpected("freshened",
                   CssLinkHref(kPath),
                   CssLinkHref(Encode("", "tw", "nnVv_VJ4Xn",
                                      "test.css", "css")));
  EXPECT_EQ(1, metadata_cache_info().num_misses());
  EXPECT_EQ(0, metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, metadata_cache_info().num_hits());
  EXPECT_EQ(0, metadata_cache_info().num_stale_rewrites());
  EXPECT_EQ(0, metadata_cache_info().num_successful_revalidates());
  EXPECT_EQ(1, metadata_cache_info().num_successful_rewrites_on_miss());
  EXPECT_EQ(1, metadata_cache_info().num_rewrites_completed());
  CallFetcherCallbacks();
  EXPECT_EQ(1, trim_filter_->num_rewrites());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

// Even though the rewrite delay is more than the deadline, the rewrite is
// finished by the time the response is completely flushed.
TEST_F(RewriteContextTest, BlockingRewrite) {
  InitCombiningFilter(kRewriteDelayMs);
  EnableDebug();
  InitResources();
  GoogleString combined_url = Encode("", CombiningFilter::kFilterId,
                                     "0", MultiUrl("a.css", "b.css"), "css");
  rewrite_driver()->set_fully_rewrite_on_flush(true);

  ValidateExpected(
      "combination_rewrite", StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
      CssLinkHref(combined_url));
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // partition, and 2 inputs.
  EXPECT_EQ(4, lru_cache()->num_inserts());  // partition, output, and 2 inputs.
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

// See http://code.google.com/p/modpagespeed/issues/detail?id=494.  Make sure
// we apply domain-mapping when fetching resources, so that we get HTTP cache
// hits on the resource fetch based on the CSS file we optimized during the
// HTML rewrite.
TEST_F(RewriteContextTest, CssCdnMapToDifferentOrigin) {
  int64 start_time_ms = timer()->NowMs();
  UseMd5Hasher();
  DomainLawyer* lawyer = options()->WriteableDomainLawyer();
  InitNestedFilter(true);
  InitResources();
  lawyer->AddRewriteDomainMapping("test.com", "static.test.com",
                                  message_handler());
  const char kCdnOriginDomain[] = "http://static.test.com/";
  InitResourcesToDomain(kCdnOriginDomain);
  const char kHash[] = "WTYjEzrEWX";

  // The newline-separated list of URLS is the format used by the simple
  // nested rewriter used in testing.
  // Note: These tests do not use HtmlResourceSlots and thus they do not
  // preserve URL relativity.
  const GoogleString kRewrittenCssContents = StrCat(
      Encode(kTestDomain, UpperCaseRewriter::kFilterId, "lRGWyjVMXH", "a.css",
             "css"),
      "\n",
      Encode(kTestDomain, UpperCaseRewriter::kFilterId, "nV7WeP5XvM", "b.css",
             "css"),
      "\n");

  // First, rewrite the HTML.
  GoogleString rewritten_css = Encode("", NestedFilter::kFilterId,
                                      kHash, "c.css", "css");
  ValidateExpected("trimmable_async",
                   CssLinkHref("c.css"),
                   CssLinkHref(rewritten_css));

  // Now fetch this file from its "natural" domain -- the one that we wrote
  // into the HTML file.  This works fine, and did so even with Issue 494
  // broken.  This will hit cache and give long cache lifetimes.
  CheckFetchFromHttpCache(StrCat(kTestDomain, rewritten_css),
                          kRewrittenCssContents,
                          start_time_ms + Timer::kYearMs);

  // Now simulate an origin-fetch from a CDN, which has been instructed
  // to fetch from "static.test.com".  This requires proper domain-mapping
  // of Fetch urls to succeed.
  GoogleString cdn_origin_css = Encode(
      "http://static.test.com/", NestedFilter::kFilterId, kHash, "c.css",
      "css");
  CheckFetchFromHttpCache(cdn_origin_css, kRewrittenCssContents,
                          start_time_ms + Timer::kYearMs);
}

TEST_F(RewriteContextTest, CssCdnMapToDifferentOriginSharded) {
  int64 start_time_ms = timer()->NowMs();
  UseMd5Hasher();
  DomainLawyer* lawyer = options()->WriteableDomainLawyer();
  InitNestedFilter(true);
  InitResources();

  const char kShard1[] = "http://s1.com/";
  const char kShard2[] = "http://s2.com/";
  const char kCdnOriginDomain[] = "http://static.test.com/";

  lawyer->AddRewriteDomainMapping(kTestDomain, kCdnOriginDomain,
                                  message_handler());

  lawyer->AddShard(kTestDomain, StrCat(kShard1, ",", kShard2),
                   message_handler());
  InitResourcesToDomain(kCdnOriginDomain);
  const char kHash[] = "HeWbtJb3Ks";

  const GoogleString kRewrittenCssContents = StrCat(
      Encode(kShard1, UpperCaseRewriter::kFilterId, "lRGWyjVMXH", "a.css",
             "css"), "\n",
      Encode(kShard2, UpperCaseRewriter::kFilterId, "nV7WeP5XvM", "b.css",
             "css"), "\n");

  // First, rewrite the HTML.
  GoogleString rewritten_css = Encode(kShard2, NestedFilter::kFilterId, kHash,
                                      "c.css", "css");
  ValidateExpected("trimmable_async",
                   CssLinkHref("c.css"),
                   CssLinkHref(rewritten_css));

  // Now fetch this file from its "natural" domain -- the one that we wrote
  // into the HTML file.  This works fine, and did so even with Issue 494
  // broken.  This will hit cache and give long cache lifetimes.
  ClearStats();
  CheckFetchFromHttpCache(rewritten_css, kRewrittenCssContents,
                          start_time_ms + Timer::kYearMs);

  // Now simulate an origin-fetch from a CDN, which has been instructed
  // to fetch from "static.test.com".  This requires proper domain-mapping
  // of Fetch urls to succeed.
  ClearStats();
  GoogleString cdn_origin_css = Encode(
      kCdnOriginDomain, NestedFilter::kFilterId, kHash, "c.css", "css");
  CheckFetchFromHttpCache(cdn_origin_css, kRewrittenCssContents,
                          start_time_ms + Timer::kYearMs);

  // Check from either shard -- we should always be looking up based on
  // the rewrite domain.
  ClearStats();
  GoogleString shard1_css = Encode(
      kShard1, NestedFilter::kFilterId, kHash, "c.css", "css");
  CheckFetchFromHttpCache(shard1_css, kRewrittenCssContents,
                          start_time_ms + Timer::kYearMs);

  ClearStats();
  GoogleString shard2_css = Encode(
      kTestDomain, NestedFilter::kFilterId, kHash, "c.css", "css");
  CheckFetchFromHttpCache(shard2_css, kRewrittenCssContents,
                          start_time_ms + Timer::kYearMs);
}

TEST_F(RewriteContextTest, ShutdownBeforeFetch) {
  InitTrimFilters(kRewrittenResource);
  InitResources();
  factory()->ShutDown();
  GoogleString output;

  ResponseHeaders response_headers;
  EXPECT_FALSE(FetchResourceUrl(
      Encode(kTestDomain, "tw", "0", "b.css", "css"),
      &output, &response_headers));
  EXPECT_EQ(HttpStatus::kInternalServerError, response_headers.status_code());
}

TEST_F(RewriteContextTest, InlineContextWithImplicitTtl) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kInlineCss);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  options()->set_implicit_cache_ttl_ms(100 * Timer::kSecondMs);
  options()->set_css_inline_max_bytes(2);  // so css_inline filter will bail.
  options()->set_rewrite_deadline_ms(1);

  // Avoid noise by disabling other filters. This is so that only InlineCss
  // and CacheExtender filters are effecting the cache hits and misses.
  options()->SetRewriteLevel(RewriteOptions::kPassThrough);
  options()->ComputeSignature();
  rewrite_driver()->AddFilters();

  SetCacheDelayUs(2000);  // so that rewrite deadline is hit.
  ResponseHeaders headers;
  int64 now_ms = http_cache()->timer()->NowMs();
  const char kContent[] = "Example";
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.SetDateAndCaching(now_ms, 600 * Timer::kSecondMs);
  GoogleString css_url = AbsolutifyUrl("text.css");
  SetFetchResponse(css_url, headers, kContent);

  // Note: Output is absolute, because input is absolute.
  GoogleString rewritten_url =
      Encode(kTestDomain, "ce", "0", "text.css", "css");

  // The first request does not get rewritten because the deadline is 1 ms
  // and the cache delay is 2 ms. However, the rewrites happen asynchronously
  // in the background though the HTML is served out.
  ValidateNoChanges("ce_enabled", CssLinkHref(css_url));
  rewrite_driver()->WaitForCompletion();
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());  // ci, ce and original text.css
  EXPECT_EQ(3, lru_cache()->num_inserts());  // // ci, ce and original text.css
  ClearStats();

  // The resources are rewritten in the background and are ready for the
  // subsequent request.
  ValidateExpected("ce_enabled", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  rewrite_driver()->WaitForCompletion();
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  ClearStats();

  // Advance time to past 100 s which is the implicit cache ttl. We should get
  // the same cache hits as the resource ttl is used instead of implicit ttl.
  // Also the rewritten resource is served from the cache.
  AdvanceTimeMs(120 * Timer::kSecondMs);
  ValidateExpected("ce_enabled", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  rewrite_driver()->WaitForCompletion();
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  ClearStats();

  // Advance time by the ttl on the resource, now we should see a different
  // number of cache hits and misses. And the original resource is served
  // because the rewrite deadline is smaller than the cache delay. Also the
  // metadata is no longer valid because the resource ttl has expired.
  AdvanceTimeMs(600 * Timer::kSecondMs);
  GoogleString output;
  ResponseHeaders headers1;
  ValidateNoChanges("ce_enabled", CssLinkHref(css_url));
  rewrite_driver()->WaitForCompletion();
  // One extra lookup for text.css and its a hit.
  EXPECT_EQ(3, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  // All the resources expired and re-inserted. ci, ce and text.css.
  EXPECT_EQ(3, lru_cache()->num_inserts());
  ClearStats();
}

TEST_F(RewriteContextTest, CacheTtlWithDuplicateOtherDeps) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  options()->set_rewrite_deadline_ms(1);
  options()->set_proactive_resource_freshening(true);  // Enable dedup code.
  options()->ComputeSignature();
  rewrite_driver()->AddFilters();

  SetCacheDelayUs(2000);  // so that rewrite deadline is hit.
  ResponseHeaders headers;
  int64 now_ms = http_cache()->timer()->NowMs();
  const char kImageContent[] = "image1";
  headers.Add(HttpAttributes::kContentType, kContentTypeJpeg.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.SetDateAndCaching(now_ms, 200 * Timer::kSecondMs);
  GoogleString image_url = AbsolutifyUrl("1.jpg");
  SetFetchResponse(image_url, headers, kImageContent);

  GoogleString css_content = StrCat("{background:url(\"",
                                    AbsolutifyUrl("1.jpg"), "\")}");
  // Have duplicate entries to trigger the de-dup code for other dependencies.
  GoogleString duplicate_css_content = StrCat(css_content, css_content);
  headers.Clear();
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.SetDateAndCaching(now_ms, 600 * Timer::kSecondMs);
  GoogleString css_url = AbsolutifyUrl("text.css");
  SetFetchResponse(css_url, headers, duplicate_css_content);

  // Note: Output is absolute, because input is absolute.
  GoogleString rewritten_url =
      Encode(kTestDomain, "cf", "0", "text.css", "css");

  // The first request is not rewritten as there is cache miss and rewrite
  // deadline is small.
  ValidateNoChanges("cf_no_changes_1", CssLinkHref(css_url));
  rewrite_driver()->WaitForCompletion();
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses());  // cf, ic, 1.jpg, original text.css
  EXPECT_EQ(5, lru_cache()->num_inserts());  // above + rewritten text.css
  ClearStats();

  // The subsequent request should see a cache hit and no misses or inserts.
  ValidateExpected("cf_rewritten_2", CssLinkHref(css_url),
                   CssLinkHref(rewritten_url));
  rewrite_driver()->WaitForCompletion();  // cf metadata cache hit
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  ClearStats();

  // Advance time by the shorter of the ttls of the resources. We should see
  // cache inserts as the metadata expired. And the css file is not rewritten
  // as the rewrite deadline is too short.
  AdvanceTimeMs(220 * Timer::kSecondMs);
  ValidateNoChanges("cf_md_cache_miss", CssLinkHref(css_url));
  rewrite_driver()->WaitForCompletion();
  EXPECT_EQ(4, lru_cache()->num_hits());  // cf, ic, 1.jpg, original text.css
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(4, lru_cache()->num_inserts());
  ClearStats();
}

TEST_F(RewriteContextTest, DropFetchesAndRecover) {
  // Construct some HTML with more resources to fetch than our rate-limiting
  // fetcher will allow.
  InitUpperFilter(kRewrittenResource, rewrite_driver());
  SetupWaitFetcher();
  options()->ComputeSignature();
  rewrite_driver()->AddFilters();

  // Build HTML content that has 33 more CSS links than we can queue up due
  // to rate-limited fetching.
  const int kExcessResources =
      TestRewriteDriverFactory::kFetchesPerHostOutgoingRequestThreshold / 3;
  const int kResourceCount =
      TestRewriteDriverFactory::kMaxFetchGlobalQueueSize +
      TestRewriteDriverFactory::kFetchesPerHostOutgoingRequestThreshold +
      kExcessResources;
  GoogleString html;
  for (int i = 0; i < kResourceCount; ++i) {
    GoogleString url = StringPrintf("x%d.css", i);
    GoogleString content = StringPrintf("a%d", i);  // Rewriter will upper-case.
    SetResponseWithDefaultHeaders(url, kContentTypeCss, content, 100 /* sec */);
    StrAppend(&html, CssLinkHref(url));
  }

  // Rewrite the HTML.  None of the fetches will be done before the deadline,
  // so no changes will be made to the HTML.
  ValidateNoChanges("no_changes_call_all_fetches_delayed", html);

  // Let's take a look at the rate-controlling fetcher's stats and make
  // sure they are sane.
  UpDownCounter* fetch_queue_size = statistics()->GetUpDownCounter(
      RateController::kCurrentGlobalFetchQueueSize);
  EXPECT_EQ(TestRewriteDriverFactory::kFetchesPerHostQueuedRequestThreshold,
            TimedValue(RateController::kQueuedFetchCount));
  EXPECT_EQ(kExcessResources, TimedValue(RateController::kDroppedFetchCount));
  EXPECT_EQ(TestRewriteDriverFactory::kMaxFetchGlobalQueueSize,
            fetch_queue_size->Get());

  // Now let the fetches all go -- the ones that weren't dropped, anyway.
  factory()->wait_url_async_fetcher()->SetPassThroughMode(true);
  rewrite_driver()->WaitForCompletion();

  // Having waited for those fetches to complete, there are no more queued.
  EXPECT_EQ(0, fetch_queue_size->Get());

  // And the rewritten page will have all but kExcessResources rewritten.
  int num_unrewritten_css = RewriteAndCountUnrewrittenCss("1st_round", html);
  EXPECT_EQ(kExcessResources, num_unrewritten_css);

  // OK that's not a very happy state.  Even after we let the fetches
  // finish, an immediate page refresh still won't get the entire page
  // rewritten.  But we can't grow the dropped-request list unbounded.
  // The important thing is that we can recover in a limited amount of
  // time, say, 10.001 seconds.
  AdvanceTimeMs(http_cache()->remember_fetch_dropped_ttl_seconds() *
                Timer::kSecondMs + 1);

  // OK now all is well.  Note that if we had a lot more fetches beyond our
  // max queue size, we might have to wait another 10 seconds for another
  // round of fetches to make it through.  So initiate another HTML rewrite
  // which will queue up the fetches that were previously dropped.  But we
  // delay them so they don't show up within the deadline first.  The fetches
  // will be queued up and not dropped, but we are still see kExcessResource
  // unrewritten resources.
  factory()->wait_url_async_fetcher()->SetPassThroughMode(false);
  num_unrewritten_css = RewriteAndCountUnrewrittenCss("10.001_delay", html);
  EXPECT_EQ(kExcessResources, num_unrewritten_css);

  // Release the fetches.  The HTML will be fully rewritten on the next refresh.
  factory()->wait_url_async_fetcher()->SetPassThroughMode(true);
  rewrite_driver()->WaitForCompletion();
  num_unrewritten_css = RewriteAndCountUnrewrittenCss("10.001_release", html);
  EXPECT_EQ(0, num_unrewritten_css);
}

TEST_F(RewriteContextTest, AbandonRedundantFetchInHTML) {
  // Test that two nearly-simultaneous HTML requests which contain the same
  // resource result in a single rewrite and fetch. We simulate this by
  // rewriting on two RewriteDrivers with the same ServerContext. The first
  // rewrite acquires the creation lock and then delays the rewrite. The second
  // rewrite is not willing to delay the rewrite but abandons its rewrite
  // attempt because it can't acquire the lock.

  // Replace the other_rewrite_driver_ with one that's derived from the
  // same ServerContext as the primary one, as that's a better test of
  // shared locking and multiple rewrites on the same task.
  RewriteOptions* new_options_ = other_options_->Clone();
  delete other_rewrite_driver_;
  other_rewrite_driver_ = MakeDriver(server_context_, new_options_);
  InitResources();

  // We use fake filters since they provide delayed rewriter functionality.
  FakeFilter* fake1 =
      new FakeFilter(TrimWhitespaceRewriter::kFilterId, rewrite_driver_,
                     semantic_type::kStylesheet);
  fake1->set_exceed_deadline(true);
  rewrite_driver_->AppendRewriteFilter(fake1);
  rewrite_driver_->AddFilters();
  FakeFilter* fake2 =
      new FakeFilter(TrimWhitespaceRewriter::kFilterId, other_rewrite_driver_,
                     semantic_type::kStylesheet);
  fake2->set_exceed_deadline(false);  // This is default, but being explicit.
  other_rewrite_driver_->AppendRewriteFilter(fake2);
  other_rewrite_driver_->AddFilters();

  // Optimize the page once
  const GoogleString encoded =
      Encode("", TrimWhitespaceRewriter::kFilterId, "0", "a.css", "css");
  ValidateNoChanges("trimmable", CssLinkHref("a.css"));

  // Optimize the same page again but with a driver that doesn't have a delayed
  // rewriter.
  SetActiveServer(kSecondary);
  ValidateNoChanges("trimmable2", CssLinkHref("a.css"));

  SetActiveServer(kPrimary);
  EXPECT_EQ(0, fake1->num_rewrites());
  EXPECT_EQ(0, fake2->num_rewrites());
  // Note: The lru_cache is shared between the two drivers.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());   // meta twice and http once
  EXPECT_EQ(1, lru_cache()->num_inserts());  // original resource
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Advance the time and make sure the rewrite does eventually complete.
  ClearStats();
  AdvanceTimeMs(1);  // The fake filter waits until just after the deadline.
  EXPECT_EQ(1, fake1->num_rewrites());
  EXPECT_EQ(0, fake2->num_rewrites());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // metadata and optimized
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());

  // Make sure that we can fetch it.
  ClearStats();
  ValidateExpected("trimmable3", CssLinkHref("a.css"), CssLinkHref(encoded));
  EXPECT_EQ(1, fake1->num_rewrites());  // ClearStats didn't clear this.
  EXPECT_EQ(0, fake2->num_rewrites());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
}

TEST_F(RewriteContextTest, WaitForRedundantRewriteInFetchAfterHtml) {
  // Test that an HTML request with a resource followed by a reconstruction
  // request for the same resource only rewrites and fetches once. We simulate
  // this by rewriting on two RewriteDrivers with the same ServerContext. The
  // first rewrite acquires the creation lock and then delays the rewrite. The
  // second waits for the first to complete.

  // Replace the other_rewrite_driver_ with one that's derived from the
  // same ServerContext as the primary one, as that's a better test of
  // shared locking and multiple rewrites on the same task.
  RewriteOptions* new_options_ = other_options_->Clone();
  delete other_rewrite_driver_;
  other_rewrite_driver_ = MakeDriver(server_context_, new_options_);
  InitResources();

  // We use fake filters since they provide delayed rewriter functionality.
  FakeFilter* fake1 =
      new FakeFilter(TrimWhitespaceRewriter::kFilterId, rewrite_driver_,
                     semantic_type::kStylesheet);
  fake1->set_exceed_deadline(true);
  rewrite_driver_->AppendRewriteFilter(fake1);
  rewrite_driver_->AddFilters();
  FakeFilter* fake2 =
      new FakeFilter(TrimWhitespaceRewriter::kFilterId, other_rewrite_driver_,
                     semantic_type::kStylesheet);
  fake2->set_exceed_deadline(false);  // This is default, but being explicit.
  other_rewrite_driver_->AppendRewriteFilter(fake2);
  other_rewrite_driver_->AddFilters();

  // Optimize the page once
  ValidateNoChanges("trimmable", CssLinkHref("a.css"));
  EXPECT_EQ(0, fake1->num_rewrites());
  EXPECT_EQ(0, fake2->num_rewrites());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Optimize the same resource as a .pagespeed. resource on a driver that
  // doesn't have a delayed rewriter. It should wait for the first rewrite to
  // finish and return the optimized result, but we should have only fetched and
  // optimized once.
  ClearStats();
  SetActiveServer(kSecondary);
  GoogleString content;
  EXPECT_TRUE(FetchResource(kTestDomain, TrimWhitespaceRewriter::kFilterId,
                            "a.css", "css", &content));
  EXPECT_EQ(StrCat(" a :", TrimWhitespaceRewriter::kFilterId), content);

  SetActiveServer(kPrimary);
  // The initial http cache lookup will fail.  Then the lock attempt will block.
  // By the time the lock is released the metadata and retry at the http cache
  // will succeed.
  // Note: The lru_cache is shared between the two drivers.
  EXPECT_EQ(2, lru_cache()->num_hits());     // meta and http of the fetch
  EXPECT_EQ(1, lru_cache()->num_misses());   // http of the html request
  EXPECT_EQ(2, lru_cache()->num_inserts());  // meta and http of the fetch
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, fake1->num_rewrites());
  EXPECT_EQ(0, fake2->num_rewrites());
}

TEST_F(RewriteContextTest, WaitForRedundantFetchInFetchAfterFetch) {
  // Test that a .pagespeed. fetch for a resource followed by another
  // .pagespeed. fetch for the same resource only rewrites and fetches from
  // origin once. We simulate this by rewriting on two RewriteDrivers with the
  // same ServerContext. The first fetch acquires the creation lock and then
  // delays the rewrite. The second fetch1 waits for the first to complete.

  // Replace the other_rewrite_driver_ with one that's derived from the
  // same ServerContext as the primary one, as that's a better test of
  // shared locking and multiple rewrites on the same task.
  RewriteOptions* new_options_ = other_options_->Clone();
  delete other_rewrite_driver_;
  other_rewrite_driver_ = MakeDriver(server_context_, new_options_);
  InitResources();

  // We use fake filters since they provide delayed rewriter functionality.
  FakeFilter* fake1 =
      new FakeFilter(TrimWhitespaceRewriter::kFilterId, rewrite_driver_,
                     semantic_type::kStylesheet);
  fake1->set_exceed_deadline(true);
  rewrite_driver_->AppendRewriteFilter(fake1);
  rewrite_driver_->AddFilters();
  FakeFilter* fake2 =
      new FakeFilter(TrimWhitespaceRewriter::kFilterId, other_rewrite_driver_,
                     semantic_type::kStylesheet);
  fake2->set_exceed_deadline(false);  // This is default, but being explicit.
  other_rewrite_driver_->AppendRewriteFilter(fake2);
  other_rewrite_driver_->AddFilters();

  // Do a .pagespeed. fetch but delay the rewrite until past the deadline.
  GoogleString content1;
  StringAsyncFetch async_fetch(rewrite_driver_->request_context(), &content1);
  GoogleString url = Encode(kTestDomain, TrimWhitespaceRewriter::kFilterId, "0",
                            "a.css", "css");
  // Note that this is rewrite_driver_->FetchResource and not
  // RewriteTestBase::FetchResource, therefore WaitForShutdown will not be
  // called.
  EXPECT_TRUE(rewrite_driver_->FetchResource(url, &async_fetch));

  // Verify that the rewrite is still pending, so the lock should still be held.
  EXPECT_EQ(0, fake1->num_rewrites());

  // Fetch again on a driver that doesn't have a delayed rewriter. It should
  // wait for the first rewrite to finish and return the optimized result, but
  // we should have only fetched and optimized once.
  SetActiveServer(kSecondary);
  GoogleString content2;
  EXPECT_TRUE(FetchResource(kTestDomain, TrimWhitespaceRewriter::kFilterId,
                            "a.css", "css", &content2));
  EXPECT_EQ(StrCat(" a :", TrimWhitespaceRewriter::kFilterId), content2);

  SetActiveServer(kPrimary);
  // Let the first driver wrap up.
  rewrite_driver_->WaitForShutDown();

  // We have the stats for both rewrites here:
  // Fetch 1: http, metadata, and original resource misses, one fetch, and three
  // inserts.
  // Fetch 2: http miss, lock, metadata hit, http hit and return.
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, fake1->num_rewrites());
  EXPECT_EQ(0, fake2->num_rewrites());
}

}  // namespace net_instaweb
