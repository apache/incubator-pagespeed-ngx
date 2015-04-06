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
// Author: gee@google.com (Adam Gee)

#include "net/instaweb/config/rewrite_options_manager.h"

#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

template<class T>
void Copy(T* to, T from) {
  *to = from;
}

class RewriteOptionsManagerTest :
      public RewriteOptionsTestBase<RewriteOptions> {
 protected:
  RewriteOptionsManagerTest()
      : timer_(Platform::CreateTimer()) {
    request_context_.reset(
        new RequestContext(thread_system()->NewMutex(),
                           timer_.get()));
  }

  void RunGetRewriteOptionsTest() {
    // Initialize the pointer to some garbage;
    scoped_ptr<RewriteOptions> non_null(NewOptions());
    ASSERT_TRUE(non_null.get());
    GoogleUrl url("http://www.foo.com");
    RequestHeaders request_headers;
    options_manager_.GetRewriteOptions(
        url, request_headers,
        NewCallback(this, &RewriteOptionsManagerTest::CheckNull));
  }

  void CheckNull(RewriteOptions* rewrite_options) {
    EXPECT_TRUE(NULL == rewrite_options);
  }

  bool PrepareRequest(const RewriteOptions* rewrite_options,
                      const RequestContextPtr& request_context,
                      GoogleString* url,
                      RequestHeaders* request_headers) {
    prepare_done_ = false;

    options_manager_.PrepareRequest(
        rewrite_options, request_context_, url, request_headers,
        NewCallback(this, &RewriteOptionsManagerTest::PrepareDone));
    EXPECT_TRUE(prepare_done_);
    return prepare_success_;
  }

  RewriteOptionsManager options_manager_;
  RequestContextPtr request_context_;

 private:
  void PrepareDone(bool success) {
    prepare_success_ = success;
    prepare_done_ = true;
  }

  scoped_ptr<Timer> timer_;
  bool prepare_done_;
  bool prepare_success_;
};

TEST_F(RewriteOptionsManagerTest, GetRewriteOptions) {
  RunGetRewriteOptionsTest();
}

TEST_F(RewriteOptionsManagerTest, Prepare_NULLOptions) {
  GoogleString url;
  RequestHeaders request_headers;
  ASSERT_TRUE(PrepareRequest(NULL, request_context_, &url, &request_headers));
}

TEST_F(RewriteOptionsManagerTest, Prepare_InvalidUrl) {
  scoped_ptr<RewriteOptions> default_options(NewOptions());
  GoogleString url("invalid_url");
  RequestHeaders request_headers;
  ASSERT_FALSE(PrepareRequest(
      default_options.get(), request_context_, &url, &request_headers));
}

TEST_F(RewriteOptionsManagerTest, Prepare_NotProxy) {
  scoped_ptr<RewriteOptions> default_options(NewOptions());
  GoogleString url("http://www.foo.com");
  RequestHeaders request_headers;
  ASSERT_TRUE(PrepareRequest(
      default_options.get(), request_context_, &url, &request_headers));

  // Domains that aren't proxied should have the host header.
  const GoogleString host_header =
      request_headers.Lookup1(HttpAttributes::kHost);
  EXPECT_STREQ("www.foo.com", host_header);
}

TEST_F(RewriteOptionsManagerTest, Prepare_Proxy) {
  scoped_ptr<RewriteOptions> default_options(NewOptions());
  ASSERT_TRUE(default_options->WriteableDomainLawyer()->AddProxyDomainMapping(
      "www.foo.com",
      "www.origin.com",
      "",
      NULL));

  GoogleString url("http://www.foo.com");
  RequestHeaders request_headers;
  ASSERT_TRUE(PrepareRequest(
      default_options.get(), request_context_, &url, &request_headers));
  ASSERT_TRUE(NULL == request_headers.Lookup1(HttpAttributes::kHost));
}

TEST_F(RewriteOptionsManagerTest, Prepare_ProxySuffix) {
  scoped_ptr<RewriteOptions> default_options(NewOptions());
  DomainLawyer* lawyer = default_options->WriteableDomainLawyer();
  lawyer->set_proxy_suffix(".suffix");

  GoogleString url("http://www.foo.com");
  RequestHeaders request_headers;
  ASSERT_TRUE(PrepareRequest(
      default_options.get(), request_context_, &url, &request_headers));
  EXPECT_FALSE(request_context_->IsSessionAuthorizedFetchOrigin(
      "http://www.foo.com"));
  EXPECT_STREQ("www.foo.com", request_headers.Lookup1(HttpAttributes::kHost));

  request_headers.Clear();
  url = "http://www.foo.com.suffix";
  ASSERT_TRUE(PrepareRequest(
      default_options.get(), request_context_, &url, &request_headers));
  EXPECT_TRUE(request_context_->IsSessionAuthorizedFetchOrigin(
      "http://www.foo.com"));
  EXPECT_STREQ("www.foo.com", request_headers.Lookup1(HttpAttributes::kHost));
}

}  // namespace

}  // namespace net_instaweb
