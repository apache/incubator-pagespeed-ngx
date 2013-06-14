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

#include "base/logging.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/mock_resource_callback.h"
#include "net/instaweb/rewriter/public/resource.h"  // for Resource, etc
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"  // for GoogleString
#include "pagespeed/kernel/base/string_util.h"  // for StrCat, etc
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"


namespace net_instaweb {

class MessageHandler;
class RewriteOptions;

namespace {

const char kTestUrl[] = "http://www.example.com";
const char kContent[] = "content!";

class TestResource : public CacheableResourceBase {
 public:
  explicit TestResource(ServerContext* context)
      : CacheableResourceBase(context, NULL),
        cache_control_("public,max-age=1000"),
        load_calls_(0),
        freshen_calls_(0),
        fail_fetch_(false) {}

  virtual GoogleString url() const { return kTestUrl; }
  virtual const RewriteOptions* rewrite_options() const {
    return server_context()->global_options();
  }

  virtual void LoadAndSaveToCache(NotCacheablePolicy not_cacheable_policy,
                                  AsyncCallback* callback,
                                  MessageHandler* message_handler) {
    ++load_calls_;
    if (fail_fetch_) {
      server_context()->http_cache()->RememberFetchFailed(
          kTestUrl, message_handler);
      callback->Done(false /* no lock trouble*/, false);
      return;
    }

    ResponseHeaders response_headers;
    server_context()->SetDefaultLongCacheHeaders(
        &kContentTypeText, &response_headers);
    response_headers.RemoveAll(HttpAttributes::kCacheControl);
    response_headers.Add(HttpAttributes::kCacheControl, cache_control_);
    response_headers.ComputeCaching();

    bool cacheable = !server_context()->http_cache()->IsAlreadyExpired(
                         NULL, response_headers);

    HTTPValue result;
    result.Write(kContent, message_handler);
    result.SetHeaders(&response_headers);
    EXPECT_TRUE(Link(&result, message_handler));

    if (cacheable) {
      server_context()->http_cache()->Put(kTestUrl, &result, message_handler);
    } else {
      server_context()->http_cache()->RememberNotCacheable(
          kTestUrl, true, message_handler);
    }

    bool ok = cacheable || (not_cacheable_policy == kLoadEvenIfNotCacheable);

    callback->Done(false /* no lock trouble*/, ok);
  }

  virtual void Freshen(FreshenCallback* callback, MessageHandler* handler) {
    CHECK(callback == NULL) << "Non-null Callback Freshen unimplemented";
    ++freshen_calls_;
  }

  // Wipe any loaded values, but not the configuration.
  void Reset() {
    HTTPValue empty_value;
    // Don't want it to totally empty, or it won't get set.
    empty_value.Write("", server_context()->message_handler());
    Link(&empty_value, server_context()->message_handler());
    LinkFallbackValue(&empty_value);
  }

  int load_calls() const { return load_calls_; }
  int freshen_calls() const { return freshen_calls_; }

  void set_cache_control(StringPiece c) { c.CopyToString(&cache_control_); }
  void set_fail_fetch(bool f) { fail_fetch_ = f; }

 private:
  GoogleString cache_control_;
  int load_calls_;
  int freshen_calls_;
  bool fail_fetch_;
};

class CacheableResourceBaseTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();

    resource_.reset(new TestResource(server_context()));
  }

  RefCountedPtr<TestResource> resource_;
};


TEST_F(CacheableResourceBaseTest, BasicCached) {
  MockResourceCallback callback(ResourcePtr(resource_.get()),
                                server_context()->thread_system());
  resource_->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_TRUE(callback.success());
  EXPECT_EQ(kContent, resource_->contents());
  EXPECT_EQ(1, resource_->load_calls());
  EXPECT_EQ(0, resource_->freshen_calls());

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
  EXPECT_EQ(1, resource_->load_calls());
  EXPECT_EQ(0, resource_->freshen_calls());

  // Make sure freshening happens. The rest resource defaults to 1000 sec ttl,
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
  EXPECT_EQ(1, resource_->load_calls());
  EXPECT_EQ(1, resource_->freshen_calls());
}

TEST_F(CacheableResourceBaseTest, Private) {
  resource_->set_cache_control("private");
  MockResourceCallback callback(ResourcePtr(resource_.get()),
                                server_context()->thread_system());

  resource_->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_FALSE(callback.success());
  EXPECT_EQ(1, resource_->load_calls());
  EXPECT_EQ(0, resource_->freshen_calls());

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
  EXPECT_EQ(1, resource_->load_calls());
  EXPECT_EQ(0, resource_->freshen_calls());
}

TEST_F(CacheableResourceBaseTest, PrivateForFetch) {
  // This test private + kLoadEvenIfNotCacheable.
  resource_->set_cache_control("private");
  MockResourceCallback callback(ResourcePtr(resource_.get()),
                                server_context()->thread_system());

  resource_->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_TRUE(callback.success());
  EXPECT_EQ(kContent, resource_->contents());
  EXPECT_EQ(1, resource_->load_calls());
  EXPECT_EQ(0, resource_->freshen_calls());

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
  EXPECT_EQ(2, resource_->load_calls());
  EXPECT_EQ(0, resource_->freshen_calls());
}

TEST_F(CacheableResourceBaseTest, FetchFailure) {
  resource_->set_fail_fetch(true);
  MockResourceCallback callback(ResourcePtr(resource_.get()),
                                server_context()->thread_system());

  resource_->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                       RequestContext::NewTestRequestContext(
                           server_context()->thread_system()),
                       &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_FALSE(callback.success());
  EXPECT_EQ(1, resource_->load_calls());
  EXPECT_EQ(0, resource_->freshen_calls());

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
  EXPECT_EQ(1, resource_->load_calls());
  EXPECT_EQ(0, resource_->freshen_calls());

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
  EXPECT_EQ(2, resource_->load_calls());
  EXPECT_EQ(0, resource_->freshen_calls());
}

}  // namespace

}  // namespace net_instaweb
