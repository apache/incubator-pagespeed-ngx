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

// Author: gagansingh@google.com (Gagan Singh)

#include "net/instaweb/automatic/public/proxy_fetch.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class ProxyFetchTest : public ResourceManagerTestBase {
 protected:
  virtual void SetUp() {
    ResourceManagerTestBase::SetUp();

    factory_.reset(new ProxyFetchFactory(resource_manager()));
    async_fetch_.reset(new StringAsyncFetch());

    req_headers_.reset(new RequestHeaders());
    req_headers_->Add(HttpAttributes::kXForwardedFor, "60.70.80.90");
    async_fetch_->set_request_headers(req_headers_.get());

    resp_headers_.reset(new ResponseHeaders());
    resp_headers_->SetStatusAndReason(HttpStatus::kOK);
    async_fetch_->set_response_headers(resp_headers_.get());

    RewriteOptions* options = resource_manager()->NewOptions();
    proxy_fetch_ = new ProxyFetch(
        "http://www.example.com/blah", false, async_fetch_.get(),
        options, resource_manager(),
        resource_manager()->timer(), factory_.get());
  }

  virtual void TearDown() {
    proxy_fetch_->Done(true);
    EXPECT_EQ(0, resource_manager()->num_active_rewrite_drivers());
    ResourceManagerTestBase::TearDown();
  }

  RewriteDriver* driver(ProxyFetch* proxy_fetch) {
    return proxy_fetch->driver_;
  }

  scoped_ptr<ProxyFetchFactory> factory_;
  scoped_ptr<RequestHeaders> req_headers_;
  scoped_ptr<ResponseHeaders> resp_headers_;
  scoped_ptr<AsyncFetch> async_fetch_;
  ProxyFetch* proxy_fetch_;  // gets deleted by ProxyFetch::Finish()
};

// TODO(gagansingh): Move request_start_time_ms_ test here for consistency.
TEST_F(ProxyFetchTest, XForwardedFor) {
  EXPECT_EQ("60.70.80.90", driver(proxy_fetch_)->user_ip());
}

}  // namespace net_instaweb
