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

// Author: hujie@google.com (Jie Hu)

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/rewriter/public/mock_resource_callback.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource.h"  // for Resource, etc
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"  // for GoogleString
#include "pagespeed/kernel/base/string_util.h"  // for StrCat, etc
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"


namespace net_instaweb {

class UrlInputResourceTest : public RewriteTestBase {
 protected:
  void CheckResourceFetchHasReferer(const GoogleString& url,
                                    const GoogleString& base_url,
                                    bool is_background_fetch,
                                    const GoogleString& expected_referer) {
    PrepareResourceFetch(url);
    SetBaseUrlForFetch(base_url);
    ResourcePtr resource(
        new UrlInputResource(rewrite_driver(), &kContentTypeJpeg, url));
    RequestContextPtr request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    resource->set_is_background_fetch(is_background_fetch);
    MockResourceCallback cb(resource, factory()->thread_system());
    resource->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                        request_context, &cb);
    cb.Wait();
    ASSERT_TRUE(cb.done());
    ASSERT_TRUE(cb.success());
    EXPECT_STREQ(expected_referer, mock_url_fetcher()->last_referer());
  }

  void PrepareResourceFetch(const GoogleString& resource_url) {
    ResponseHeaders response_headers;
    DefaultResponseHeaders(kContentTypeJpeg, 100, &response_headers);
    SetFetchResponse(AbsolutifyUrl(resource_url), response_headers, "payload");
  }
};


// Test of referer (BackgroundFetch): When the resource fetching request header
// misses referer, we set the referer for it. Base url and resource url are
// same.
TEST_F(UrlInputResourceTest, TestBackgroundFetchRefererSameDomain) {
  GoogleString url = StrCat(kTestDomain, "1.jpg");
  CheckResourceFetchHasReferer(url, kTestDomain, true, kTestDomain);
}

// Test of referer (BackgroundFetch): When the resource fetching request header
// misses referer, we set the referer for it. Base url and resource url are
// different.
TEST_F(UrlInputResourceTest, TestBackgroundFetchRefererDifferentDomain) {
  GoogleString url = "http://other.com/1.jpg";
  CheckResourceFetchHasReferer(url, kTestDomain, true, kTestDomain);
}

// Test of referer (NonBackgroundFetch): When the resource fetching request
// header misses referer, we check if there is any referer from the original
// request header. If that referer is empty, No referer would be set for this
// fetching request.
TEST_F(UrlInputResourceTest, TestNonBackgroundFetchWithRefererMissing) {
  GoogleString url = "http://other.com/1.jpg";
  RequestHeaders headers;
  rewrite_driver()->SetRequestHeaders(headers);
  CheckResourceFetchHasReferer(url, kTestDomain, false, "");
}

// Test of referer (NonBackgroundFetch): When the resource fetching request
// header misses referer, we set the referer for it from the original request
// header.
TEST_F(UrlInputResourceTest, TestNonBackgroundFetchWithReferer) {
  GoogleString url = "http://other.com/1.jpg";
  RequestHeaders headers;
  headers.Add(HttpAttributes::kReferer, kTestDomain);
  rewrite_driver()->SetRequestHeaders(headers);
  CheckResourceFetchHasReferer(url, kTestDomain, false, kTestDomain);
}

}  // namespace net_instaweb
