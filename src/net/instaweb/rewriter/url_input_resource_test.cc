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
#include "pagespeed/kernel/base/basictypes.h"  // for arraysize
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"  // for GoogleString
#include "pagespeed/kernel/base/string_util.h"  // for StrCat, etc
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

class UrlInputResourceTest : public RewriteTestBase {
 protected:
  // A const used for all tests that don't care about is_auth_domain_expected
  // in the CheckResourceFetchHasReferer calls.
  static const bool kUnusedBoolArg = true;

  // Helper method that does all the verification wrt creation of an input
  // resource with the given "url" argument. fetch_url indicates the URL to
  // use for setting up fetch responses. Note that fetch_url is almost always
  // the same as url, except when standard port numbers  (80 for http and 443
  // for https) are used, in which case the fetch_url drops these port numbers.
  // base_url is the URL of the page wrt which this resource URL is being
  // created.
  // is_authorized_domain indicates whether the URL corresponds to an explicitly
  // authorized domain or not. is_auth_domain_expected is only used when
  // is_authorized_domain is false, and it indicates whether the fetched URL's
  // domain is to be considered authorized after input resource creation or not.
  void CheckResourceFetchHasReferer(const GoogleString& url,
                                    const GoogleString& fetch_url,
                                    const GoogleString& base_url,
                                    bool is_background_fetch,
                                    bool is_authorized_domain,
                                    bool is_auth_domain_expected,
                                    const GoogleString& expected_cache_key,
                                    const GoogleString& expected_referer) {
    PrepareResourceFetch(fetch_url);
    SetBaseUrlForFetch(base_url);
    ResourcePtr resource(
        new UrlInputResource(rewrite_driver(), &kContentTypeJpeg, url,
                             is_authorized_domain));
    EXPECT_STREQ(url, resource->url());
    EXPECT_STREQ(expected_cache_key, resource->cache_key());
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
    if (!is_authorized_domain) {
      GoogleUrl tmp_url(fetch_url);
      EXPECT_EQ(is_auth_domain_expected,
                request_context->IsSessionAuthorizedFetchOrigin(
                    tmp_url.Origin().as_string()));
    }
  }

  UrlInputResource* MakeUrlInputResource(const GoogleString& url,
                                         bool is_authorized_domain) {
    return new UrlInputResource(rewrite_driver(), &kContentTypeJpeg, url,
                                is_authorized_domain);
  }

