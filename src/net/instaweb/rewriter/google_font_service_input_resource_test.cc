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

#include "net/instaweb/rewriter/public/google_font_service_input_resource.h"

#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/ua_sensitive_test_fetcher.h"
#include "net/instaweb/rewriter/public/mock_resource_callback.h"
#include "net/instaweb/rewriter/public/resource.h"  // for Resource, etc
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

namespace {

const char kRoboto[] = "http://fonts.googleapis.com/css?family=Roboto";
const char kRobotoSsl[] = "https://fonts.googleapis.com/css?family=Roboto";
const char kNonCss[] = "http://fonts.googleapis.com/some.txt";

class GoogleFontServiceInputResourceTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    GoogleFontServiceInputResource::InitStats(statistics());

    rewrite_driver()->SetSessionFetcher(
        new UserAgentSensitiveTestFetcher(rewrite_driver()->async_fetcher()));

    // Font loader CSS gets Cache-Control:private, max-age=86400
    ResponseHeaders response_headers;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &response_headers);
    response_headers.SetDateAndCaching(
        timer()->NowMs(), 86400 * Timer::kSecondMs, ", private");

    // Set them up in spots where UaSensitiveFetcher would direct them.
    SetFetchResponse(StrCat(kRoboto, "&UA=Chromezilla"),
                     response_headers, "font_chromezilla");

    SetFetchResponse(StrCat(kRoboto, "&UA=Safieri"),
                     response_headers, "font_safieri");

    SetFetchResponse(StrCat(kRobotoSsl, "&UA=Chromezilla"),
                     response_headers, "sfont_chromezilla");

    SetFetchResponse(StrCat(kRobotoSsl, "&UA=Safieri"),
                     response_headers, "sfont_safieri");

    ResponseHeaders non_css;
    SetDefaultLongCacheHeaders(&kContentTypeText, &non_css);
    SetFetchResponse(StrCat(kNonCss, "?UA=Chromezilla"),
                     non_css, "something weird");
  }
};

TEST_F(GoogleFontServiceInputResourceTest, Creation) {
  rewrite_driver()->SetUserAgent("Chromezilla");

  scoped_ptr<GoogleFontServiceInputResource> resource;

  GoogleUrl url1("efpeRO#@($@#K$!@($");
  resource.reset(GoogleFontServiceInputResource::Make(url1, rewrite_driver()));
  EXPECT_TRUE(resource.get() == NULL);

  GoogleUrl url2("http://example.com/foo.css");
  resource.reset(GoogleFontServiceInputResource::Make(url2, rewrite_driver()));
  EXPECT_TRUE(resource.get() == NULL);

  GoogleUrl url3(kRoboto);
  resource.reset(GoogleFontServiceInputResource::Make(url3, rewrite_driver()));
  ASSERT_TRUE(resource.get() != NULL);
  EXPECT_EQ(kRoboto, resource->url());
  EXPECT_EQ(
      "gfnt://fonts.googleapis.com/css?family=Roboto&X-PS-UA=Chromezilla",
      resource->cache_key());

  GoogleUrl url4(kRobotoSsl);
  resource.reset(GoogleFontServiceInputResource::Make(url4, rewrite_driver()));
  ASSERT_TRUE(resource.get() != NULL);
  EXPECT_EQ(kRobotoSsl, resource->url());
  EXPECT_EQ(
      "gfnts://fonts.googleapis.com/css?family=Roboto&X-PS-UA=Chromezilla",
      resource->cache_key());
}

TEST_F(GoogleFontServiceInputResourceTest, Load) {
  GoogleUrl url(kRoboto);
  rewrite_driver()->SetUserAgent("Chromezilla");

  ResourcePtr resource(
      GoogleFontServiceInputResource::Make(url, rewrite_driver()));
  ASSERT_TRUE(resource.get() != NULL);
  MockResourceCallback callback(resource,
                                server_context()->thread_system());
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_TRUE(callback.success());
  EXPECT_EQ("font_chromezilla", resource->contents());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Make sure it's cached.
  ResourcePtr resource2(
      GoogleFontServiceInputResource::Make(url, rewrite_driver()));
  ASSERT_TRUE(resource2.get() != NULL);
  MockResourceCallback callback2(resource2,
                                 server_context()->thread_system());
  resource2->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       rewrite_driver()->request_context(),
                       &callback2);
  EXPECT_TRUE(callback2.done());
  EXPECT_TRUE(callback2.success());
  EXPECT_EQ("font_chromezilla", resource2->contents());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // But that different UA gets different string.
  rewrite_driver()->SetUserAgent("Safieri");
  ResourcePtr resource3(
      GoogleFontServiceInputResource::Make(url, rewrite_driver()));
  ASSERT_TRUE(resource3.get() != NULL);
  MockResourceCallback callback3(resource3,
                                 server_context()->thread_system());
  resource3->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       rewrite_driver()->request_context(),
                       &callback3);
  EXPECT_TRUE(callback3.done());
  EXPECT_TRUE(callback3.success());
  EXPECT_EQ("font_safieri", resource3->contents());
  EXPECT_EQ(2, counting_url_async_fetcher()->fetch_count());
}

