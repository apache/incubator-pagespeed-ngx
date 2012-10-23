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

// Author: morlovich@google.com (Maksim Orlovich)
//
// Unit tests for LoopbackRouteFetcher
//
#include "net/instaweb/apache/loopback_route_fetcher.h"

#include <cstdlib>

#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/reflecting_test_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

#include "apr_network_io.h"
#include "apr_pools.h"


namespace net_instaweb {

namespace {

class LoopbackRouteFetcherTest : public RewriteOptionsTestBase<RewriteOptions> {
 public:
  LoopbackRouteFetcherTest()
      : pool_(NULL),
        loopback_route_fetcher_(&options_, 42, &reflecting_fetcher_) {}

  static void SetUpTestCase() {
    apr_initialize();
    atexit(apr_terminate);
  }

  virtual void SetUp() {
    apr_pool_create(&pool_, NULL);
  }

  virtual void TearDown() {
    apr_pool_destroy(pool_);
  }

 protected:
  char* DumpAddr(apr_sockaddr_t* addr) {
    char* dbg = NULL;
    apr_sockaddr_ip_get(&dbg, addr);
    return dbg;  // it's in pool_
  }

  apr_pool_t* pool_;
  GoogleMessageHandler handler_;
  RewriteOptions options_;
  ReflectingTestFetcher reflecting_fetcher_;
  LoopbackRouteFetcher loopback_route_fetcher_;
};

TEST_F(LoopbackRouteFetcherTest, LoopbackRouteFetcherWorks) {
  // As we use the reflecting fetcher as the backend here, the reply
  // messages will contain the URL the fetcher got as payload.

  ExpectStringAsyncFetch dest(true);
  loopback_route_fetcher_.Fetch("http://somehost.com/url", &handler_, &dest);
  EXPECT_STREQ("http://127.0.0.1:42/url", dest.buffer());
  EXPECT_STREQ("somehost.com",
               dest.response_headers()->Lookup1("Host"));

  // Now make somehost.com known, as well as somehost.cdn.com
  options_.domain_lawyer()->AddOriginDomainMapping(
      "somehost.cdn.com", "somehost.com", &handler_);

  ExpectStringAsyncFetch dest2(true);
  loopback_route_fetcher_.Fetch("http://somehost.com/url", &handler_, &dest2);
  EXPECT_STREQ("http://somehost.com/url", dest2.buffer());

  ExpectStringAsyncFetch dest3(true);
  loopback_route_fetcher_.Fetch("http://somehost.cdn.com/url",
                                &handler_, &dest3);
  EXPECT_STREQ("http://somehost.cdn.com/url", dest3.buffer());

  // Should still be redirected if the port doesn't match.
  ExpectStringAsyncFetch dest4(true);
  loopback_route_fetcher_.Fetch("http://somehost.cdn.com:123/url",
                                &handler_, &dest4);
  EXPECT_STREQ("http://127.0.0.1:42/url", dest4.buffer());
  EXPECT_STREQ("somehost.cdn.com:123",
               dest4.response_headers()->Lookup1("Host"));
}

TEST_F(LoopbackRouteFetcherTest, CanDetectSelfSrc) {
  apr_sockaddr_t* loopback_1 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&loopback_1, "127.0.0.1", APR_INET,
                                  80, 0, pool_));

  apr_sockaddr_t* loopback_2 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&loopback_2, "127.12.34.45", APR_INET,
                                  80, 0, pool_));

  apr_sockaddr_t* loopback_3 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&loopback_3, "::1", APR_INET6,
                                  80, 0, pool_));

  apr_sockaddr_t* loopback_4 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&loopback_4, "::FFFF:127.0.0.2", APR_INET6,
                                  80, 0, pool_));

  apr_sockaddr_t* not_loopback_1 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&not_loopback_1, "128.0.0.1", APR_INET,
                                  80, 0, pool_));

  apr_sockaddr_t* not_loopback_2 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&not_loopback_2, "::1:1", APR_INET6,
                                  80, 0, pool_));

  apr_sockaddr_t* not_loopback_3 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&not_loopback_3, "::1:FFFF:127.0.0.1",
                                  APR_INET6, 80, 0, pool_));

  EXPECT_TRUE(LoopbackRouteFetcher::IsLoopbackAddr(loopback_1))
      << DumpAddr(loopback_1);
  EXPECT_TRUE(LoopbackRouteFetcher::IsLoopbackAddr(loopback_2))
      << DumpAddr(loopback_2);
  EXPECT_TRUE(LoopbackRouteFetcher::IsLoopbackAddr(loopback_3))
      << DumpAddr(loopback_3);
  EXPECT_TRUE(LoopbackRouteFetcher::IsLoopbackAddr(loopback_4))
      << DumpAddr(loopback_4);
  EXPECT_FALSE(LoopbackRouteFetcher::IsLoopbackAddr(not_loopback_1))
      << DumpAddr(not_loopback_1);
  EXPECT_FALSE(LoopbackRouteFetcher::IsLoopbackAddr(not_loopback_2))
      << DumpAddr(not_loopback_2);
  EXPECT_FALSE(LoopbackRouteFetcher::IsLoopbackAddr(not_loopback_3))
      << DumpAddr(not_loopback_3);
}

}  // namespace

}  // namespace net_instaweb
