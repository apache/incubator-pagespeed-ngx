/*
 * Copyright 2013 Google Inc.
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

// Author: jkarlin@google.com (Josh Karlin)

// Unit-test the distributed pathways through the RewriteContext class.

#include "net/instaweb/rewriter/public/rewrite_context.h"

#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"  // for Code::kOK
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/rewrite_context_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/test_distributed_fetcher.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
namespace {

// A class for testing the distributed paths through the rewrite context. It
// uses the RewriteContextTestBase's "other" RewriteDriver, factory, and options
// as a second task to perform distributed rewrites on. Call
// SetupDistributedTest to configure the test class.
class DistributedRewriteContextTest : public RewriteContextTestBase {
 protected:
  DistributedRewriteContextTest() {
    distributed_rewrite_failures_ = statistics()->GetVariable(
        RewriteContext::kNumDistributedRewriteFailures);
    distributed_rewrite_successes_ = statistics()->GetVariable(
        RewriteContext::kNumDistributedRewriteSuccesses);
    fetch_failures_ =
        statistics()->GetVariable(RewriteStats::kNumResourceFetchFailures);
    fetch_successes_ =
        statistics()->GetVariable(RewriteStats::kNumResourceFetchSuccesses);
  }

  // Sets the options to be the same for the two tasks and configures a shared
  // LRU cache between them. Note that when a distributed call is made, the
  // fetcher will call the RewriteContextTestBase's "other" driver directly (see
  // TestDistributedFetcher).
  void SetupDistributedTest() {
    SetupSharedCache();
    options_->DistributeFilter(TrimWhitespaceRewriter::kFilterId);
    options_->set_distributed_rewrite_servers("example.com:80");
    // Make sure they have the same options so that they generate the same
    // metadata keys.
    other_options()->Merge(*options());
    InitTrimFilters(kRewrittenResource);
    InitResources();
  }

  void CheckDistributedFetch(int distributed_fetch_success_count,
                             bool local_fetch_required,
                             bool distributed_fetch_required, bool rewritten) {
    EXPECT_EQ(1, counting_distributed_fetcher()->fetch_count());
    EXPECT_EQ(local_fetch_required,
              counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(
        0, other_factory_->counting_distributed_async_fetcher()->fetch_count());
    EXPECT_EQ(distributed_fetch_required,
              other_factory_->counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(distributed_fetch_success_count,
              distributed_rewrite_successes_->Get());
    EXPECT_EQ(!distributed_fetch_success_count,
              distributed_rewrite_failures_->Get());
    EXPECT_EQ(0, trim_filter_->num_rewrites());
    EXPECT_EQ(rewritten, other_trim_filter_->num_rewrites());
  }

  Variable* fetch_failures_;
  Variable* fetch_successes_;
  Variable* distributed_rewrite_failures_;
  Variable* distributed_rewrite_successes_;
};

}  // namespace

// Distribute a .pagespeed. reconstruction.
TEST_F(DistributedRewriteContextTest, IngressDistributedRewriteFetch) {
  SetupDistributedTest();
  GoogleString encoded_url = Encode(
      kTestDomain, TrimWhitespaceRewriter::kFilterId, "0", "a.css", "css");

  // Fetch the .pagespeed. resource and ensure that the rewrite was distributed.
  GoogleString content;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  EXPECT_TRUE(FetchResourceUrl(encoded_url, &request_headers, &content,
                               &response_headers));
  // Content should be optimized.
  EXPECT_EQ("a", content);

  CheckDistributedFetch(1,      // distributed fetch success count
                        false,  // ingress fetch required
                        true,   // distributed fetch required
                        true);  // rewrite

  // Ingress task misses on two HTTP lookups (check twice for rewritten
  // resource) and one metadata lookup.
  // Rewrite task misses on three HTTP lookups (twice for rewritten resource
  // plus once for original resource) and one metdata lookup. Then inserts
  // original resource, optimized resource, and metadata.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(7, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(5, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());

  // On the second .pagespeed. request the optimized resource should be in the
  // shared cache.
  ClearStats();
  EXPECT_TRUE(FetchResourceUrl(encoded_url, &request_headers, &content,
                               &response_headers));

  // Content should be optimized.
  EXPECT_EQ("a", content);

  // The distributed fetcher should not have run.
  EXPECT_EQ(0, counting_distributed_fetcher()->fetch_count());

  // Ingress task hits on one HTTP lookup and returns it.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

// If the distributed fetcher returns a 404 then that's what should be
// returned.
TEST_F(DistributedRewriteContextTest, IngressDistributedRewriteNotFoundFetch) {
  SetupDistributedTest();
  GoogleString orig_url = StrCat(kTestDomain, "fourofour.css");
  GoogleString encoded_url =
      Encode(kTestDomain, TrimWhitespaceRewriter::kFilterId, "0",
             "fourofour.css", "css");
  SetFetchResponse404(orig_url);

  // Fetch the .pagespeed. resource and ensure that the rewrite gets
  // distributed.
  GoogleString content;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;

  EXPECT_FALSE(FetchResourceUrl(encoded_url, &request_headers, &content,
                                &response_headers));
  // Should be a 404 response.
  EXPECT_EQ(HttpStatus::kNotFound, response_headers.status_code());

  // The distributed fetcher should have run once on the ingress task and the
  // url fetcher should have run once on the rewrite task.  The result goes to
  // shared cache.
  CheckDistributedFetch(0,       // distributed fetch success count
                        false,   // ingress fetch required
                        true,    // distributed fetch required
                        false);  // rewrite

  // Ingress task misses on two HTTP lookups (check twice for rewritten
  // resource) and one metadata lookup.  Then hits on the 404'd resource.
  // Rewrite task misses on three HTTP lookups (twice for rewritten resource
  // plus once for original resource) and one metdata lookup. Then inserts
  // 404'd original resource and metadata.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(7, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_inserts());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(6, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());

  // Fetching again causes another reconstruction and therefore another
  // distributed rewrite, even though we hit the 404 in cache.
  //
  // ingress task: 2 .pagespeed, misses, 1 metadata hit, 1 http hit, then
  // distribute because 404, it fails (because 404) so fetch locally and hit.
  // Return.
  //
  // rewrite task: 2 .pagespeed. misses, 1 metadata hit, 1 http hit, then fetch
  // again because 404, fetch locally and hit. Return.
  ClearStats();
  EXPECT_FALSE(FetchResourceUrl(encoded_url, &request_headers, &content,
                                &response_headers));
  CheckDistributedFetch(0,       // distributed fetch success count
                        false,   // ingress fetch required
                        false,   // distributed fetch required
                        false);  // rewrite
  EXPECT_EQ(6, lru_cache()->num_hits());
  EXPECT_EQ(4, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_inserts());
}

// Simulate distributed fetch failure and ensure that we fall back to the
// original.
TEST_F(DistributedRewriteContextTest,
       IngressDistributedRewriteFailFallbackFetch) {
  SetupDistributedTest();
  test_distributed_fetcher()->set_fail_after_headers(true);

  // Mock the optimized .pagespeed. response from the rewrite task.
  GoogleString encoded_url = Encode(
      kTestDomain, TrimWhitespaceRewriter::kFilterId, "0", "a.css", "css");

  GoogleString content;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  EXPECT_TRUE(FetchResourceUrl(encoded_url, &request_headers, &content,
                               &response_headers));

  EXPECT_STREQ(" a ", content);

  // Ingress task distributes, which fails, but pick up original resource from
  // shared cache.
  CheckDistributedFetch(0,      // distributed fetch success count
                        false,  // ingress fetch required
                        true,   // distributed fetch required
                        true);  // rewrite
  // Ingress task: Misses http cache twice, then metadata. Distributed rewrite
  // fails, so fetches original (a hit because of shared cache), and returns.
  // Distributed task: Misses http cache twice, then metadata. Fetches original
  // (misses in process), writes it, optimizes, writes optimized, and writes
  // metadata.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(7, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_inserts());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(5, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, http_cache()->cache_inserts()->Get());
}

}  // namespace net_instaweb