TEST_F(GoogleFontServiceInputResourceTest, UANormalization) {
  const char kIE7a[] =
      "Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; Trident/4.0; "
      ".NET CLR 2.0.50727; .NET CLR 3.0.4506.2152; .NET CLR 3.5.30729)";

  GoogleUrl url(kRoboto);
  scoped_ptr<GoogleUrl> url_plus_ua(url.CopyAndAddQueryParam("UA", kIE7a));
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &response_headers);
  response_headers.SetDateAndCaching(
      timer()->NowMs(), 86400 * Timer::kSecondMs, ", private");
  SetFetchResponse(url_plus_ua->Spec(), response_headers, "font_IE7");

  // Try fetches with a couple of possible aliases. The one we uploaded it under
  // is first, since it's the only one the fetcher replies to.
  rewrite_driver()->SetUserAgent(kIE7a);

  ResourcePtr resource(
      GoogleFontServiceInputResource::Make(url, rewrite_driver()));
  ASSERT_TRUE(resource.get() != NULL);
  MockResourceCallback callback(resource,
                                server_context()->thread_system());
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_TRUE(callback.success());
  EXPECT_EQ("font_IE7", resource->contents());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Different list of .NET versions.
  const char kIE7b[] =
      "Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; Trident/4.0; "
      ".NET CLR 2.0.50727; .NET4.0C; .NET4.0E)";
  rewrite_driver()->SetUserAgent(kIE7b);

  ResourcePtr resource2(
      GoogleFontServiceInputResource::Make(url, rewrite_driver()));
  ASSERT_TRUE(resource2.get() != NULL);
  MockResourceCallback callback2(resource2,
                                 server_context()->thread_system());
  resource2->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       rewrite_driver()->request_context(),
                       &callback2);
  EXPECT_TRUE(callback2.done());
  EXPECT_TRUE(callback2.success());
  EXPECT_EQ("font_IE7", resource->contents());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(GoogleFontServiceInputResourceTest, LoadParallel) {
  GoogleUrl url(kRoboto);
  SetupWaitFetcher();

  rewrite_driver()->SetUserAgent("Chromezilla");
  ResourcePtr resource(
      GoogleFontServiceInputResource::Make(url, rewrite_driver()));
  ASSERT_TRUE(resource.get() != NULL);
  MockResourceCallback callback(resource,
                                server_context()->thread_system());
  resource->LoadAsync(Resource::kReportFailureIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback);
  EXPECT_FALSE(callback.done());

  rewrite_driver()->SetUserAgent("Safieri");
  ResourcePtr resource2(
      GoogleFontServiceInputResource::Make(url, rewrite_driver()));
  ASSERT_TRUE(resource2.get() != NULL);
  MockResourceCallback callback2(resource2,
                                 server_context()->thread_system());
  resource2->LoadAsync(Resource::kReportFailureIfNotCacheable,
                       rewrite_driver()->request_context(),
                       &callback2);
  EXPECT_FALSE(callback2.done());

  CallFetcherCallbacks();

  ASSERT_TRUE(callback.done());
  EXPECT_TRUE(callback.success());
  EXPECT_EQ("font_chromezilla", resource->contents());

  ASSERT_TRUE(callback2.done());
  EXPECT_TRUE(callback2.success());
  EXPECT_EQ("font_safieri", resource2->contents());
}

TEST_F(GoogleFontServiceInputResourceTest, FetchFailure) {
  SetFetchFailOnUnexpected(false);

  // Regression test --- don't crash when fetch fails.
  // Bug discovered by accident due to a bug in a test.
  rewrite_driver()->SetUserAgent("Huhzilla");
  GoogleUrl url(kRoboto);
  ResourcePtr resource(
      GoogleFontServiceInputResource::Make(url, rewrite_driver()));
  ASSERT_TRUE(resource.get() != NULL);
  MockResourceCallback callback(resource,
                                server_context()->thread_system());
  resource->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_FALSE(callback.success());
  EXPECT_EQ("", resource->contents());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

TEST_F(GoogleFontServiceInputResourceTest, DontLoadNonCss) {
  rewrite_driver()->SetUserAgent("Chromezilla");

  GoogleUrl non_css_url(kNonCss);
  ResourcePtr resource(
      GoogleFontServiceInputResource::Make(non_css_url, rewrite_driver()));
  ASSERT_TRUE(resource.get() != NULL);
  MockResourceCallback callback(resource,
                                server_context()->thread_system());
  resource->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                      rewrite_driver()->request_context(),
                      &callback);
  EXPECT_TRUE(callback.done());
  EXPECT_FALSE(resource->HttpStatusOk());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  // Make sure we don't end up caching a success, either.
  ResourcePtr resource2(
      GoogleFontServiceInputResource::Make(non_css_url, rewrite_driver()));
  ASSERT_TRUE(resource2.get() != NULL);
  MockResourceCallback callback2(resource2,
                                 server_context()->thread_system());
  resource2->LoadAsync(Resource::kLoadEvenIfNotCacheable,
                       rewrite_driver()->request_context(),
                       &callback2);
  EXPECT_TRUE(callback2.done());
  EXPECT_FALSE(resource->HttpStatusOk());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
}

}  // namespace

}  // namespace net_instaweb
