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

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/cacheable_resource_base.h"

#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/mock_resource_callback.h"
#include "net/instaweb/rewriter/public/resource.h"  // for Resource, etc
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"  // for GoogleString
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

namespace {

const char kTestUrl[] = "http://www.example.com/";
const char kContent[] = "content!";

class TestResource : public CacheableResourceBase {
 public:
  explicit TestResource(RewriteDriver* rewrite_driver)
      : CacheableResourceBase("test", rewrite_driver, NULL) {
  }

  static void InitStats(Statistics* stats) {
    CacheableResourceBase::InitStats("test", stats);
  }

  virtual GoogleString url() const { return kTestUrl; }

  virtual bool IsValidAndCacheableImpl(const ResponseHeaders& headers) const {
    return !server_context()->http_cache()->IsAlreadyExpired(NULL, headers);
  }

  // Wipe any loaded values, but not the configuration.
  void Reset() {
    HTTPValue empty_value;
    // Don't want it to totally empty, or it won't get set.
    empty_value.Write("", server_context()->message_handler());
    Link(&empty_value, server_context()->message_handler());
    LinkFallbackValue(&empty_value);
  }
};

class MockFreshenCallback : public Resource::FreshenCallback {
 public:
  MockFreshenCallback(const ResourcePtr& resource,
                      InputInfo* input_info)
      : FreshenCallback(resource),
        input_info_(input_info),
        done_(false),
        extend_success_(false) {
  }

  virtual InputInfo* input_info() { return input_info_; }

  virtual void Done(bool lock_failure, bool extend_success) {
    done_ = true;
    extend_success_ = extend_success;
  }

  bool done() const { return done_; }
  bool extend_success() const { return extend_success_; }

 private:
  InputInfo* input_info_;
  bool done_;
  bool extend_success_;
};

}  // namespace

class CacheableResourceBaseTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    TestResource::InitStats(server_context()->statistics());

    resource_.reset(new TestResource(rewrite_driver()));
  }

  void CheckStats(TestResource* resource,
                  int expect_hits,
                  int expect_recent_fetch_failures,
                  int expect_recent_uncacheables_treated_as_miss,
                  int expect_recent_uncacheables_treated_as_failure,
                  int expect_misses) {
    EXPECT_EQ(expect_hits,
              resource->hits_->Get());
    EXPECT_EQ(expect_recent_fetch_failures,
              resource->recent_fetch_failures_->Get());
    EXPECT_EQ(expect_recent_uncacheables_treated_as_miss,
              resource->recent_uncacheables_treated_as_miss_->Get());
    EXPECT_EQ(expect_recent_uncacheables_treated_as_failure,
              resource->recent_uncacheables_treated_as_failure_->Get());
    EXPECT_EQ(expect_misses,
              resource->misses_->Get());
  }

  RefCountedPtr<TestResource> resource_;
};

TEST_F(CacheableResourceBaseTest, BasicCached) {
  SetResponseWithDefaultHeaders(kTestUrl, kContentTypeText,
                                kContent, 1000);

  MockResourceCallback callback(ResourcePtr(resource_.get()),
                                server_context()->thread_system());
  resource_->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_TRUE(callback.success());
  EXPECT_EQ(kContent, resource_->contents());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  CheckStats(resource_.get(), 0, 0, 0, 0, 1);

  // 2nd read should be cached.
  resource_->Reset();
  MockResourceCallback callback2(ResourcePtr(resource_.get()),
                                 server_context()->thread_system());
  resource_->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback2);
  EXPECT_TRUE(callback2.done());
  EXPECT_TRUE(callback2.success());
  EXPECT_EQ(kContent, resource_->contents());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  CheckStats(resource_.get(), 1, 0, 0, 0, 1);

  // Make sure freshening happens. The rest resource is set to 1000 sec ttl,
  // so forward time 900 seconds ahead.
  AdvanceTimeMs(900 * Timer::kSecondMs);
  resource_->Reset();
  MockResourceCallback callback3(ResourcePtr(resource_.get()),
                                 server_context()->thread_system());
  resource_->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback3);
  EXPECT_TRUE(callback3.done());
  EXPECT_TRUE(callback3.success());
  EXPECT_EQ(kContent, resource_->contents());
  // Freshening resulted in an extra fetch
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  CheckStats(resource_.get(), 2, 0, 0, 0, 1);
}

