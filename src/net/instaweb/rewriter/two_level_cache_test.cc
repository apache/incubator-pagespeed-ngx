/*
 * Copyright 2012 Google Inc.
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

// Author: sriharis@google.com (Srihari Sukumaran)

// Test the interaction of L1 and L2 cache for the metadata cache.

#include <utility>

#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/rewrite_context_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/write_through_cache.h"

namespace net_instaweb {

class CacheInterface;
class MockUrlFetcher;
class TestDistributedFetcher;

namespace {

// A test rewrite driver factory for setting up metadata cache either as a write
// through cache (with two LRUCaches) or as a single level cache using the
// second LRUCache.  CustomRewriteDriverFactory objects are built via the static
// function MakeFactories(), which returns are a pair of factories to be used in
// constructing RewriteTestBase.  The LRUCaches are shared accross both
// instances of this returned by MakeFactories -- they are owned by the first
// instance of this class, while the other only keeps pointers.
class CustomRewriteDriverFactory : public TestRewriteDriverFactory {
 public:
  static std::pair<TestRewriteDriverFactory*, TestRewriteDriverFactory*>
      MakeFactories(MockUrlFetcher* mock_url_fetcher,
                    TestDistributedFetcher* mock_distributed_fetcher) {
    CustomRewriteDriverFactory* factory1 = new CustomRewriteDriverFactory(
        mock_url_fetcher, mock_distributed_fetcher,
        true /* Use write through cache */, 1000);
    CustomRewriteDriverFactory* factory2 = new CustomRewriteDriverFactory(
        mock_url_fetcher, mock_distributed_fetcher,
        false /* Do not use write through cache */, factory1->owned_cache1(),
        factory1->owned_cache2());
    return std::make_pair(factory1, factory2);
  }

  virtual void SetupCaches(ServerContext* server_context) {
    server_context->set_http_cache(
        new HTTPCache(cache1_, timer(), hasher(), statistics()));
    if (use_write_through_cache_) {
      CacheInterface* write_through = new WriteThroughCache(cache1_, cache2_);
      server_context->set_metadata_cache(write_through);
      server_context->DeleteCacheOnDestruction(write_through);
    } else {
      server_context->set_metadata_cache(cache2_);
    }
    server_context->MakePagePropertyCache(
        server_context->CreatePropertyStore(cache2_));
    server_context->set_enable_property_cache(false);
  }

  LRUCache* cache1() const { return cache1_; }
  LRUCache* cache2() const { return cache2_; }
  void set_caches(LRUCache* cache1, LRUCache* cache2) {
    cache1_ = cache1;
    cache2_ = cache2;
  }

  LRUCache* owned_cache1() const { return owned_cache1_.get(); }
  LRUCache* owned_cache2() const { return owned_cache2_.get(); }

 private:
  CustomRewriteDriverFactory(MockUrlFetcher* url_fetcher,
                             TestDistributedFetcher* distributed_fetcher,
                             bool use_write_through_cache,
                             int cache_size)
      : TestRewriteDriverFactory(GTestTempDir(), url_fetcher,
                                 distributed_fetcher),
        owned_cache1_(new LRUCache(cache_size)),
        owned_cache2_(new LRUCache(cache_size)),
        cache1_(owned_cache1_.get()),
        cache2_(owned_cache2_.get()),
        use_write_through_cache_(use_write_through_cache) {
    InitializeDefaultOptions();
  }

  CustomRewriteDriverFactory(MockUrlFetcher* url_fetcher,
                             TestDistributedFetcher* distributed_fetcher,
                             bool use_write_through_cache,
                             LRUCache* cache1, LRUCache* cache2)
      : TestRewriteDriverFactory(GTestTempDir(), url_fetcher,
                                 distributed_fetcher),
        cache1_(cache1),
        cache2_(cache2),
        use_write_through_cache_(use_write_through_cache) {
    InitializeDefaultOptions();
  }

  scoped_ptr<LRUCache> owned_cache1_;
  scoped_ptr<LRUCache> owned_cache2_;
  LRUCache* cache1_;
  LRUCache* cache2_;
  bool use_write_through_cache_;

  DISALLOW_COPY_AND_ASSIGN(CustomRewriteDriverFactory);
};

}  // namespace

class TwoLevelCacheTest : public RewriteContextTestBase {
 protected:
  TwoLevelCacheTest()
      : RewriteContextTestBase(
          CustomRewriteDriverFactory::MakeFactories(
              &mock_url_fetcher_,
              &test_distributed_fetcher_)) {}

  // These must be run prior to the calls to 'new CustomRewriteDriverFactory'
  // in the constructor initializer above.  Thus the calls to Initialize() in
  // the base class are too late.
  static void SetUpTestCase() {
    RewriteOptions::Initialize();
  }
  static void TearDownTestCase() {
    RewriteOptions::Terminate();
  }

  virtual void SetUp() {
    // Keeping local pointers to cache backends for convenience.  Note that both
    // factory and other_factory use the same cache backends.
    CustomRewriteDriverFactory* custom_factory =
        static_cast<CustomRewriteDriverFactory*>(factory());
    cache1_ = custom_factory->cache1();
    cache2_ = custom_factory->cache2();
    RewriteContextTestBase::SetUp();
  }

