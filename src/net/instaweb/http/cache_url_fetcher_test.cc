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

// Unit-test the caching fetcher, using a mock fetcher, and an async
// wrapper around that.

#include "net/instaweb/http/public/cache_url_fetcher.h"

#include "net/instaweb/http/cache_fetcher_test.h"
#include "net/instaweb/http/public/fetcher_test.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class CacheUrlFetcherTest : public CacheFetcherTest {};

TEST_F(CacheUrlFetcherTest, TestCachableWithSyncFetcher) {
  CacheUrlFetcher cache_fetcher(&http_cache_, &mock_fetcher_);
  EXPECT_EQ(1, CountFetchesSync(kGoodUrl, &cache_fetcher, true, true));
  EXPECT_EQ(0, CountFetchesSync(kGoodUrl, &cache_fetcher, true, true));
}

TEST_F(CacheUrlFetcherTest, TestNonCachableWithSyncFetcher) {
  // When a CacheUrlFetcher is implemented using a sync fetcher,
  // then non-cacheable URLs will result in sync-fetch successes
  // that are not cached.
  // so we will not do the fetch the second time.
  CacheUrlFetcher cache_fetcher(&http_cache_, &mock_fetcher_);
  EXPECT_EQ(1, CountFetchesSync(kNotCachedUrl, &cache_fetcher, true, true));
  EXPECT_EQ(1, CountFetchesSync(kNotCachedUrl, &cache_fetcher, true, true));
}

TEST_F(CacheUrlFetcherTest, TestCacheWithSyncFetcherFail) {
  CacheUrlFetcher cache_fetcher(&http_cache_, &mock_fetcher_);
  EXPECT_EQ(1, CountFetchesSync(kBadUrl, &cache_fetcher, false, true));
  // For now, we don't cache failure, so we expect a new fetch
  // on each request
  EXPECT_EQ(1, CountFetchesSync(kBadUrl, &cache_fetcher, false, true));
}

TEST_F(CacheUrlFetcherTest, TestCachableWithASyncFetcher) {
  CacheUrlFetcher cache_fetcher(&http_cache_, &mock_async_fetcher_);

  // The first fetch from the cache fetcher with the async interface
  // will not succeed (arg==false), and it will queue up an async fetch
  EXPECT_EQ(1, CountFetchesSync(kGoodUrl, &cache_fetcher, false, false));

  // The second one will behave exactly the same, because we have
  // not let the async callback run yet.
  EXPECT_EQ(1, CountFetchesSync(kGoodUrl, &cache_fetcher, false, false));

  // Now we call the callbacks, and so subsequent requests to the
  // the cache will succeed (arg==true) and will yield 0 new fetch
  // requests.
  mock_async_fetcher_.CallCallbacks();
  EXPECT_EQ(0, CountFetchesSync(kGoodUrl, &cache_fetcher, true, false));
}

TEST_F(CacheUrlFetcherTest, TestNonCachableWithASyncFetcher) {
  CacheUrlFetcher cache_fetcher(&http_cache_, &mock_async_fetcher_);

  // The first fetch from the cache fetcher with the async interface
  // will not succeed (arg==false), and it will queue up an async fetch
  EXPECT_EQ(1, CountFetchesSync(kNotCachedUrl, &cache_fetcher, false, false));
  EXPECT_EQ(1, CountFetchesSync(kNotCachedUrl, &cache_fetcher, false, false));
  mock_async_fetcher_.CallCallbacks();

  // When a CacheUrlFetcher is implemented using an async fetcher,
  // then non-cacheable URLs will result in sync-fetch failures,
  // but we will remember the failure so we don't have to do the
  // second fetch.
  EXPECT_EQ(0, CountFetchesSync(kNotCachedUrl, &cache_fetcher, false, false));
}

TEST_F(CacheUrlFetcherTest, TestCacheWithASyncFetcherFail) {
  CacheUrlFetcher cache_fetcher(&http_cache_, &mock_async_fetcher_);

  EXPECT_EQ(1, CountFetchesSync(kBadUrl, &cache_fetcher, false, false));
  EXPECT_EQ(1, CountFetchesSync(kBadUrl, &cache_fetcher, false, false));
  mock_async_fetcher_.CallCallbacks();
  EXPECT_EQ(1, CountFetchesSync(kBadUrl, &cache_fetcher, false, false));
  mock_async_fetcher_.CallCallbacks();  // Otherwise memory will be leaked
}

}  // namespace net_instaweb
