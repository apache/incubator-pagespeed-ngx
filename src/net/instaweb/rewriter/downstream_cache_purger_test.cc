/*
 * Copyright 2014 Google Inc.
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

// Author: anupama@google.com (Anupama Dutta)

#include "net/instaweb/rewriter/public/downstream_cache_purger.h"

#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/google_url.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/request_headers.h"

namespace net_instaweb {

class DownstreamCachePurgerTest : public RewriteTestBase {
 protected:
  void PrepareForPurgeTest(StringPiece downstream_cache_purge_method,
                           StringPiece downstream_cache_purge_location_prefix,
                           const GoogleUrl& google_url,
                           bool is_original_request_post,
                           int num_initiated_rewrites,
                           int num_detached_rewrites) {
    RequestHeaders request_headers;
    if (is_original_request_post) {
      request_headers.set_method(RequestHeaders::kPost);
    }
    rewrite_driver()->SetRequestHeaders(request_headers);

    SetDownstreamCacheDirectives(downstream_cache_purge_method,
                                 downstream_cache_purge_location_prefix, "");
    // Setup a fake response for the expected purge path.
    while (downstream_cache_purge_location_prefix.ends_with("/")) {
      downstream_cache_purge_location_prefix.remove_suffix(1);
    }
    GoogleString cache_purge_url = StrCat(
        downstream_cache_purge_location_prefix, google_url.PathAndLeaf());
    SetResponseWithDefaultHeaders(cache_purge_url, kContentTypeCss, "", 100);

    rewrite_driver()->set_num_initiated_rewrites(num_initiated_rewrites);
    rewrite_driver()->set_num_detached_rewrites(num_detached_rewrites);
  }
};

TEST_F(DownstreamCachePurgerTest, TestNullRequestHeaders) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl google_url("http://www.example.com/");
  EXPECT_FALSE(dcache.MaybeIssuePurge(google_url));
}

TEST_F(DownstreamCachePurgerTest, TestInvalidUrl) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl invalid_google_url("invalid_url");
  EXPECT_FALSE(dcache.MaybeIssuePurge(invalid_google_url));
}

TEST_F(DownstreamCachePurgerTest, TestPurgeRequestHeaderPresent) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl google_url("http://www.example.com/");

  RequestHeaders request_headers;
  request_headers.Add(kPsaPurgeRequest, "1");
  rewrite_driver()->SetRequestHeaders(request_headers);
  EXPECT_FALSE(dcache.MaybeIssuePurge(google_url));
}

TEST_F(DownstreamCachePurgerTest, TestDownstreamCachePurgerDisabled) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl google_url("http://www.example.com/");
  PrepareForPurgeTest("", "", google_url, false, 100, 6);
  EXPECT_FALSE(dcache.MaybeIssuePurge(google_url));
}

TEST_F(DownstreamCachePurgerTest, TestDownstreamCachePurgerNoPurgeMethod) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl google_url("http://www.example.com/");
  PrepareForPurgeTest("", "http://localhost:1234/purge", google_url, false, 100,
                      6);
  EXPECT_FALSE(dcache.MaybeIssuePurge(google_url));
}

TEST_F(DownstreamCachePurgerTest, TestPercentageRewrittenAboveThreshold) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl google_url("http://www.example.com/");
  // With 96% rewritten before the response was sent out, there should
  // be no purge.
  PrepareForPurgeTest("", "http://localhost:1234/purge", google_url, false, 100,
                      4);
  EXPECT_FALSE(dcache.MaybeIssuePurge(google_url));
}

TEST_F(DownstreamCachePurgerTest, TestNumInitiatedRewritesZero) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl google_url("http://www.example.com/");
  // With numbers indicating there was nothing to rewrite, there should
  // be no purge
  PrepareForPurgeTest("", "http://localhost:1234/purge", google_url, false, 0,
                      0);
  EXPECT_FALSE(dcache.MaybeIssuePurge(google_url));
}

TEST_F(DownstreamCachePurgerTest, TestPercentageRewrittenBelowThreshold) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl google_url("http://www.example.com/");
  // With 94% rewritten before the response was sent out, there should
  // be a purge.
  PrepareForPurgeTest("GET", "http://localhost:1234/purge", google_url, false,
                      100, 6);

  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_TRUE(dcache.MaybeIssuePurge(google_url));
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_STREQ("http://localhost:1234/purge/",
               counting_url_async_fetcher()->most_recent_fetched_url());
  EXPECT_EQ(1, factory()->rewrite_stats()->
                   downstream_cache_purge_attempts()->Get());

  // A second purge attempt will fail because made_downstream_purge_attempt_
  // will be true already.
  EXPECT_FALSE(dcache.MaybeIssuePurge(google_url));
}

TEST_F(DownstreamCachePurgerTest, TestWithPurgeMethod) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl google_url("http://www.example.com/");
  // With 94% rewritten before the response was sent out, there should
  // be a purge even if the purge method is specified as PURGE.
  PrepareForPurgeTest("PURGE", "http://localhost:1234/purge", google_url, false,
                      100, 6);

  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_TRUE(dcache.MaybeIssuePurge(google_url));
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_STREQ("http://localhost:1234/purge/",
               counting_url_async_fetcher()->most_recent_fetched_url());
  EXPECT_EQ(1, factory()->rewrite_stats()->
                   downstream_cache_purge_attempts()->Get());

  // A second purge attempt will fail because made_downstream_purge_attempt_
  // will be true already.
  EXPECT_FALSE(dcache.MaybeIssuePurge(google_url));
}

TEST_F(DownstreamCachePurgerTest, TestWithPostOnOriginalRequest) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl google_url("http://www.example.com/");
  // With 94% rewritten before the response was sent out, there should
  // be no purge if the original request had its method set to something
  // other than GET.
  PrepareForPurgeTest("PURGE", "http://localhost:1234/purge", google_url, true,
                      100, 6);

  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_FALSE(dcache.MaybeIssuePurge(google_url));
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, factory()->rewrite_stats()->
                   downstream_cache_purge_attempts()->Get());
}

// Issue #921. Verify that trailing slashes from purge URLs are removed so that
// the purge request URL doesn't have double slashes in it.
TEST_F(DownstreamCachePurgerTest, TestPurgeUrlTrailingSlash) {
  DownstreamCachePurger dcache(rewrite_driver());
  GoogleUrl google_url("http://www.example.com/example.html");
  PrepareForPurgeTest("PURGE", "http://localhost:1234/purge/", google_url,
                      false, 100, 6);
  EXPECT_TRUE(dcache.MaybeIssuePurge(google_url));
  EXPECT_STREQ("http://localhost:1234/purge/example.html",
               counting_url_async_fetcher()->most_recent_fetched_url());
}

}  // namespace net_instaweb