  void PrepareResourceFetch(const GoogleString& resource_url) {
    mock_url_fetcher()->set_verify_pagespeed_header_off(true);
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
  CheckResourceFetchHasReferer(url, url, kTestDomain,
                               true, true, kUnusedBoolArg,
                               url, kTestDomain);
}

// Test of referer (BackgroundFetch): When the resource fetching request header
// misses referer, we set the referer for it. Base url and resource url are
// different.
TEST_F(UrlInputResourceTest, TestBackgroundFetchRefererDomain) {
  GoogleString url = "http://other.com/1.jpg";
  CheckResourceFetchHasReferer(url, url, kTestDomain,
                               true, true, kUnusedBoolArg,
                               url, kTestDomain);
}

// Test of referer (NonBackgroundFetch): When the resource fetching request
// header misses referer, we check if there is any referer from the original
// request header. If that referer is empty, No referer would be set for this
// fetching request.
TEST_F(UrlInputResourceTest, TestNonBackgroundFetchWithRefererMissing) {
  GoogleString url = "http://other.com/1.jpg";
  RequestHeaders headers;
  rewrite_driver()->SetRequestHeaders(headers);
  CheckResourceFetchHasReferer(url, url, kTestDomain,
                               false, true, kUnusedBoolArg,
                               url, "");
}

// Test of referer (NonBackgroundFetch): When the resource fetching request
// header misses referer, we set the referer for it from the original request
// header.
TEST_F(UrlInputResourceTest, TestNonBackgroundFetchWithReferer) {
  GoogleString url = "http://other.com/1.jpg";
  RequestHeaders headers;
  headers.Add(HttpAttributes::kReferer, kTestDomain);
  rewrite_driver()->SetRequestHeaders(headers);
  CheckResourceFetchHasReferer(url, url, kTestDomain,
                               false, true, kUnusedBoolArg,
                               url, kTestDomain);
}

/* Tests related to unauthorized http domains. */

// Test that unauthorized resources are created correctly with http protocol.
TEST_F(UrlInputResourceTest, TestUnauthorizedDomainHttp) {
  GoogleString url = "http://other.com/1.jpg";
  CheckResourceFetchHasReferer(url, url, kTestDomain,
                               true, false, true,
                               "unauth://other.com/1.jpg", kTestDomain);
}

// Test that unauthorized resources are not created with wrong protocol.
TEST_F(UrlInputResourceTest, TestUnauthorizedDomainWrongProtocol) {
  GoogleString url = "ftp://other.com/1.jpg";
  PrepareResourceFetch(url);;
  SetBaseUrlForFetch(kTestDomain);
  ASSERT_DEATH(MakeUrlInputResource(url, false), "");
}

// Test that unauthorized resources are not created with a relative URL.
TEST_F(UrlInputResourceTest, TestUnauthorizedDomainRelativeURL) {
  GoogleString url = "/1.jpg";
  PrepareResourceFetch(url);;
  SetBaseUrlForFetch(kTestDomain);
  ASSERT_DEATH(MakeUrlInputResource(url, false), "");
}

// Test that unauthorized resources are created when a standard (80) port is
// specified for http.
TEST_F(UrlInputResourceTest, TestUnauthorizedDomainHttpWithCorrectPort) {
  GoogleString url = "http://other.com:80/1.jpg";
  CheckResourceFetchHasReferer(url, "http://other.com/1.jpg", kTestDomain,
                               true, false, true,
                               "unauth://other.com/1.jpg", kTestDomain);
}

// Test that unauthorized resources are not created when a non-standard port
// is specified for http.
TEST_F(UrlInputResourceTest, TestNoUnauthorizedDomainHttpWithWrongPort) {
  GoogleString url = "http://other.com:1234/1.jpg";
  CheckResourceFetchHasReferer(url, url, kTestDomain,
                               true, false, false,
                               "unauth://other.com:1234/1.jpg", kTestDomain);
}

// Test that unauthorized resources are not created when a https (443) port is
// specified for http.
TEST_F(UrlInputResourceTest, TestNoUnauthorizedDomainHttpWithHttpsPort) {
  GoogleString url = "http://other.com:443/1.jpg";
  CheckResourceFetchHasReferer(url, url, kTestDomain,
                               true, false, false,
                               "unauth://other.com:443/1.jpg", kTestDomain);
}

/* Tests related to unauthorized https domains. */

// Test that unauthorized resources are created correctly with https protocol.
TEST_F(UrlInputResourceTest, TestUnauthorizedDomainHttps) {
  GoogleString url = "https://other.com/1.jpg";
  CheckResourceFetchHasReferer(url, url, kTestDomain,
                               true, false, true,
                               "unauths://other.com/1.jpg", kTestDomain);
}

// Test that unauthorized resources are created when a standard (443) port is
// specified for https.
TEST_F(UrlInputResourceTest, TestUnauthorizedDomainHttpsWithCorrectPort) {
  GoogleString url = "https://other.com:443/1.jpg";
  CheckResourceFetchHasReferer(url, "https://other.com/1.jpg", kTestDomain,
                               true, false, true,
                               "unauths://other.com/1.jpg", kTestDomain);
}

// Test that unauthorized resources are not created when a non-standard port
// is specified for https.
TEST_F(UrlInputResourceTest, TestNoUnauthorizedDomainHttpsWithWrongPort) {
  GoogleString url = "https://other.com:1234/1.jpg";
  CheckResourceFetchHasReferer(url, url, kTestDomain,
                               true, false, false,
                               "unauths://other.com:1234/1.jpg", kTestDomain);
}

// Test that unauthorized resources are not created when a http (80) port is
// specified for https.
TEST_F(UrlInputResourceTest, TestNoUnauthorizedDomainHttpsWithHttpPort) {
  GoogleString url = "https://other.com:80/1.jpg";
  CheckResourceFetchHasReferer(url, url, kTestDomain,
                               true, false, false,
                               "unauths://other.com:80/1.jpg", kTestDomain);
}

struct PortTest {
  const char* spec;
  int expected_int_port;
  int int_port;
};

// Test that verifies that standard port numbers are treated as PORT_UNSPECIFIED
// by GoogleUrl.
TEST_F(UrlInputResourceTest, IntPort) {
  const PortTest port_tests[] = {
    // http
    {"http://www.google.com/", 80, url_parse::PORT_UNSPECIFIED},
    {"http://www.google.com:80/", 80, url_parse::PORT_UNSPECIFIED},
    {"http://www.google.com:443/", 443, 443},
    {"http://www.google.com:1234/", 1234, 1234},

    // https
    {"https://www.google.com/", 443, url_parse::PORT_UNSPECIFIED},
    {"https://www.google.com:443/", 443, url_parse::PORT_UNSPECIFIED},
    {"https://www.google.com:80/", 80, 80},
    {"https://www.google.com:1234/", 1234, 1234},
  };

  for (int i = 0; i < arraysize(port_tests); ++i) {
    GoogleUrl url(port_tests[i].spec);
    EXPECT_TRUE(url.IsWebValid());
    EXPECT_EQ(port_tests[i].expected_int_port, url.EffectiveIntPort());
    EXPECT_EQ(port_tests[i].int_port, url.IntPort());
  }
}

}  // namespace net_instaweb