TEST_F(CacheableResourceBaseTest, Private) {
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypeText, &response_headers);
  response_headers.Add(HttpAttributes::kCacheControl, "private");
  SetFetchResponse(kTestUrl, response_headers, kContent);

  MockResourceCallback callback(ResourcePtr(resource_.get()),
                                server_context()->thread_system());

  resource_->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_FALSE(callback.success());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  CheckStats(resource_.get(), 0, 0, 0, 0, 1);

  // The non-cacheability should be cached.
  resource_->Reset();
  MockResourceCallback callback2(ResourcePtr(resource_.get()),
                                 server_context()->thread_system());
  resource_->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback2);
  EXPECT_TRUE(callback2.done());
  EXPECT_FALSE(callback2.success());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  CheckStats(resource_.get(), 0, 0, 0, 1, 1);
}

TEST_F(CacheableResourceBaseTest, PrivateForFetch) {
  // This test private + kLoadEvenIfNotCacheable.
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypeText, &response_headers);
  response_headers.Add(HttpAttributes::kCacheControl, "private");
  SetFetchResponse(kTestUrl, response_headers, kContent);

  MockResourceCallback callback(ResourcePtr(resource_.get()),
                                server_context()->thread_system());

  resource_->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_TRUE(callback.success());
  EXPECT_EQ(kContent, resource_->contents());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  CheckStats(resource_.get(), 0, 0, 0, 0, 1);

  // Since it's non-cacheable, but we have kLoadEvenIfNotCacheable
  // set, we should re-fetch it.
  resource_->Reset();
  MockResourceCallback callback2(ResourcePtr(resource_.get()),
                                 server_context()->thread_system());
  resource_->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback2);
  EXPECT_TRUE(callback2.done());
  EXPECT_TRUE(callback2.success());
  EXPECT_EQ(kContent, resource_->contents());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  CheckStats(resource_.get(), 0, 0, 1, 0, 1);
}

TEST_F(CacheableResourceBaseTest, FetchFailure) {
  SetFetchFailOnUnexpected(false);
  MockResourceCallback callback(ResourcePtr(resource_.get()),
                                server_context()->thread_system());

  resource_->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_FALSE(callback.success());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  CheckStats(resource_.get(), 0, 0, 0, 0, 1);

  // Failure should get cached, and we should take advantage of it.
  resource_->Reset();
  MockResourceCallback callback2(ResourcePtr(resource_.get()),
                                 server_context()->thread_system());
  resource_->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback2);
  EXPECT_TRUE(callback2.done());
  EXPECT_FALSE(callback2.success());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  CheckStats(resource_.get(), 0, 1, 0, 0, 1);

  // Now advance time, should force a refetch.
  int64 remember_sec =
      server_context()->http_cache()->remember_fetch_failed_ttl_seconds();
  AdvanceTimeMs(2 * remember_sec * Timer::kSecondMs);
  resource_->Reset();
  MockResourceCallback callback3(ResourcePtr(resource_.get()),
                                 server_context()->thread_system());
  resource_->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback3);
  EXPECT_TRUE(callback3.done());
  EXPECT_FALSE(callback3.success());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  CheckStats(resource_.get(), 0, 1, 0, 0, 2);
}

TEST_F(CacheableResourceBaseTest, FreshenInfo) {
  SetResponseWithDefaultHeaders(kTestUrl, kContentTypeText,
                                kContent, 1000);

  MockResourceCallback callback(ResourcePtr(resource_.get()),
                                server_context()->thread_system());
  resource_->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_TRUE(callback.done());
  EXPECT_TRUE(callback.success());

  InputInfo input_info;
  resource_->FillInPartitionInputInfo(Resource::kIncludeInputHash,
                                      &input_info);
  InputInfo input_info2 = input_info;

  // Move time ahead so freshening actually does something.
  AdvanceTimeMs(900 * Timer::kSecondMs);

  MockFreshenCallback freshen_cb(ResourcePtr(resource_.get()), &input_info);
  resource_->Freshen(&freshen_cb, message_handler());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());

  EXPECT_TRUE(freshen_cb.done());
  EXPECT_TRUE(freshen_cb.extend_success());

  // Expiration time must have moved ahead, too.
  EXPECT_EQ(1000 * Timer::kSecondMs + timer()->NowMs(),
            input_info.expiration_time_ms());

  // The above freshened from fetches, now we should be able to do it
  // from cache as well.
  MockFreshenCallback freshen_cb2(ResourcePtr(resource_.get()), &input_info2);
  resource_->Freshen(&freshen_cb2, message_handler());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
  EXPECT_TRUE(freshen_cb.done());
  EXPECT_TRUE(freshen_cb.extend_success());
  EXPECT_EQ(1000 * Timer::kSecondMs + timer()->NowMs(),
            input_info2.expiration_time_ms());
}

}  // namespace net_instaweb