  virtual void TearDown() {
    RewriteContextTestBase::TearDown();
  }

  virtual void ClearStats() {
    RewriteContextTestBase::ClearStats();
    cache1_->ClearStats();
    cache2_->ClearStats();
  }

  void ParseWithOther(const StringPiece& id, const GoogleString& html_input) {
    GoogleString url = StrCat(kTestDomain, id, ".html");
    SetupWriter();
    other_rewrite_driver()->StartParse(url);
    other_rewrite_driver()->ParseText(doctype_string_ +
                                      AddHtmlBody(html_input));
    other_rewrite_driver()->FinishParse();
  }

  void TwoCachesInDifferentState(bool stale_ok) {
    InitTrimFilters(kOnTheFlyResource);
    InitResources();

    if (stale_ok) {
      options()->ClearSignatureForTesting();
      options()->set_metadata_cache_staleness_threshold_ms(2 * kOriginTtlMs);
      options()->ComputeSignature();
      other_options()->ClearSignatureForTesting();
      other_options()->set_metadata_cache_staleness_threshold_ms(
          2 * kOriginTtlMs);
      other_options()->ComputeSignature();
    }

    // The first rewrite was successful because we got an 'instant' url
    // fetch, not because we did any cache lookups. We'll have 2 cache
    // misses: one for the OutputPartitions, one for the fetch.  We
    // should need two items in the cache: the element and the resource
    // mapping (OutputPartitions).  The output resource should not be
    // stored.
    GoogleString input_html(CssLinkHref("a.css"));
    GoogleString output_html(CssLinkHref(
        Encode("", "tw", "0", "a.css", "css")));
    ValidateExpected("trimmable", input_html, output_html);
    EXPECT_EQ(0, cache1_->num_hits());
    EXPECT_EQ(2, cache1_->num_misses());
    EXPECT_EQ(2, cache1_->num_inserts());  // 2 because it's kOnTheFlyResource
    EXPECT_EQ(0, cache2_->num_hits());
    // Miss only for metadata and not HTTPcache.
    EXPECT_EQ(1, cache2_->num_misses());
    EXPECT_EQ(1, cache2_->num_inserts());  // Only OutputPartitions
    EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(1, logging_info()->metadata_cache_info().num_misses());
    EXPECT_EQ(0, logging_info()->metadata_cache_info().num_revalidates());
    EXPECT_EQ(0, logging_info()->metadata_cache_info().num_hits());
    ClearStats();

    // The second time we request this URL, we should find no additional
    // cache inserts or fetches.  The rewrite should complete using a
    // single cache hit for the metadata.  No cache misses will occur.
    ValidateExpected("trimmable", input_html, output_html);
    EXPECT_EQ(1, cache1_->num_hits());
    EXPECT_EQ(0, cache1_->num_misses());
    EXPECT_EQ(0, cache1_->num_inserts());
    EXPECT_EQ(0, cache2_->num_hits());
    EXPECT_EQ(0, cache2_->num_misses());
    EXPECT_EQ(0, cache2_->num_inserts());
    EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(0, logging_info()->metadata_cache_info().num_misses());
    EXPECT_EQ(0, logging_info()->metadata_cache_info().num_revalidates());
    EXPECT_EQ(1, logging_info()->metadata_cache_info().num_hits());
    ClearStats();

    AdvanceTimeMs(2 * kOriginTtlMs);
    other_factory_->AdvanceTimeMs(2 * kOriginTtlMs);
    // The third time we request this URL through the other_rewrite_driver
    // (which has cache2 as metadata cache) so that we have a fresh value in
    // cache2 which is the L2 cache for the write through cache used in
    // rewrite_driver.
    ParseWithOther("trimmable", input_html);
    EXPECT_EQ(1, cache1_->num_hits());
    EXPECT_EQ(0, cache1_->num_misses());
    EXPECT_EQ(1, cache1_->num_inserts());
    EXPECT_EQ(1, cache2_->num_hits());
    EXPECT_EQ(0, cache2_->num_misses());
    EXPECT_EQ(1, cache2_->num_inserts());
    EXPECT_EQ(1, other_factory_->counting_url_async_fetcher()->fetch_count());
    LoggingInfo* other_logging_info =
        other_rewrite_driver()->request_context()->log_record()->logging_info();
    EXPECT_EQ(0, other_logging_info->metadata_cache_info().num_misses());
    if (stale_ok) {
      // If metadata staleness threshold is set, we would get a cache hit
      // as we allow stale rewrites.
      EXPECT_EQ(
          1, other_logging_info->metadata_cache_info().num_stale_rewrites());
      EXPECT_EQ(0,
                other_logging_info->metadata_cache_info().num_revalidates());
      EXPECT_EQ(1, other_logging_info->metadata_cache_info().num_hits());
    } else {
      EXPECT_EQ(1,
                other_logging_info->metadata_cache_info().num_revalidates());
      EXPECT_EQ(0, other_logging_info->metadata_cache_info().num_hits());
    }

    ClearStats();
    // The fourth time we request this URL, we find fresh metadata in the write
    // through cache (in its L2 cache) and so there is no fetch.  The metadata
    // is also inserted into L1 cache.
    ValidateExpected("trimmable", input_html, output_html);
    // We have an expired hit for metadata in cache1, and a fresh hit for it in
    // cache2.  The fresh metadata is inserted in cache1.
    EXPECT_EQ(1, cache1_->num_hits());     // expired hit
    EXPECT_EQ(0, cache1_->num_misses());
    EXPECT_EQ(1, cache1_->num_inserts());  // re-inserts after expiration.
    EXPECT_EQ(1, cache2_->num_hits());     // fresh hit
    EXPECT_EQ(0, cache2_->num_misses());
    EXPECT_EQ(0, cache2_->num_inserts());
    EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
    EXPECT_EQ(0, logging_info()->metadata_cache_info().num_misses());
    EXPECT_EQ(0, logging_info()->metadata_cache_info().num_revalidates());
    EXPECT_EQ(1, logging_info()->metadata_cache_info().num_hits());
    ClearStats();
  }

  LRUCache* cache1_;
  LRUCache* cache2_;
};

TEST_F(TwoLevelCacheTest, BothCachesInSameState) {
  InitTrimFilters(kOnTheFlyResource);
  InitResources();

  // The first rewrite was successful because we got an 'instant' url
  // fetch, not because we did any cache lookups. We'll have 2 cache
  // misses: one for the OutputPartitions, one for the fetch.  We
  // should need two items in the cache: the element and the resource
  // mapping (OutputPartitions).  The output resource should not be
  // stored.
  GoogleString input_html(CssLinkHref("a.css"));
  GoogleString output_html(CssLinkHref(
      Encode("", "tw", "0", "a.css", "css")));
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(0, cache1_->num_hits());
  EXPECT_EQ(2, cache1_->num_misses());
  EXPECT_EQ(2, cache1_->num_inserts());  // 2 because it's kOnTheFlyResource
  EXPECT_EQ(0, cache2_->num_hits());
  EXPECT_EQ(1, cache2_->num_misses());   // Only for metadata and not HTTPcache
  EXPECT_EQ(1, cache2_->num_inserts());  // Only OutputPartitions
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, logging_info()->metadata_cache_info().num_misses());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_hits());
  ClearStats();

  // The second time we request this URL, we should find no additional
  // cache inserts or fetches.  The rewrite should complete using a
  // single cache hit for the metadata.  No cache misses will occur.
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(1, cache1_->num_hits());
  EXPECT_EQ(0, cache1_->num_misses());
  EXPECT_EQ(0, cache1_->num_inserts());
  EXPECT_EQ(0, cache2_->num_hits());
  EXPECT_EQ(0, cache2_->num_misses());
  EXPECT_EQ(0, cache2_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_misses());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_revalidates());
  EXPECT_EQ(1, logging_info()->metadata_cache_info().num_hits());
  ClearStats();

  // The third time we request this URL, we've advanced time so that the origin
  // resource TTL has expired.  The data will be re-fetched, and the Date
  // corrected.   See url_input_resource.cc, AddToCache().  The http cache will
  // miss, but we'll re-insert.  We won't need to do any more rewrites because
  // the data did not actually change.
  AdvanceTimeMs(2 * kOriginTtlMs);
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(2, cache1_->num_hits());     // 1 expired hit, 1 valid hit.
  EXPECT_EQ(0, cache1_->num_misses());
  EXPECT_EQ(2, cache1_->num_inserts());  // re-inserts after expiration.
  // The following is 1 because the ValidateCandidate check in
  // OutputCacheCallback will return false for cache1.  Without the check we
  // will simply return expired value from cache1 instead of trying cache2.
  EXPECT_EQ(1, cache2_->num_hits());     // 1 expired hit
  EXPECT_EQ(0, cache2_->num_misses());
  EXPECT_EQ(1, cache2_->num_inserts());  // re-inserts after expiration.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_misses());
  EXPECT_EQ(1, logging_info()->metadata_cache_info().num_revalidates());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_hits());
  ClearStats();

  // The fourth time we request this URL, the cache is in good shape despite
  // the expired date header from the origin.
  ValidateExpected("trimmable", input_html, output_html);
  EXPECT_EQ(1, cache1_->num_hits());     // 1 expired hit, 1 valid hit.
  EXPECT_EQ(0, cache1_->num_misses());
  EXPECT_EQ(0, cache1_->num_inserts());  // re-inserts after expiration.
  EXPECT_EQ(0, cache2_->num_hits());
  EXPECT_EQ(0, cache2_->num_misses());
  EXPECT_EQ(0, cache2_->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_misses());
  EXPECT_EQ(0, logging_info()->metadata_cache_info().num_revalidates());
  EXPECT_EQ(1, logging_info()->metadata_cache_info().num_hits());
}

TEST_F(TwoLevelCacheTest, BothCachesInDifferentState) {
  TwoCachesInDifferentState(false);
}

TEST_F(TwoLevelCacheTest, BothCachesInDifferentStaleState) {
  TwoCachesInDifferentState(true);
}

}  // namespace net_instaweb
