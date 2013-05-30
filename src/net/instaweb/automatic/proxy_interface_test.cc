/*
 * Copyright 2011 Google Inc.
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

// Unit-tests for ProxyInterface.

#include "net/instaweb/automatic/public/proxy_interface.h"

#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/automatic/public/proxy_interface_test_base.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/reflecting_test_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/thread_synchronizer.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

namespace {

// This jpeg file lacks a .jpg or .jpeg extension.  So we initiate
// a property-cache read prior to getting the response-headers back,
// but will never go into the ProxyFetch flow that blocks waiting
// for the cache lookup to come back.
const char kImageFilenameLackingExt[] = "jpg_file_lacks_ext";
const char kHttpsPageUrl[] = "https://www.test.com/page.html";
const char kHttpsCssUrl[] = "https://www.test.com/style.css";

const char kCssContent[] = "* { display: none; }";
const char kMinimizedCssContent[] = "*{display:none}";

}  // namespace

class ProxyInterfaceTest : public ProxyInterfaceTestBase {
 protected:
  static const int kHtmlCacheTimeSec = 5000;

  ProxyInterfaceTest()
      : start_time_ms_(0),
        max_age_300_("max-age=300"),
        request_start_time_ms_(-1) {
    ConvertTimeToString(MockTimer::kApr_5_2010_ms, &start_time_string_);
    ConvertTimeToString(MockTimer::kApr_5_2010_ms + 5 * Timer::kMinuteMs,
                        &start_time_plus_300s_string_);
    ConvertTimeToString(MockTimer::kApr_5_2010_ms - 2 * Timer::kDayMs,
                        &old_time_string_);
  }
  virtual ~ProxyInterfaceTest() {}

  virtual void SetUp() {
    RewriteOptions* options = server_context()->global_options();
    server_context_->set_enable_property_cache(true);
    const PropertyCache::Cohort* dom_cohort =
        SetupCohort(server_context_->page_property_cache(),
                    RewriteDriver::kDomCohort);
    const PropertyCache::Cohort* blink_cohort =
        SetupCohort(server_context_->page_property_cache(),
                    BlinkUtil::kBlinkCohort);
    server_context()->set_dom_cohort(dom_cohort);
    server_context()->set_blink_cohort(blink_cohort);
    options->ClearSignatureForTesting();
    options->EnableFilter(RewriteOptions::kRewriteCss);
    options->set_max_html_cache_time_ms(kHtmlCacheTimeSec * Timer::kSecondMs);
    options->set_in_place_rewriting_enabled(true);
    options->Disallow("*blacklist*");
    server_context()->ComputeSignature(options);
    ProxyInterfaceTestBase::SetUp();
    // The original url_async_fetcher() is still owned by RewriteDriverFactory.
    background_fetch_fetcher_.reset(new BackgroundFetchCheckingUrlAsyncFetcher(
        factory()->ComputeUrlAsyncFetcher()));
    server_context()->set_default_system_fetcher(
        background_fetch_fetcher_.get());

    start_time_ms_ = timer()->NowMs();

    SetResponseWithDefaultHeaders(kImageFilenameLackingExt, kContentTypeJpeg,
                                  "image data", 300);
    SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml,
                                  "<div><p></p></div>", 0);
  }

  void CheckHeaders(const ResponseHeaders& headers,
                    const ContentType& expect_type) {
    ASSERT_TRUE(headers.has_status_code());
    EXPECT_EQ(HttpStatus::kOK, headers.status_code());
    EXPECT_STREQ(expect_type.mime_type(),
                 headers.Lookup1(HttpAttributes::kContentType));
  }

  void CheckBackgroundFetch(const ResponseHeaders& headers,
                            bool is_background_fetch) {
    EXPECT_STREQ(is_background_fetch ? "1" : "0",
              headers.Lookup1(kBackgroundFetchHeader));
  }

  void CheckNumBackgroundFetches(int num) {
    EXPECT_EQ(num, background_fetch_fetcher_->num_background_fetches());
  }

  virtual void ClearStats() {
    RewriteTestBase::ClearStats();
    background_fetch_fetcher_->clear_num_background_fetches();
  }

  // Serve a trivial HTML page with initial Cache-Control header set to
  // input_cache_control and return the Cache-Control header after running
  // through ProxyInterface.
  //
  // A unique id must be set to assure different websites are requested.
  // id is put in a URL, so it probably shouldn't have spaces and other
  // special chars.
  GoogleString RewriteHtmlCacheHeader(const StringPiece& id,
                                      const StringPiece& input_cache_control) {
    GoogleString url = StrCat("http://www.example.com/", id, ".html");
    ResponseHeaders input_headers;
    DefaultResponseHeaders(kContentTypeHtml, 100, &input_headers);
    input_headers.Replace(HttpAttributes::kCacheControl, input_cache_control);
    SetFetchResponse(url, input_headers, "<body>Foo</body>");

    GoogleString body;
    ResponseHeaders output_headers;
    FetchFromProxy(url, true, &body, &output_headers);
    ConstStringStarVector values;
    output_headers.Lookup(HttpAttributes::kCacheControl, &values);
    return JoinStringStar(values, ", ");
  }

  int GetStatusCodeInPropertyCache(const GoogleString& url) {
    PropertyCache* pcache = page_property_cache();
    StringPiece device_type_suffix =
        UserAgentMatcher::DeviceTypeSuffix(UserAgentMatcher::kDesktop);
    GoogleString cache_key = StrCat(url, device_type_suffix);
    scoped_ptr<MockPropertyPage> page(NewMockPage(cache_key));
    const PropertyCache::Cohort* cohort = pcache->GetCohort(
        RewriteDriver::kDomCohort);
    PropertyValue* value;
    pcache->Read(page.get());
    value = page->GetProperty(
        cohort, RewriteDriver::kStatusCodePropertyName);
    int status_code;
    EXPECT_TRUE(StringToInt(value->value(), &status_code));
    return status_code;
  }

  GoogleString GetDefaultUserAgentForDeviceType(
      UserAgentMatcher::DeviceType device_type) {
    switch (device_type) {
      case UserAgentMatcher::kMobile:
        return UserAgentMatcherTestBase::kAndroidICSUserAgent;
      case UserAgentMatcher::kTablet:
        return UserAgentMatcherTestBase::kIPadUserAgent;
      case UserAgentMatcher::kDesktop:
      case UserAgentMatcher::kEndOfDeviceType:
      default:
        return UserAgentMatcherTestBase::kChromeUserAgent;
    }
  }

  void TestOptionsAndDeviceTypeUsedInCacheKey(
      UserAgentMatcher::DeviceType device_type) {
    GoogleUrl gurl("http://www.test.com/");
    StringAsyncFetch callback(
        RequestContext::NewTestRequestContext(
            server_context()->thread_system()));

    const GoogleString& user_agent =
        GetDefaultUserAgentForDeviceType(device_type);
    RequestHeaders request_headers;
    request_headers.Replace(HttpAttributes::kUserAgent, user_agent);
    callback.set_request_headers(&request_headers);
    scoped_ptr<ProxyFetchPropertyCallbackCollector> callback_collector(
        proxy_interface_->InitiatePropertyCacheLookup(
        false, gurl, options(), &callback, false, NULL));
    EXPECT_NE(static_cast<ProxyFetchPropertyCallbackCollector*>(NULL),
              callback_collector.get());
    PropertyPage* page = callback_collector->property_page();
    EXPECT_NE(static_cast<PropertyPage*>(NULL), page);
    server_context()->ComputeSignature(options());
    GoogleString expected = StrCat(
        gurl.Spec(), "_",
        server_context_->hasher()->Hash(options()->signature()),
        UserAgentMatcher::DeviceTypeSuffix(device_type));
    EXPECT_EQ(expected, page->key());
  }

  void DisableAjax() {
    RewriteOptions* options = server_context()->global_options();
    options->ClearSignatureForTesting();
    options->set_in_place_rewriting_enabled(false);
    server_context()->ComputeSignature(options);
  }

  void RejectBlacklisted() {
    RewriteOptions* options = server_context()->global_options();
    options->ClearSignatureForTesting();
    options->set_reject_blacklisted(true);
    options->set_reject_blacklisted_status_code(HttpStatus::kImATeapot);
    server_context()->ComputeSignature(options);
  }

  // This function is primarily meant to enable writes to the dom cohort of the
  // property cache. Writes to this cohort are predicated on a filter that uses
  // that cohort being enabled, which includes the insert dns prefetch filter.
  void EnableDomCohortWritesWithDnsPrefetch() {
    RewriteOptions* options = server_context()->global_options();
    options->ClearSignatureForTesting();
    options->EnableFilter(RewriteOptions::kInsertDnsPrefetch);
    server_context()->ComputeSignature(options);
  }

  void TestFallbackPageProperties(
      const GoogleString& url, const GoogleString& fallback_url) {
    GoogleUrl gurl(url);
    GoogleString kPropertyName("prop");
    GoogleString kValue("value");
    options()->set_use_fallback_property_cache_values(true);
    // No fallback value is present.
    const PropertyCache::Cohort* cohort =
        page_property_cache()->GetCohort(RewriteDriver::kDomCohort);
    StringAsyncFetch callback(
        RequestContext::NewTestRequestContext(
            server_context()->thread_system()));
    RequestHeaders request_headers;
    callback.set_request_headers(&request_headers);
    scoped_ptr<ProxyFetchPropertyCallbackCollector> callback_collector(
        proxy_interface_->InitiatePropertyCacheLookup(
            false, gurl, options(), &callback, false, NULL));

    FallbackPropertyPage* fallback_page =
        callback_collector->fallback_property_page();
    fallback_page->UpdateValue(cohort, kPropertyName, kValue);
    fallback_page->WriteCohort(cohort);

    // Read from fallback value.
    GoogleUrl new_gurl(fallback_url);
    callback_collector.reset(proxy_interface_->InitiatePropertyCacheLookup(
        false, new_gurl, options(), &callback, false, NULL));
    fallback_page = callback_collector->fallback_property_page();
    EXPECT_FALSE(fallback_page->actual_property_page()->GetProperty(
        cohort, kPropertyName)->has_value());
    EXPECT_EQ(kValue,
              fallback_page->GetProperty(cohort, kPropertyName)->value());

    // If use_fallback_property_cache_values option is set to false, fallback
    // values will not be used.
    options()->ClearSignatureForTesting();
    options()->set_use_fallback_property_cache_values(false);
    callback_collector.reset(proxy_interface_->InitiatePropertyCacheLookup(
          false, new_gurl, options(), &callback, false, NULL));
    EXPECT_FALSE(callback_collector->fallback_property_page()->GetProperty(
        cohort, kPropertyName)->has_value());
  }

  scoped_ptr<BackgroundFetchCheckingUrlAsyncFetcher> background_fetch_fetcher_;
  int64 start_time_ms_;
  GoogleString start_time_string_;
  GoogleString start_time_plus_300s_string_;
  GoogleString old_time_string_;
  const GoogleString max_age_300_;
  int64 request_start_time_ms_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyInterfaceTest);
};

TEST_F(ProxyInterfaceTest, LoggingInfo) {
  GoogleString url = "http://www.example.com/";
  GoogleString text;
  RequestHeaders request_headers;
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);

  // Fetch HTML content.
  mock_url_fetcher_.SetResponse(url, headers, "<html></html>");
  FetchFromProxy(url, request_headers, true, &text, &headers);

  CheckBackgroundFetch(headers, false);
  CheckNumBackgroundFetches(0);
  const RequestContext::TimingInfo& rti = timing_info();
  int64 latency_ms;
  ASSERT_TRUE(rti.GetHTTPCacheLatencyMs(&latency_ms));
  EXPECT_EQ(0, latency_ms);
  EXPECT_FALSE(rti.GetL2HTTPCacheLatencyMs(&latency_ms));

  EXPECT_FALSE(rti.GetFetchHeaderLatencyMs(&latency_ms));
  EXPECT_FALSE(rti.GetFetchLatencyMs(&latency_ms));
  EXPECT_TRUE(logging_info()->is_html_response());
  EXPECT_FALSE(logging_info()->is_url_disallowed());
  EXPECT_FALSE(logging_info()->is_request_disabled());
  EXPECT_FALSE(logging_info()->is_pagespeed_resource());

  // Fetch non-HTML content.
  logging_info()->Clear();
  mock_url_fetcher_.SetResponse(url, headers, "js");
  FetchFromProxy(url, request_headers, true, &text, &headers);
  EXPECT_FALSE(logging_info()->is_html_response());
  EXPECT_FALSE(logging_info()->is_url_disallowed());
  EXPECT_FALSE(logging_info()->is_request_disabled());

  // Fetch blacklisted url.
  url = "http://www.blacklist.com/";
  logging_info()->Clear();
  mock_url_fetcher_.SetResponse(url, headers, "<html></html>");
  FetchFromProxy(url, request_headers, true, &text, &headers);
  EXPECT_TRUE(logging_info()->is_html_response());
  EXPECT_TRUE(logging_info()->is_url_disallowed());
  EXPECT_FALSE(logging_info()->is_request_disabled());

  // Fetch disabled url.
  url = "http://www.example.com/?PageSpeed=off";
  logging_info()->Clear();
  mock_url_fetcher_.SetResponse("http://www.example.com/", headers,
                                "<html></html>");
  FetchFromProxy(url, request_headers, true, &text, &headers);
  EXPECT_TRUE(logging_info()->is_html_response());
  EXPECT_FALSE(logging_info()->is_url_disallowed());
  EXPECT_TRUE(logging_info()->is_request_disabled());
}

TEST_F(ProxyInterfaceTest, SkipPropertyCacheLookupIfOptionsNotEnabled) {
  GoogleString url = "http://www.example.com/";
  GoogleString text;
  RequestHeaders request_headers;
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);

  // Fetch disabled url.
  url = "http://www.example.com/?PageSpeed=off";
  logging_info()->Clear();
  mock_url_fetcher_.SetResponse("http://www.example.com/", headers,
                                "<html></html>");
  FetchFromProxy(url, request_headers, true, &text, &headers);
  EXPECT_TRUE(logging_info()->is_html_response());
  EXPECT_FALSE(logging_info()->is_url_disallowed());
  EXPECT_TRUE(logging_info()->is_request_disabled());

  // Only the HTTP response lookup is issued and it is not in the cache.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
}

TEST_F(ProxyInterfaceTest, SkipPropertyCacheLookupIfUrlBlacklisted) {
  GoogleString url = "http://www.blacklist.com/";
  RequestHeaders request_headers;
  GoogleString text;
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);

  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());

  custom_options->AddRejectedUrlWildcard(AbsolutifyUrl("blacklist*"));
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  logging_info()->Clear();
  mock_url_fetcher_.SetResponse(url, headers, "<html></html>");
  FetchFromProxy(url, request_headers, true, &text, &headers);
  EXPECT_TRUE(logging_info()->is_html_response());
  EXPECT_TRUE(logging_info()->is_url_disallowed());
  EXPECT_FALSE(logging_info()->is_request_disabled());

  // Only the HTTP response lookup is issued and it is not in the cache.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
}

TEST_F(ProxyInterfaceTest, HeadRequest) {
  // Test to check if we are handling Head requests correctly.
  GoogleString url = "http://www.example.com/";
  GoogleString set_text, get_text;
  RequestHeaders request_headers;
  ResponseHeaders set_headers, get_headers;

  set_headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  set_headers.SetStatusAndReason(HttpStatus::kOK);

  set_text = "<html></html>";

  mock_url_fetcher_.SetResponse(url, set_headers, set_text);
  FetchFromProxy(url, request_headers, true, &get_text, &get_headers);

  // Headers and body are correct for a Get request.
  EXPECT_EQ("HTTP/1.0 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "X-Background-Fetch: 0\r\n"
            "Date: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
            "Expires: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
            "Cache-Control: max-age=0, private\r\n"
            "X-Page-Speed: \r\n"
            "HeadersComplete: 1\r\n\r\n", get_headers.ToString());
  EXPECT_EQ(set_text, get_text);

  // Remove from the cache so we can actually test a HEAD fetch.
  http_cache()->Delete(url);

  ClearStats();

  // Headers and body are correct for a Head request.
  request_headers.set_method(RequestHeaders::kHead);
  FetchFromProxy(url, request_headers, true, &get_text, &get_headers);

  EXPECT_EQ(0, http_cache()->cache_hits()->Get());

  EXPECT_EQ("HTTP/1.0 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "X-Background-Fetch: 0\r\n"
            "X-Page-Speed: \r\n"
            "HeadersComplete: 1\r\n\r\n", get_headers.ToString());
  EXPECT_TRUE(get_text.empty());
}

TEST_F(ProxyInterfaceTest, RedirectRequestWhenDomainRewriterEnabled) {
  // Test to check if we are handling Head requests correctly.
  GoogleString url = "http://www.example.com/";
  GoogleString set_text, get_text;
  RequestHeaders request_headers;
  ResponseHeaders set_headers, get_headers;
  NullMessageHandler handler;

  set_headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  set_headers.Add(HttpAttributes::kLocation, "http://m.example.com");
  set_headers.SetStatusAndReason(HttpStatus::kFound);
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kRewriteDomains);
  custom_options->WriteableDomainLawyer()->AddTwoProtocolRewriteDomainMapping(
      "www.example.com", "m.example.com", &handler);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);
  set_text = "<html></html>";
  mock_url_fetcher_.SetResponse(url, set_headers, set_text);
  FetchFromProxy(url, request_headers, true, &get_text, &get_headers);

  // Headers and body are correct for a Get request.
  EXPECT_EQ("HTTP/1.0 302 Found\r\n"
            "Content-Type: text/html\r\n"
            "Location: http://www.example.com/\r\n"
            "X-Background-Fetch: 0\r\n"
            "Date: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
            "Expires: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
            "Cache-Control: max-age=0, private\r\n"
            "X-Page-Speed: \r\n"
            "HeadersComplete: 1\r\n\r\n", get_headers.ToString());
}

TEST_F(ProxyInterfaceTest, HeadResourceRequest) {
  // Test to check if we are handling Head requests correctly in pagespeed
  // resource flow.
  const char kCssWithEmbeddedImage[] = "*{background-image:url(%s)}";
  const char kBackgroundImage[] = "1.png";

  GoogleString text;
  RequestHeaders request_headers;
  ResponseHeaders response_headers;
  GoogleString expected_response_headers_string = "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/css\r\n"
      "X-Background-Fetch: 0\r\n"
      "Date: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
      "Expires: Tue, 02 Feb 2010 18:56:26 GMT\r\n"
      "Cache-Control: max-age=300,private\r\n"
      "X-Page-Speed: \r\n"
      "HeadersComplete: 1\r\n\r\n";

  // We're not going to image-compress so we don't need our mock image
  // to really be an image.
  SetResponseWithDefaultHeaders(kBackgroundImage, kContentTypePng, "image",
                                kHtmlCacheTimeSec * 2);
  GoogleString orig_css = StringPrintf(kCssWithEmbeddedImage, kBackgroundImage);
  SetResponseWithDefaultHeaders("embedded.css", kContentTypeCss,
                                orig_css, kHtmlCacheTimeSec * 2);

  // By default, cache extension is off in the default options.
  server_context()->global_options()->SetDefaultRewriteLevel(
      RewriteOptions::kPassThrough);

  // Because cache-extension was turned off, the image in the CSS file
  // will not be changed.
  FetchFromProxy("I.embedded.css.pagespeed.cf.0.css", request_headers,
                 true, &text, &response_headers);
  EXPECT_TRUE(logging_info()->is_pagespeed_resource());
  EXPECT_EQ(expected_response_headers_string, response_headers.ToString());
  EXPECT_EQ(orig_css, text);
  // Headers and body are correct for a Head request.
  request_headers.set_method(RequestHeaders::kHead);
  FetchFromProxy("I.embedded.css.pagespeed.cf.0.css", request_headers,
                 true, &text, &response_headers);

  // This leads to a conditional refresh of the original resource.
  expected_response_headers_string = "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/css\r\n"
      "X-Background-Fetch: 0\r\n"
      "Etag: W/\"PSA-0\"\r\n"
      "Date: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
      "Expires: Tue, 02 Feb 2010 18:56:26 GMT\r\n"
      "Cache-Control: max-age=300,private\r\n"
      "X-Page-Speed: \r\n"
      "HeadersComplete: 1\r\n\r\n";

  EXPECT_EQ(expected_response_headers_string, response_headers.ToString());
  EXPECT_TRUE(text.empty());
}

TEST_F(ProxyInterfaceTest, FetchFailure) {
  GoogleString text;
  ResponseHeaders headers;

  // We don't want fetcher to fail the test, merely the fetch.
  SetFetchFailOnUnexpected(false);
  FetchFromProxy("invalid", false, &text, &headers);
  CheckBackgroundFetch(headers, false);
  CheckNumBackgroundFetches(0);
}

TEST_F(ProxyInterfaceTest, ReturnUnavailableForBlockedUrls) {
  GoogleString text;
  ResponseHeaders response_headers;
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(AbsolutifyUrl("blocked"), response_headers,
                                "<html></html>");
  FetchFromProxy("blocked", true,
                 &text, &response_headers);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());

  text.clear();
  response_headers.Clear();

  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());

  custom_options->AddRejectedUrlWildcard(AbsolutifyUrl("block*"));
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  FetchFromProxy("blocked", false, &text, &response_headers);
  EXPECT_EQ(HttpStatus::kProxyDeclinedRequest, response_headers.status_code());
}

TEST_F(ProxyInterfaceTest, RewriteUrlsEarly) {
  GoogleString text;
  ResponseHeaders response_headers;
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  NullMessageHandler handler;
  mock_url_fetcher_.SetResponse(StrCat(kTestDomain, "index.html"),
                                response_headers,
                                "<html></html>");
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->WriteableDomainLawyer()->AddOriginDomainMapping(
      "test.com", "pagespeed.test.com/test.com", &handler);
  custom_options->set_rewrite_request_urls_early(true);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);
  FetchFromProxy("http://pagespeed.test.com/test.com/index.html", true,
                 &text, &response_headers);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
  EXPECT_EQ("<html></html>", text);
}

TEST_F(ProxyInterfaceTest, RewriteUrlsEarlyUsingReferer) {
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  NullMessageHandler handler;
  mock_url_fetcher_.SetResponse(StrCat(kTestDomain, "index.html"),
                                response_headers,
                                "<html></html>");
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->WriteableDomainLawyer()->AddOriginDomainMapping(
      "test.com", "pagespeed.test.com/test.com", &handler);
  custom_options->set_rewrite_request_urls_early(true);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);
  request_headers.Replace(HttpAttributes::kReferer,
                          "http://pagespeed.test.com/test.com/");
  FetchFromProxy("http://pagespeed.test.com/index.html", request_headers, true,
                 &text, &response_headers);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
  EXPECT_EQ("<html></html>", text);
}

TEST_F(ProxyInterfaceTest, ReturnUnavailableForBlockedHeaders) {
  GoogleString text;
  RequestHeaders request_headers;
  ResponseHeaders response_headers;
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(kTestDomain, response_headers, "<html></html>");
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());

  custom_options->AddRejectedHeaderWildcard(HttpAttributes::kUserAgent,
                                            "*Chrome*");
  custom_options->AddRejectedHeaderWildcard(HttpAttributes::kXForwardedFor,
                                            "10.3.4.*");
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  request_headers.Add(HttpAttributes::kUserAgent, "Firefox");
  request_headers.Add(HttpAttributes::kXForwardedFor, "10.0.0.11");
  FetchFromProxy(kTestDomain, request_headers, true,
                 &text, &response_headers);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());

  request_headers.Clear();
  response_headers.Clear();

  request_headers.Add(HttpAttributes::kUserAgent, "abc");
  request_headers.Add(HttpAttributes::kUserAgent, "xyz Chrome abc");
  FetchFromProxy(kTestDomain, request_headers, false,
                 &text, &response_headers);
  EXPECT_EQ(HttpStatus::kProxyDeclinedRequest, response_headers.status_code());

  request_headers.Clear();
  response_headers.Clear();

  request_headers.Add(HttpAttributes::kXForwardedFor, "10.3.4.32");
  FetchFromProxy(kTestDomain, request_headers, false,
                 &text, &response_headers);
  EXPECT_EQ(HttpStatus::kProxyDeclinedRequest, response_headers.status_code());
}

TEST_F(ProxyInterfaceTest, PassThrough404) {
  GoogleString text;
  ResponseHeaders headers;
  SetFetchResponse404("404");
  FetchFromProxy("404", true, &text, &headers);
  ASSERT_TRUE(headers.has_status_code());
  EXPECT_EQ(HttpStatus::kNotFound, headers.status_code());
}

TEST_F(ProxyInterfaceTest, PassThroughResource) {
  GoogleString text;
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";

  SetResponseWithDefaultHeaders("text.txt", kContentTypeText, kContent,
                                kHtmlCacheTimeSec * 2);
  FetchFromProxy("text.txt", true, &text, &headers);
  CheckHeaders(headers, kContentTypeText);
  CheckBackgroundFetch(headers, false);
  CheckNumBackgroundFetches(0);
  EXPECT_EQ(kContent, text);
}

TEST_F(ProxyInterfaceTest, PassThroughEmptyResource) {
  ResponseHeaders headers;
  const char kContent[] = "";
  SetDefaultLongCacheHeaders(&kContentTypeText, &headers);
  SetFetchResponse(AbsolutifyUrl("text.txt"), headers, kContent);

  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.txt", true, &text, &response_headers);
  EXPECT_EQ(kContent, text);
  // One lookup for ajax metadata and one for the HTTP response. Neither are
  // found.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  ClearStats();
  GoogleString text2;
  ResponseHeaders response_headers2;
  FetchFromProxy("text.txt", true, &text2, &response_headers2);
  EXPECT_EQ(kContent, text2);
  // The HTTP response is found but the ajax metadata is not found.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
}

TEST_F(ProxyInterfaceTest, SetCookieNotCached) {
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  SetDefaultLongCacheHeaders(&kContentTypeText, &headers);
  headers.Add(HttpAttributes::kSetCookie, "cookie");
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.txt"), headers, kContent);

  // The first response served by the fetcher has Set-Cookie headers.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.txt", true, &text, &response_headers);
  EXPECT_STREQ("cookie", response_headers.Lookup1(HttpAttributes::kSetCookie));
  EXPECT_EQ(kContent, text);
  // One lookup for ajax metadata and one for the HTTP response. Neither are
  // found.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  ClearStats();
  // The next response that is served from cache does not have any Set-Cookie
  // headers.
  GoogleString text2;
  ResponseHeaders response_headers2;
  FetchFromProxy("text.txt", true, &text2, &response_headers2);
  EXPECT_EQ(NULL, response_headers2.Lookup1(HttpAttributes::kSetCookie));
  EXPECT_EQ(kContent, text2);
  // The HTTP response is found but the ajax metadata is not found.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
}

TEST_F(ProxyInterfaceTest, SetCookie2NotCached) {
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  SetDefaultLongCacheHeaders(&kContentTypeText, &headers);
  headers.Add(HttpAttributes::kSetCookie2, "cookie");
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.txt"), headers, kContent);

  // The first response served by the fetcher has Set-Cookie headers.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.txt", true, &text, &response_headers);
  EXPECT_STREQ("cookie", response_headers.Lookup1(HttpAttributes::kSetCookie2));
  EXPECT_EQ(kContent, text);
  // One lookup for ajax metadata and one for the HTTP response. Neither are
  // found.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  ClearStats();
  // The next response that is served from cache does not have any Set-Cookie
  // headers.
  GoogleString text2;
  ResponseHeaders response_headers2;
  FetchFromProxy("text.txt", true, &text2, &response_headers2);
  EXPECT_EQ(NULL, response_headers2.Lookup1(HttpAttributes::kSetCookie2));
  EXPECT_EQ(kContent, text2);
  // The HTTP response is found but the ajax metadata is not found.
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
}

TEST_F(ProxyInterfaceTest, NotCachedIfAuthorizedAndNotPublic) {
  // We should not cache things which are default cache-control if we
  // are sending Authorization:. See RFC 2616, 14.8.
  ReflectingTestFetcher reflect;
  server_context()->set_default_system_fetcher(&reflect);

  RequestHeaders request_headers;
  request_headers.Add("Was", "Here");
  request_headers.Add(HttpAttributes::kAuthorization, "Secret");
  // This will get reflected as well, and hence will determine whether
  // cacheable or not.
  request_headers.Replace(HttpAttributes::kCacheControl, "max-age=600000");

  ResponseHeaders out_headers;
  GoogleString out_text;
  // Using .txt here so we don't try any AJAX rewriting.
  FetchFromProxy("http://test.com/file.txt",
                 request_headers,  true, &out_text, &out_headers);
  // We should see the request headers we sent back as the response headers
  // as we're using a ReflectingTestFetcher.
  EXPECT_STREQ("Here", out_headers.Lookup1("Was"));

  // Not cross-domain, so should propagate out header.
  EXPECT_TRUE(out_headers.Has(HttpAttributes::kAuthorization));

  // Should not have written anything to cache, due to the authorization
  // header.
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());

  ClearStats();

  // Now try again. This time no authorization header, different 'Was'.
  request_headers.Replace("Was", "There");
  request_headers.RemoveAll(HttpAttributes::kAuthorization);

  FetchFromProxy("http://test.com/file.txt",
                 request_headers,  true, &out_text, &out_headers);
  // Should get different headers since we should not be cached.
  EXPECT_STREQ("There", out_headers.Lookup1("Was"));
  EXPECT_FALSE(out_headers.Has(HttpAttributes::kAuthorization));

  // And should be a miss per stats.
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());

  mock_scheduler()->AwaitQuiescence();
}

TEST_F(ProxyInterfaceTest, CachedIfAuthorizedAndPublic) {
  // This with Cache-Control: public should be cached even if
  // we are sending Authorization:. See RFC 2616.
  ReflectingTestFetcher reflect;
  server_context()->set_default_system_fetcher(&reflect);

  RequestHeaders request_headers;
  request_headers.Add("Was", "Here");
  request_headers.Add(HttpAttributes::kAuthorization, "Secret");
  // This will get reflected as well, and hence will determine whether
  // cacheable or not.
  request_headers.Replace(HttpAttributes::kCacheControl, "max-age=600000");
  request_headers.Add(HttpAttributes::kCacheControl, "public");  // unlike above

  ResponseHeaders out_headers;
  GoogleString out_text;
  // Using .txt here so we don't try any AJAX rewriting.
  FetchFromProxy("http://test.com/file.txt",
                 request_headers,  true, &out_text, &out_headers);
  EXPECT_STREQ("Here", out_headers.Lookup1("Was"));

  // Not cross-domain, so should propagate out header.
  EXPECT_TRUE(out_headers.Has(HttpAttributes::kAuthorization));

  // Should have written the result to the cache, despite the request having
  // Authorization: thanks to cache-control: public,
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());

  ClearStats();

  // Now try again. This time no authorization header, different 'Was'.
  request_headers.Replace("Was", "There");
  request_headers.RemoveAll(HttpAttributes::kAuthorization);

  FetchFromProxy("http://test.com/file.txt",
                 request_headers,  true, &out_text, &out_headers);
  // Should get old headers, since original was cacheable.
  EXPECT_STREQ("Here", out_headers.Lookup1("Was"));

  // ... of course hopefully a real server won't serve secrets on a
  // cache-control: public page.
  EXPECT_STREQ("Secret", out_headers.Lookup1(HttpAttributes::kAuthorization));

  // And should be a hit per stats.
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());

  mock_scheduler()->AwaitQuiescence();
}

TEST_F(ProxyInterfaceTest, ImplicitCachingHeadersForCss) {
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetDate(MockTimer::kApr_5_2010_ms);
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.css"), headers, kContent);

  // The first response served by the fetcher has caching headers.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.css", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kContent, text);
  // One lookup for ajax metadata, one for the HTTP response and one by the css
  // filter which looks up metadata while rewriting. None are found.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  ClearStats();
  // Fetch again from cache. It has the same caching headers.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.css", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kContent, text);
  // One hit for ajax metadata and one for the HTTP response.
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, lru_cache()->num_misses());
}

TEST_F(ProxyInterfaceTest, CacheableSize) {
  // Test to check that we are not caching responses which have content length >
  // max_cacheable_response_content_length.
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.SetDateAndCaching(MockTimer::kApr_5_2010_ms, 300 * Timer::kSecondMs);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.html"), headers, kContent);

  // Set the set_max_cacheable_response_content_length to 10 bytes.
  http_cache()->set_max_cacheable_response_content_length(10);

  // Fetch once.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.html", true, &text, &response_headers);

  // One lookup for ajax metadata, one for the HTTP response and one for the
  // property cache entry. None are found.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_inserts());

  // Fetch again. It has the same caching headers.
  ClearStats();
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);

  // None are found as the size is bigger than
  // max_cacheable_response_content_length.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  // Set the set_max_cacheable_response_content_length to 1024 bytes.
  http_cache()->set_max_cacheable_response_content_length(1024);
  ClearStats();
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);
  // None are found.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_inserts());

  // Fetch again.
  ClearStats();
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);

  // One hit for the HTTP response as content is smaller than
  // max_cacheable_response_content_length.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());
}

TEST_F(ProxyInterfaceTest, CacheableSizeAjax) {
  // Test to check that we are not caching responses which have content length >
  // max_cacheable_response_content_length in Ajax flow.
  ResponseHeaders headers;
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetDate(MockTimer::kApr_5_2010_ms);
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.css"), headers, kCssContent);

  http_cache()->set_max_cacheable_response_content_length(0);
  // The first response served by the fetcher and is not rewritten. An ajax
  // rewrite should not be triggered as the content length is greater than
  // max_cacheable_response_content_length.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.css", true, &text, &response_headers);

  EXPECT_EQ(kCssContent, text);
  // One lookup for ajax metadata, one for the HTTP response. None are found.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_inserts());

  ClearStats();
  // Fetch again. Optimized version is not served.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.css", true, &text, &response_headers);

  EXPECT_EQ(kCssContent, text);
  // None are found.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
}

TEST_F(ProxyInterfaceTest, CacheableSizeResource) {
  // Test to check that we are not caching responses which have content length >
  // max_cacheable_response_content_length in resource flow.
  GoogleString text;
  ResponseHeaders headers;

  // Fetching of a rewritten resource we did not just create
  // after an HTML rewrite.
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCssContent,
                                kHtmlCacheTimeSec * 2);
  // Set the set_max_cacheable_response_content_length to 0 bytes.
  http_cache()->set_max_cacheable_response_content_length(0);
  // Fetch fails as original is not accessible.
  FetchFromProxy(Encode("", "cf", "0", "a.css", "css"), false, &text, &headers);
}

TEST_F(ProxyInterfaceTest, InvalidationForCacheableHtml) {
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetDate(MockTimer::kApr_5_2010_ms);
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.SetDateAndCaching(MockTimer::kApr_5_2010_ms, 300 * Timer::kSecondMs);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.html"), headers, kContent);

  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.html", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kContent, text);
  // One lookup for ajax metadata, one for the HTTP response and one for the
  // property cache entry. None are found.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  ClearStats();
  // Fetch again from cache. It has the same caching headers.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kContent, text);
  // One hit for the HTTP response. Misses for the property cache entry and the
  // ajax metadata.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());

  // Change the response.
  SetFetchResponse(AbsolutifyUrl("text.html"), headers, "new");

  ClearStats();
  // Fetch again from cache. It has the same caching headers.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  // We continue to serve the previous response since we've cached it.
  EXPECT_EQ(kContent, text);
  // One hit for the HTTP response. Misses for the property cache entry and the
  // ajax metadata.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());

  // Invalidate the cache.
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->set_cache_invalidation_timestamp(timer()->NowMs());
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  ClearStats();
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  // We get the new response since we've invalidated the cache.
  EXPECT_EQ("new", text);
  // The HTTP response is found in the LRU cache but counts as a miss in the
  // HTTPCache since it has been invalidated. Also, cache misses for the ajax
  // metadata and property cache entry.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());
}

TEST_F(ProxyInterfaceTest, UrlInvalidationForCacheableHtml) {
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.SetDateAndCaching(MockTimer::kApr_5_2010_ms, 300 * Timer::kSecondMs);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.html"), headers, kContent);

  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.html", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kContent, text);
  // One lookup for ajax metadata, one for the HTTP response and one for the
  // property cache entry. None are found.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  ClearStats();
  // Fetch again from cache. It has the same caching headers.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kContent, text);
  // One hit for the HTTP response. Misses for the property cache entry and the
  // ajax metadata.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());

  // Change the response.
  SetFetchResponse(AbsolutifyUrl("text.html"), headers, "new");

  ClearStats();
  // Fetch again from cache. It has the same caching headers.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  // We continue to serve the previous response since we've cached it.
  EXPECT_EQ(kContent, text);
  // One hit for the HTTP response. Misses for the property cache entry and the
  // ajax metadata.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());


  // Invalidate the cache for some URL other than 'text.html'.
  scoped_ptr<RewriteOptions> custom_options_1(
      server_context()->global_options()->Clone());
  custom_options_1->AddUrlCacheInvalidationEntry(
      AbsolutifyUrl("foo.bar"), timer()->NowMs(), true);
  ProxyUrlNamer url_namer_1;
  url_namer_1.set_options(custom_options_1.get());
  server_context()->set_url_namer(&url_namer_1);

  ClearStats();
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  // We continue to serve the previous response since we've cached it.
  EXPECT_EQ(kContent, text);
  // One hit for the HTTP response. Misses for the property cache entry and the
  // ajax metadata.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());


  // Invalidate the cache.
  scoped_ptr<RewriteOptions> custom_options_2(
      server_context()->global_options()->Clone());
  // Strictness of URL cache invalidation entry (last argument below) does not
  // matter in this test since there is nothing cached in metadata or property
  // caches.
  custom_options_2->AddUrlCacheInvalidationEntry(
      AbsolutifyUrl("text.html"), timer()->NowMs(), true);
  ProxyUrlNamer url_namer_2;
  url_namer_2.set_options(custom_options_2.get());
  server_context()->set_url_namer(&url_namer_2);

  ClearStats();
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  // We get the new response since we've invalidated the cache.
  EXPECT_EQ("new", text);
  // The HTTP response is found in the LRU cache but counts as a miss in the
  // HTTPCache since it has been invalidated. Also, cache misses for the ajax
  // metadata and property cache entry.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());
}

TEST_F(ProxyInterfaceTest, NoImplicitCachingHeadersForHtml) {
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.SetDate(MockTimer::kApr_5_2010_ms);
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.html"), headers, kContent);

  // The first response served by the fetcher does not have implicit caching
  // headers.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.html", true, &text, &response_headers);
  EXPECT_STREQ(NULL, response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kContent, text);
  // Lookups for: (1) ajax metadata (2) HTTP response (3) Property cache.
  // None are found.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  ClearStats();
  // Fetch again. Not found in cache.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.html", true, &text, &response_headers);
  EXPECT_EQ(NULL, response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kContent, text);
  // Lookups for: (1) ajax metadata (2) HTTP response (3) Property cache.
  // None are found.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
}

TEST_F(ProxyInterfaceTest, ModifiedImplicitCachingHeadersForCss) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_implicit_cache_ttl_ms(500 * Timer::kSecondMs);
  server_context()->ComputeSignature(options);

  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  // Do not call ComputeCaching before calling SetFetchResponse because it will
  // add an explicit max-age=300 cache control header. We do not want that
  // header in this test.
  SetFetchResponse(AbsolutifyUrl("text.css"), headers, kContent);

  // The first response served by the fetcher has caching headers.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.css", true, &text, &response_headers);

  GoogleString max_age_500 = "max-age=500";
  GoogleString start_time_plus_500s_string;
  ConvertTimeToString(MockTimer::kApr_5_2010_ms + 500 * Timer::kSecondMs,
                      &start_time_plus_500s_string);

  EXPECT_STREQ(max_age_500,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_500s_string,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kContent, text);
  // One lookup for ajax metadata, one for the HTTP response and one by the css
  // filter which looks up metadata while rewriting. None are found.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  ClearStats();
  // Fetch again from cache. It has the same caching headers.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.css", true, &text, &response_headers);

  EXPECT_STREQ(max_age_500,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_500s_string,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kContent, text);
  // One hit for ajax metadata and one for the HTTP response.
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, lru_cache()->num_misses());
}

TEST_F(ProxyInterfaceTest, EtagsAddedWhenAbsent) {
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  SetDefaultLongCacheHeaders(&kContentTypeText, &headers);
  headers.RemoveAll(HttpAttributes::kEtag);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.txt"), headers, kContent);

  // The first response served by the fetcher has no Etag in the response.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.txt", true, &text, &response_headers);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
  EXPECT_EQ(NULL, response_headers.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ(kContent, text);
  // One lookup for ajax metadata and one for the HTTP response. Neither are
  // found.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  ClearStats();

  // An Etag is added before writing to cache. The next response is served from
  // cache and has an Etag.
  GoogleString text2;
  ResponseHeaders response_headers2;
  FetchFromProxy("text.txt", true, &text2, &response_headers2);
  EXPECT_EQ(HttpStatus::kOK, response_headers2.status_code());
  EXPECT_STREQ(kEtag0, response_headers2.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ(kContent, text2);
  // One lookup for ajax metadata and one for the HTTP response. The metadata is
  // not found but the HTTP response is found.
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  ClearStats();

  // The Etag matches and a 304 is served out.
  GoogleString text3;
  ResponseHeaders response_headers3;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kIfNoneMatch, kEtag0);
  FetchFromProxy("text.txt", request_headers, true, &text3, &response_headers3);
  EXPECT_EQ(HttpStatus::kNotModified, response_headers3.status_code());
  EXPECT_STREQ(NULL, response_headers3.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ("", text3);
  // One lookup for ajax metadata and one for the HTTP response. The metadata is
  // not found but the HTTP response is found.
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
}

TEST_F(ProxyInterfaceTest, EtagMatching) {
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  SetDefaultLongCacheHeaders(&kContentTypeText, &headers);
  headers.Replace(HttpAttributes::kEtag, "etag");
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.txt"), headers, kContent);

  // The first response served by the fetcher has an Etag in the response.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.txt", true, &text, &response_headers);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
  EXPECT_STREQ("etag", response_headers.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ(kContent, text);
  // One lookup for ajax metadata and one for the HTTP response. Neither are
  // found.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());

  ClearStats();
  // The next response is served from cache.
  GoogleString text2;
  ResponseHeaders response_headers2;
  FetchFromProxy("text.txt", true, &text2, &response_headers2);
  EXPECT_EQ(HttpStatus::kOK, response_headers2.status_code());
  EXPECT_STREQ("etag", response_headers2.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ(kContent, text2);
  // One lookup for ajax metadata and one for the HTTP response. The metadata is
  // not found but the HTTP response is found.
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  ClearStats();

  // The Etag matches and a 304 is served out.
  GoogleString text3;
  ResponseHeaders response_headers3;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kIfNoneMatch, "etag");
  FetchFromProxy("text.txt", request_headers, true, &text3, &response_headers3);
  EXPECT_EQ(HttpStatus::kNotModified, response_headers3.status_code());
  EXPECT_STREQ(NULL, response_headers3.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ("", text3);
  // One lookup for ajax metadata and one for the HTTP response. The metadata is
  // not found but the HTTP response is found.
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());

  ClearStats();
  // The Etag doesn't match and the full response is returned.
  GoogleString text4;
  ResponseHeaders response_headers4;
  request_headers.Replace(HttpAttributes::kIfNoneMatch, "mismatch");
  FetchFromProxy("text.txt", request_headers, true, &text4, &response_headers4);
  EXPECT_EQ(HttpStatus::kOK, response_headers4.status_code());
  EXPECT_STREQ("etag", response_headers4.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ(kContent, text4);
  // One lookup for ajax metadata and one for the HTTP response. The metadata is
  // not found but the HTTP response is found.
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
}

TEST_F(ProxyInterfaceTest, LastModifiedMatch) {
  ResponseHeaders headers;
  const char kContent[] = "A very compelling article";
  SetDefaultLongCacheHeaders(&kContentTypeText, &headers);
  headers.SetLastModified(MockTimer::kApr_5_2010_ms);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.txt"), headers, kContent);

  // The first response served by the fetcher has an Etag in the response.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.txt", true, &text, &response_headers);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kLastModified));
  EXPECT_EQ(kContent, text);
  // One lookup for ajax metadata and one for the HTTP response. Neither are
  // found.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());

  ClearStats();
  // The next response is served from cache.
  GoogleString text2;
  ResponseHeaders response_headers2;
  FetchFromProxy("text.txt", true, &text2, &response_headers2);
  EXPECT_EQ(HttpStatus::kOK, response_headers2.status_code());
  EXPECT_STREQ(start_time_string_,
               response_headers2.Lookup1(HttpAttributes::kLastModified));
  EXPECT_EQ(kContent, text2);
  // One lookup for ajax metadata and one for the HTTP response. The metadata is
  // not found but the HTTP response is found.
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());

  ClearStats();
  // The last modified timestamp matches and a 304 is served out.
  GoogleString text3;
  ResponseHeaders response_headers3;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kIfModifiedSince, start_time_string_);
  FetchFromProxy("text.txt", request_headers, true, &text3, &response_headers3);
  EXPECT_EQ(HttpStatus::kNotModified, response_headers3.status_code());
  EXPECT_STREQ(NULL, response_headers3.Lookup1(HttpAttributes::kLastModified));
  EXPECT_EQ("", text3);
  // One lookup for ajax metadata and one for the HTTP response. The metadata is
  // not found but the HTTP response is found.
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());

  ClearStats();
  // The last modified timestamp doesn't match and the full response is
  // returned.
  GoogleString text4;
  ResponseHeaders response_headers4;
  request_headers.Replace(HttpAttributes::kIfModifiedSince,
                          "Fri, 02 Apr 2010 18:51:26 GMT");
  FetchFromProxy("text.txt", request_headers, true, &text4, &response_headers4);
  EXPECT_EQ(HttpStatus::kOK, response_headers4.status_code());
  EXPECT_STREQ(start_time_string_,
               response_headers4.Lookup1(HttpAttributes::kLastModified));
  EXPECT_EQ(kContent, text4);
  // One lookup for ajax metadata and one for the HTTP response. The metadata is
  // not found but the HTTP response is found.`
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
}

TEST_F(ProxyInterfaceTest, AjaxRewritingForCss) {
  ResponseHeaders headers;
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetDate(MockTimer::kApr_5_2010_ms);
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.css"), headers, kCssContent);

  // The first response served by the fetcher and is not rewritten. An ajax
  // rewrite is triggered.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("text.css", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kCssContent, text);
  CheckBackgroundFetch(response_headers, false);
  CheckNumBackgroundFetches(0);
  // One lookup for ajax metadata, one for the HTTP response and one by the css
  // filter which looks up metadata while rewriting. None are found.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  ClearStats();
  // The rewrite is complete and the optimized version is served.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.css", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kMinimizedCssContent, text);
  // One hit for ajax metadata and one for the rewritten HTTP response.
  EXPECT_EQ(2, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, lru_cache()->num_misses());
  CheckNumBackgroundFetches(0);

  ClearStats();
  // Advance close to expiry.
  AdvanceTimeUs(270 * Timer::kSecondUs);
  // The rewrite is complete and the optimized version is served. A freshen is
  // triggered to refresh the original CSS file.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.css", true, &text, &response_headers);

  EXPECT_STREQ("max-age=30",
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ("Mon, 05 Apr 2010 18:55:56 GMT",
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kMinimizedCssContent, text);
  // One hit for ajax metadata, one for the rewritten HTTP response and one for
  // the original HTTP response while freshening.
  EXPECT_EQ(3, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, lru_cache()->num_misses());
  // One background fetch is triggered while freshening.
  CheckNumBackgroundFetches(1);

  // Disable ajax rewriting. We now received the response fetched while
  // freshening. This response has kBackgroundFetchHeader set to 1.
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_in_place_rewriting_enabled(false);
  server_context()->ComputeSignature(options);

  ClearStats();
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.css", true, &text, &response_headers);
  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ("Mon, 05 Apr 2010 19:00:56 GMT",
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ("Mon, 05 Apr 2010 18:55:56 GMT",
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kCssContent, text);
  CheckNumBackgroundFetches(0);
  CheckBackgroundFetch(response_headers, true);
  // Done HTTP cache hit for the original response.
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, lru_cache()->num_misses());
}

TEST_F(ProxyInterfaceTest, NoAjaxRewritingWhenAuthorizationSent) {
  // We should not do ajax rewriting when sending over an authorization
  // header if the original isn't cache-control: public.
  ResponseHeaders headers;
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetDate(MockTimer::kApr_5_2010_ms);
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.css"), headers, kCssContent);

  // The first response served by the fetcher and is not rewritten. An ajax
  // rewrite is triggered.
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kAuthorization, "Paperwork");
  FetchFromProxy("text.css", request_headers, true, &text, &response_headers);
  EXPECT_EQ(kCssContent, text);

  // The second version should still be unoptimized, since original wasn't
  // cacheable.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.css", request_headers, true, &text, &response_headers);
  EXPECT_EQ(kCssContent, text);
}

TEST_F(ProxyInterfaceTest, AjaxRewritingWhenAuthorizationButPublic) {
  // We should do ajax rewriting when sending over an authorization
  // header if the original is cache-control: public.
  ResponseHeaders headers;
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetDate(MockTimer::kApr_5_2010_ms);
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.Add(HttpAttributes::kCacheControl, "public, max-age=400");
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("text.css"), headers, kCssContent);

  // The first response served by the fetcher and is not rewritten. An ajax
  // rewrite is triggered.
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kAuthorization, "Paperwork");
  FetchFromProxy("text.css", request_headers, true, &text, &response_headers);
  EXPECT_EQ(kCssContent, text);

  // The second version should be optimized in this case.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("text.css", request_headers, true, &text, &response_headers);
  EXPECT_EQ(kMinimizedCssContent, text);
}

TEST_F(ProxyInterfaceTest, AjaxRewritingDisabledByGlobalDisable) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_enabled(RewriteOptions::kEnabledOff);
  server_context()->ComputeSignature(options);

  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCssContent,
                                kHtmlCacheTimeSec * 2);
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("a.css", true, &text, &response_headers);
  // First fetch will not get rewritten no matter what.
  EXPECT_STREQ(kCssContent, text);

  // Second fetch would get minified if ajax rewriting were on; but
  // it got disabled by the global toggle.
  text.clear();
  FetchFromProxy("a.css", true, &text, &response_headers);
  EXPECT_STREQ(kCssContent, text);
}

TEST_F(ProxyInterfaceTest, AjaxRewritingSkippedIfBlacklisted) {
  ResponseHeaders headers;
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetDate(MockTimer::kApr_5_2010_ms);
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("blacklist.css"), headers, kCssContent);

  // The first response is served by the fetcher. Since the url is blacklisted,
  // no ajax rewriting happens.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("blacklist.css", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kCssContent, text);
  // Since no ajax rewriting happens, there is only a single cache lookup for
  // the resource.
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());
  EXPECT_EQ(0, lru_cache()->num_hits());

  ClearStats();
  // The same thing happens on the second request.
  text.clear();
  response_headers.Clear();
  FetchFromProxy("blacklist.css", true, &text, &response_headers);

  EXPECT_STREQ(max_age_300_,
               response_headers.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_300s_string_,
               response_headers.Lookup1(HttpAttributes::kExpires));
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_EQ(kCssContent, text);
  // The resource is found in cache this time.
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, http_cache()->cache_misses()->Get());
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
  EXPECT_EQ(1, lru_cache()->num_hits());
}

TEST_F(ProxyInterfaceTest, AjaxRewritingBlacklistReject) {
  // Makes sure that we honor reject_blacklisted() when ajax rewriting may
  // have normally happened.
  RejectBlacklisted();

  ResponseHeaders headers;
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeCss.mime_type());
  headers.SetDate(MockTimer::kApr_5_2010_ms);
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("blacklistCoffee.css"), headers, kCssContent);
  SetFetchResponse(AbsolutifyUrl("tea.css"), headers, kCssContent);

  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxy("blacklistCoffee.css", true, &text, &response_headers);
  EXPECT_EQ(HttpStatus::kImATeapot, response_headers.status_code());
  EXPECT_TRUE(text.empty());

  // Non-blacklisted stuff works OK.
  FetchFromProxy("tea.css", true, &text, &response_headers);
  EXPECT_EQ(HttpStatus::kOK, response_headers.status_code());
  EXPECT_EQ(kCssContent, text);
}

TEST_F(ProxyInterfaceTest, EatCookiesOnReconstructFailure) {
  // Make sure we don't pass through a Set-Cookie[2] when reconstructing
  // a resource on demand fails.
  GoogleString abs_path = AbsolutifyUrl("a.css");
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &response_headers);
  response_headers.Add(HttpAttributes::kSetCookie, "a cookie");
  response_headers.Add(HttpAttributes::kSetCookie2, "a weird old-time cookie");
  response_headers.ComputeCaching();
  SetFetchResponse(abs_path, response_headers, "broken_css{");

  ResponseHeaders out_response_headers;
  GoogleString text;
  FetchFromProxy(Encode(kTestDomain, "cf", "0", "a.css", "css"), true,
                 &text, &out_response_headers);
  EXPECT_EQ(NULL, out_response_headers.Lookup1(HttpAttributes::kSetCookie));
  EXPECT_EQ(NULL, out_response_headers.Lookup1(HttpAttributes::kSetCookie2));
}

TEST_F(ProxyInterfaceTest, RewriteHtml) {
  GoogleString text;
  ResponseHeaders headers;

  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->SetRewriteLevel(RewriteOptions::kPassThrough);
  options->EnableFilter(RewriteOptions::kRewriteCss);
  server_context()->ComputeSignature(options);

  headers.Add(HttpAttributes::kEtag, "something");
  headers.SetDateAndCaching(MockTimer::kApr_5_2010_ms,
                            kHtmlCacheTimeSec * 2 * Timer::kSecondMs);
  headers.SetLastModified(MockTimer::kApr_5_2010_ms);
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl(kPageUrl), headers, CssLinkHref("a.css"));

  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCssContent,
                                kHtmlCacheTimeSec * 2);

  text.clear();
  headers.Clear();
  FetchFromProxy(kPageUrl, true, &text, &headers);
  CheckBackgroundFetch(headers, false);
  CheckNumBackgroundFetches(1);
  CheckHeaders(headers, kContentTypeHtml);
  EXPECT_EQ(CssLinkHref(Encode(kTestDomain, "cf", "0", "a.css", "css")), text);
  headers.ComputeCaching();
  EXPECT_LE(start_time_ms_ + kHtmlCacheTimeSec * Timer::kSecondMs,
            headers.CacheExpirationTimeMs());
  EXPECT_EQ(NULL, headers.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ(NULL, headers.Lookup1(HttpAttributes::kLastModified));
  EXPECT_STREQ("cf", AppliedRewriterStringFromLog());

  // Fetch the rewritten resource as well.
  text.clear();
  headers.Clear();
  ClearStats();
  FetchFromProxy(Encode(kTestDomain, "cf", "0", "a.css", "css"), true,
                 &text, &headers);
  CheckHeaders(headers, kContentTypeCss);
  // Note that the fetch for the original resource was triggered as a result of
  // the initial HTML request. Hence, its headers indicate that it is a
  // background request
  // This response has kBackgroundFetchHeader set to 1 since a fetch was
  // triggered for it in the background while rewriting the original html.
  CheckBackgroundFetch(headers, true);
  CheckNumBackgroundFetches(0);
  headers.ComputeCaching();
  EXPECT_LE(start_time_ms_ + Timer::kYearMs, headers.CacheExpirationTimeMs());
  EXPECT_EQ(kMinimizedCssContent, text);
}

TEST_F(ProxyInterfaceTest, LogChainedResourceRewrites) {
  GoogleString text;
  ResponseHeaders headers;

  SetResponseWithDefaultHeaders("1.js", kContentTypeJavascript, "var wxyz=1;",
                                kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders("2.js", kContentTypeJavascript, "var abcd=2;",
                                kHtmlCacheTimeSec * 2);

  GoogleString combined_js_url = Encode(
      kTestDomain, "jc", "0",
      "1.js.pagespeed.jm.0.jsX2.js.pagespeed.jm.0.js", "js");
  combined_js_url[combined_js_url.find('X')] = '+';

  FetchFromProxy(combined_js_url, true, &text, &headers);
  EXPECT_STREQ("jc,jm", AppliedRewriterStringFromLog());
}

TEST_F(ProxyInterfaceTest, FlushHugeHtml) {
  // Test the forced flushing of HTML controlled by flush_buffer_limit_bytes().
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_flush_buffer_limit_bytes(8);  // 2 self-closing tags ("<p/>")
  options->set_flush_html(true);
  server_context()->ComputeSignature(options);

  SetResponseWithDefaultHeaders("page.html", kContentTypeHtml,
                                "<a/><b/><c/><d/><e/><f/><g/><h/>",
                                kHtmlCacheTimeSec * 2);

  GoogleString out;
  FetchFromProxyLoggingFlushes("page.html", true /*success*/, &out);
  EXPECT_EQ(
      "<a/><b/>|Flush|<c/><d/>|Flush|<e/><f/>|Flush|<g/><h/>|Flush||Flush|",
      out);

  // Now tell to flush after 3 self-closing tags.
  options->ClearSignatureForTesting();
  options->set_flush_buffer_limit_bytes(12);  // 3 self-closing tags
  server_context()->ComputeSignature(options);

  FetchFromProxyLoggingFlushes("page.html", true /*success*/, &out);
  EXPECT_EQ(
      "<a/><b/><c/>|Flush|<d/><e/><f/>|Flush|<g/><h/>|Flush|", out);

  // And now with 2.5. This means we will flush 2 (as that many are complete),
  // then 5, and 7.
  options->ClearSignatureForTesting();
  options->set_flush_buffer_limit_bytes(10);
  server_context()->ComputeSignature(options);

  FetchFromProxyLoggingFlushes("page.html", true /*success*/, &out);
  EXPECT_EQ(
      "<a/><b/>|Flush|<c/><d/><e/>|Flush|<f/><g/>|Flush|<h/>|Flush|", out);

  // Now 9 bytes, e.g. 2 1/4 of a self-closing tag. Looks almost the same as
  // every 2 self-closing tags (8 bytes), but we don't get an extra flush
  // at the end.
  options->ClearSignatureForTesting();
  options->set_flush_buffer_limit_bytes(9);
  server_context()->ComputeSignature(options);
  FetchFromProxyLoggingFlushes("page.html", true /*success*/, &out);
  EXPECT_EQ(
      "<a/><b/>|Flush|<c/><d/>|Flush|<e/><f/>|Flush|<g/><h/>|Flush|",
      out);
}

TEST_F(ProxyInterfaceTest, DontRewriteDisallowedHtml) {
  // Blacklisted URL should not be rewritten.
  SetResponseWithDefaultHeaders("blacklist.html", kContentTypeHtml,
                                CssLinkHref("a.css"), kHtmlCacheTimeSec * 2),
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCssContent,
                                kHtmlCacheTimeSec * 2);

  GoogleString text;
  ResponseHeaders headers;
  FetchFromProxy("blacklist.html", true, &text, &headers);
  CheckHeaders(headers, kContentTypeHtml);
  EXPECT_EQ(CssLinkHref("a.css"), text);
}

TEST_F(ProxyInterfaceTest, DontRewriteDisallowedHtmlRejectMode) {
  // If we're in reject_blacklisted mode, we should just respond with the
  // configured status.
  RejectBlacklisted();
  SetResponseWithDefaultHeaders("blacklistCoffee.html", kContentTypeHtml,
                                CssLinkHref("a.css"), kHtmlCacheTimeSec * 2),
  SetResponseWithDefaultHeaders("tea.html", kContentTypeHtml,
                                "tasty", kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCssContent,
                                kHtmlCacheTimeSec * 2);

  GoogleString text;
  ResponseHeaders headers;
  FetchFromProxy("blacklistCoffee.html", true, &text, &headers);
  EXPECT_EQ(HttpStatus::kImATeapot, headers.status_code());
  EXPECT_TRUE(text.empty());

  // Fetching non-blacklisted one works fine.
  FetchFromProxy("tea.html", true, &text, &headers);
  EXPECT_EQ(HttpStatus::kOK, headers.status_code());
  EXPECT_STREQ("tasty", text);
}

TEST_F(ProxyInterfaceTest, DontRewriteMislabeledAsHtml) {
  // Make sure we don't rewrite things that claim to be HTML, but aren't.
  GoogleString text;
  ResponseHeaders headers;

  SetResponseWithDefaultHeaders("page.js", kContentTypeHtml,
                                StrCat("//", CssLinkHref("a.css")),
                                kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCssContent,
                                kHtmlCacheTimeSec * 2);

  FetchFromProxy("page.js", true, &text, &headers);
  CheckHeaders(headers, kContentTypeHtml);
  EXPECT_EQ(StrCat("//", CssLinkHref("a.css")), text);
}

TEST_F(ProxyInterfaceTest, ReconstructResource) {
  GoogleString text;
  ResponseHeaders headers;

  // Fetching of a rewritten resource we did not just create
  // after an HTML rewrite.
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCssContent,
                                kHtmlCacheTimeSec * 2);
  FetchFromProxy(Encode("", "cf", "0", "a.css", "css"), true, &text, &headers);
  CheckHeaders(headers, kContentTypeCss);
  headers.ComputeCaching();
  CheckBackgroundFetch(headers, false);
  EXPECT_LE(start_time_ms_ + Timer::kYearMs, headers.CacheExpirationTimeMs());
  EXPECT_EQ(kMinimizedCssContent, text);
  EXPECT_STREQ("cf", AppliedRewriterStringFromLog());
}

TEST_F(ProxyInterfaceTest, ReconstructResourceCustomOptions) {
  const char kCssWithEmbeddedImage[] = "*{background-image:url(%s)}";
  const char kBackgroundImage[] = "1.png";

  GoogleString text;
  ResponseHeaders headers;

  // We're not going to image-compress so we don't need our mock image
  // to really be an image.
  SetResponseWithDefaultHeaders(kBackgroundImage, kContentTypePng, "image",
                                kHtmlCacheTimeSec * 2);
  GoogleString orig_css = StringPrintf(kCssWithEmbeddedImage, kBackgroundImage);
  SetResponseWithDefaultHeaders("embedded.css", kContentTypeCss,
                                orig_css, kHtmlCacheTimeSec * 2);

  // By default, cache extension is off in the default options.
  server_context()->global_options()->SetDefaultRewriteLevel(
      RewriteOptions::kPassThrough);
  ASSERT_FALSE(options()->Enabled(RewriteOptions::kExtendCacheCss));
  ASSERT_FALSE(options()->Enabled(RewriteOptions::kExtendCacheImages));
  ASSERT_FALSE(options()->Enabled(RewriteOptions::kExtendCacheScripts));
  ASSERT_FALSE(options()->Enabled(RewriteOptions::kExtendCachePdfs));
  ASSERT_EQ(RewriteOptions::kPassThrough, options()->level());

  // Because cache-extension was turned off, the image in the CSS file
  // will not be changed.
  FetchFromProxy("I.embedded.css.pagespeed.cf.0.css", true, &text, &headers);
  EXPECT_EQ(orig_css, text);

  // Now turn on cache-extension for custom options.  Invalidate cache entries
  // up to and including the current timestamp and advance by 1ms, otherwise
  // the previously stored embedded.css.pagespeed.cf.0.css will get re-used.
  scoped_ptr<RewriteOptions> custom_options(factory()->NewRewriteOptions());
  custom_options->EnableFilter(RewriteOptions::kExtendCacheCss);
  custom_options->EnableFilter(RewriteOptions::kExtendCacheImages);
  custom_options->EnableFilter(RewriteOptions::kExtendCacheScripts);
  custom_options->EnableFilter(RewriteOptions::kExtendCachePdfs);
  custom_options->set_cache_invalidation_timestamp(timer()->NowMs());
  AdvanceTimeUs(Timer::kMsUs);

  // Inject the custom options into the flow via a custom URL namer.
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  // Use EncodeNormal because it matches the logic used by ProxyUrlNamer.
  const GoogleString kExtendedBackgroundImage =
      EncodeNormal(kTestDomain, "ce", "0", kBackgroundImage, "png");

  // Now when we fetch the options, we'll find the image in the CSS
  // cache-extended.
  text.clear();
  FetchFromProxy("I.embedded.css.pagespeed.cf.0.css", true, &text, &headers);
  EXPECT_EQ(StringPrintf(kCssWithEmbeddedImage,
                         kExtendedBackgroundImage.c_str()),
            text);
}

TEST_F(ProxyInterfaceTest, MinResourceTimeZero) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->SetRewriteLevel(RewriteOptions::kPassThrough);
  options->EnableFilter(RewriteOptions::kRewriteCss);
  options->set_min_resource_cache_time_to_rewrite_ms(
      kHtmlCacheTimeSec * Timer::kSecondMs);
  server_context()->ComputeSignature(options);

  SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml,
                                CssLinkHref("a.css"), kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCssContent,
                                kHtmlCacheTimeSec * 2);

  GoogleString text;
  ResponseHeaders headers;
  FetchFromProxy(kPageUrl, true, &text, &headers);
  EXPECT_EQ(CssLinkHref(Encode(kTestDomain, "cf", "0", "a.css", "css")), text);
}

TEST_F(ProxyInterfaceTest, MinResourceTimeLarge) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->SetRewriteLevel(RewriteOptions::kPassThrough);
  options->EnableFilter(RewriteOptions::kRewriteCss);
  options->set_min_resource_cache_time_to_rewrite_ms(
      4 * kHtmlCacheTimeSec * Timer::kSecondMs);
  server_context()->ComputeSignature(options);

  SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml,
                                CssLinkHref("a.css"), kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, kCssContent,
                                kHtmlCacheTimeSec * 2);

  GoogleString text;
  ResponseHeaders headers;
  FetchFromProxy(kPageUrl, true, &text, &headers);
  EXPECT_EQ(CssLinkHref("a.css"), text);
}

TEST_F(ProxyInterfaceTest, CacheRequests) {
  ResponseHeaders html_headers;
  DefaultResponseHeaders(kContentTypeHtml, kHtmlCacheTimeSec, &html_headers);
  SetFetchResponse(AbsolutifyUrl(kPageUrl), html_headers, "1");
  ResponseHeaders resource_headers;
  DefaultResponseHeaders(kContentTypeCss, kHtmlCacheTimeSec, &resource_headers);
  SetFetchResponse(AbsolutifyUrl("style.css"), resource_headers, "a");

  GoogleString text;
  ResponseHeaders actual_headers;
  FetchFromProxy(kPageUrl, true, &text, &actual_headers);
  EXPECT_EQ("1", text);
  text.clear();
  FetchFromProxy("style.css", true, &text, &actual_headers);
  EXPECT_EQ("a", text);

  SetFetchResponse(AbsolutifyUrl(kPageUrl), html_headers, "2");
  SetFetchResponse(AbsolutifyUrl("style.css"), resource_headers, "b");

  // Original response is still cached in both cases, so we do not
  // fetch the new values.
  text.clear();
  FetchFromProxy(kPageUrl, true, &text, &actual_headers);
  EXPECT_EQ("1", text);
  text.clear();
  FetchFromProxy("style.css", true, &text, &actual_headers);
  EXPECT_EQ("a", text);
}

// Verifies that we proxy uncacheable resources, but do not insert them in the
// cache.
TEST_F(ProxyInterfaceTest, UncacheableResourcesNotCachedOnProxy) {
  ResponseHeaders resource_headers;
  DefaultResponseHeaders(kContentTypeCss, kHtmlCacheTimeSec, &resource_headers);
  resource_headers.SetDateAndCaching(http_cache()->timer()->NowMs(),
                                     300 * Timer::kSecondMs, ", private");
  resource_headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("style.css"), resource_headers, "a");

  ProxyUrlNamer url_namer;
  server_context()->set_url_namer(&url_namer);
  ResponseHeaders out_headers;
  GoogleString out_text;

  // We should not cache while fetching via kProxyHost.
  FetchFromProxy(
      StrCat("http://", ProxyUrlNamer::kProxyHost,
             "/test.com/test.com/style.css"),
      true, &out_text, &out_headers);
  EXPECT_EQ("a", out_text);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());  // mapping, input resource
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());  // input resource
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());

  // We should likewise not cache while fetching on the origin domain.
  out_text.clear();
  ClearStats();
  FetchFromProxy("style.css", true, &out_text, &out_headers);
  EXPECT_EQ("a", out_text);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());  // mapping, input resource
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());  // input resource
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());

  // Since the original response is not cached, we should pick up changes in the
  // input resource immediately.
  SetFetchResponse(AbsolutifyUrl("style.css"), resource_headers, "b");
  out_text.clear();
  ClearStats();
  FetchFromProxy("style.css", true, &out_text, &out_headers);
  EXPECT_EQ("b", out_text);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(2, lru_cache()->num_misses());  // mapping, input resource
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());  // input resource
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, http_cache()->cache_inserts()->Get());
}

// Verifies that we retrieve and serve uncacheable resources, but do not insert
// them in the cache.
TEST_F(ProxyInterfaceTest, UncacheableResourcesNotCachedOnResourceFetch) {
  ResponseHeaders resource_headers;
  DefaultResponseHeaders(kContentTypeCss, kHtmlCacheTimeSec, &resource_headers);
  resource_headers.SetDateAndCaching(http_cache()->timer()->NowMs(),
                                     300 * Timer::kSecondMs, ", private");
  resource_headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("style.css"), resource_headers, "a");

  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->SetRewriteLevel(RewriteOptions::kPassThrough);
  options->EnableFilter(RewriteOptions::kRewriteCss);
  server_context()->ComputeSignature(options);

  ResponseHeaders out_headers;
  GoogleString out_text;

  // cf is not on-the-fly, and we can reconstruct it while keeping it private.
  FetchFromProxy(Encode(kTestDomain, "cf", "0", "style.css", "css"),
                 true, &out_text, &out_headers);
  EXPECT_TRUE(out_headers.HasValue(HttpAttributes::kCacheControl, "private"));
  EXPECT_EQ("a", out_text);
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(4, lru_cache()->num_misses());  // 2x output, metadata, input
  EXPECT_EQ(3, http_cache()->cache_misses()->Get());  // 2x output, input
  EXPECT_EQ(2, lru_cache()->num_inserts());  // mapping, uncacheable memo
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());  // uncacheable memo

  out_text.clear();
  ClearStats();
  // ce is on-the-fly, and we can recover even though style.css is private.
  FetchFromProxy(Encode(kTestDomain, "ce", "0", "style.css", "css"),
                 true, &out_text, &out_headers);
  EXPECT_TRUE(out_headers.HasValue(HttpAttributes::kCacheControl, "private"));
  EXPECT_EQ("a", out_text);
  EXPECT_EQ(1, lru_cache()->num_hits());  // input uncacheable memo
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());  // input uncacheable memo
  EXPECT_EQ(1, lru_cache()->num_inserts());  // mapping
  EXPECT_EQ(1, lru_cache()->num_identical_reinserts());  // uncacheable memo
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());  // uncacheable memo

  out_text.clear();
  ClearStats();
  FetchFromProxy(Encode(kTestDomain, "ce", "0", "style.css", "css"),
                 true, &out_text, &out_headers);
  EXPECT_TRUE(out_headers.HasValue(HttpAttributes::kCacheControl, "private"));
  EXPECT_EQ("a", out_text);
  EXPECT_EQ(1, lru_cache()->num_hits());  // uncacheable memo
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());  // uncacheable memo
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(2, lru_cache()->num_identical_reinserts())
      << "uncacheable memo, metadata";
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());  // uncacheable memo

  // Since the original response is not cached, we should pick up changes in the
  // input resource immediately.
  SetFetchResponse(AbsolutifyUrl("style.css"), resource_headers, "b");
  out_text.clear();
  ClearStats();
  FetchFromProxy(Encode(kTestDomain, "ce", "0", "style.css", "css"),
                 true, &out_text, &out_headers);
  EXPECT_TRUE(out_headers.HasValue(HttpAttributes::kCacheControl, "private"));
  EXPECT_EQ("b", out_text);
  EXPECT_EQ(1, lru_cache()->num_hits());  // uncacheable memo
  EXPECT_EQ(0, http_cache()->cache_hits()->Get());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(1, http_cache()->cache_misses()->Get());  // uncacheable memo
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(2, lru_cache()->num_identical_reinserts())
      << "uncacheable memo, metadata";
  EXPECT_EQ(1, http_cache()->cache_inserts()->Get());  // uncacheable memo
}

// No matter what options->respect_vary() is set to we will respect HTML Vary
// headers.
TEST_F(ProxyInterfaceTest, NoCacheVaryHtml) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_respect_vary(false);
  server_context()->ComputeSignature(options);

  ResponseHeaders html_headers;
  DefaultResponseHeaders(kContentTypeHtml, kHtmlCacheTimeSec, &html_headers);
  html_headers.Add(HttpAttributes::kVary, HttpAttributes::kUserAgent);
  html_headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl(kPageUrl), html_headers, "1");
  ResponseHeaders resource_headers;
  DefaultResponseHeaders(kContentTypeCss, kHtmlCacheTimeSec, &resource_headers);
  resource_headers.Add(HttpAttributes::kVary, HttpAttributes::kUserAgent);
  resource_headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("style.css"), resource_headers, "a");

  GoogleString text;
  ResponseHeaders actual_headers;
  FetchFromProxy(kPageUrl, true, &text, &actual_headers);
  EXPECT_EQ("1", text);
  text.clear();
  FetchFromProxy("style.css", true, &text, &actual_headers);
  EXPECT_EQ("a", text);

  SetFetchResponse(AbsolutifyUrl(kPageUrl), html_headers, "2");
  SetFetchResponse(AbsolutifyUrl("style.css"), resource_headers, "b");

  // HTML was not cached because of Vary: User-Agent header.
  // So we do fetch the new value.
  text.clear();
  FetchFromProxy(kPageUrl, true, &text, &actual_headers);
  EXPECT_EQ("2", text);
  // Resource was cached because we have respect_vary == false.
  // So we serve the old value.
  text.clear();
  FetchFromProxy("style.css", true, &text, &actual_headers);
  EXPECT_EQ("a", text);
}

// Test https HTML responses are never cached, while https resources are cached.
TEST_F(ProxyInterfaceTest, NoCacheHttpsHtml) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_respect_vary(false);
  server_context()->ComputeSignature(options);
  http_cache()->set_disable_html_caching_on_https(true);

  ResponseHeaders html_headers;
  DefaultResponseHeaders(kContentTypeHtml, kHtmlCacheTimeSec, &html_headers);
  html_headers.ComputeCaching();
  SetFetchResponse(kHttpsPageUrl, html_headers, "1");
  ResponseHeaders resource_headers;
  DefaultResponseHeaders(kContentTypeCss, kHtmlCacheTimeSec, &resource_headers);
  resource_headers.ComputeCaching();
  SetFetchResponse(kHttpsCssUrl, resource_headers, "a");

  GoogleString text;
  ResponseHeaders actual_headers;
  FetchFromProxy(kHttpsPageUrl, true, &text, &actual_headers);
  EXPECT_EQ("1", text);
  text.clear();
  FetchFromProxy(kHttpsCssUrl, true, &text, &actual_headers);
  EXPECT_EQ("a", text);

  SetFetchResponse(kHttpsPageUrl, html_headers, "2");
  SetFetchResponse(kHttpsCssUrl, resource_headers, "b");

  ClearStats();
  // HTML was not cached because it was via https. So we do fetch the new value.
  text.clear();
  FetchFromProxy(kHttpsPageUrl, true, &text, &actual_headers);
  EXPECT_EQ("2", text);
  EXPECT_EQ(0, lru_cache()->num_hits());
  // Resource was cached, so we serve the old value.
  text.clear();
  FetchFromProxy(kHttpsCssUrl, true, &text, &actual_headers);
  EXPECT_EQ("a", text);
  EXPECT_EQ(1, http_cache()->cache_hits()->Get());
}

// Respect Vary for resources if options tell us to.
TEST_F(ProxyInterfaceTest, NoCacheVaryAll) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_respect_vary(true);
  server_context()->ComputeSignature(options);

  ResponseHeaders html_headers;
  DefaultResponseHeaders(kContentTypeHtml, kHtmlCacheTimeSec, &html_headers);
  html_headers.Add(HttpAttributes::kVary, HttpAttributes::kUserAgent);
  html_headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl(kPageUrl), html_headers, "1");
  ResponseHeaders resource_headers;
  DefaultResponseHeaders(kContentTypeCss, kHtmlCacheTimeSec, &resource_headers);
  resource_headers.Add(HttpAttributes::kVary, HttpAttributes::kUserAgent);
  resource_headers.ComputeCaching();
  SetFetchResponse(AbsolutifyUrl("style.css"), resource_headers, "a");

  GoogleString text;
  ResponseHeaders actual_headers;
  FetchFromProxy(kPageUrl, true, &text, &actual_headers);
  EXPECT_EQ("1", text);
  text.clear();
  FetchFromProxy("style.css", true, &text, &actual_headers);
  EXPECT_EQ("a", text);

  SetFetchResponse(AbsolutifyUrl(kPageUrl), html_headers, "2");
  SetFetchResponse(AbsolutifyUrl("style.css"), resource_headers, "b");

  // Original response was not cached in either case, so we do fetch the
  // new value.
  text.clear();
  FetchFromProxy(kPageUrl, true, &text, &actual_headers);
  EXPECT_EQ("2", text);
  text.clear();
  FetchFromProxy("style.css", true, &text, &actual_headers);
  EXPECT_EQ("b", text);
}

TEST_F(ProxyInterfaceTest, Blacklist) {
  const char content[] =
      "<html>\n"
      "  <head/>\n"
      "  <body>\n"
      "    <script src='tiny_mce.js'></script>\n"
      "  </body>\n"
      "</html>\n";
  SetResponseWithDefaultHeaders("tiny_mce.js", kContentTypeJavascript, "", 100);
  ValidateNoChanges("blacklist", content);

  SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml, content, 0);
  GoogleString text_out;
  ResponseHeaders headers_out;
  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_STREQ(content, text_out);
}

TEST_F(ProxyInterfaceTest, RepairMismappedResource) {
  // Teach the mock fetcher to serve origin content for
  // "http://test.com/foo.js".
  const char kContent[] = "function f() {alert('foo');}";
  SetResponseWithDefaultHeaders("foo.js", kContentTypeHtml, kContent,
                                kHtmlCacheTimeSec * 2);

  // Set up a Mock Namer that will mutate output resources to
  // be served on proxy_host.com, encoding the origin URL.
  ProxyUrlNamer url_namer;
  ResponseHeaders headers;
  GoogleString text;
  server_context()->set_url_namer(&url_namer);

  // Now fetch the origin content.  This will simply hit the
  // mock fetcher and always worked.
  FetchFromProxy("foo.js", true, &text, &headers);
  EXPECT_EQ(kContent, text);

  // Now make a weird URL encoding of the origin resource using the
  // proxy host.  This may happen via javascript that detects its
  // own path and initiates a 'load()' of another js file from the
  // same path.  In this variant, the resource is served from the
  // "source domain", so it is automatically whitelisted.
  text.clear();
  FetchFromProxy(
      StrCat("http://", ProxyUrlNamer::kProxyHost, "/test.com/test.com/foo.js"),
      true, &text, &headers);
  EXPECT_EQ(kContent, text);

  // In the next case, the resource is served from a different domain.  This
  // is an open-proxy vulnerability and thus should fail.
  text.clear();
  url_namer.set_authorized(false);
  FetchFromProxy(
      StrCat("http://", ProxyUrlNamer::kProxyHost, "/test.com/evil.com/foo.js"),
      false, &text, &headers);
}

TEST_F(ProxyInterfaceTest, CrossDomainHeaders) {
  // If we're serving content from test.com via kProxyHost URL, we need to make
  // sure that cookies are not propagated, as evil.com could also be potentially
  // proxied via kProxyHost.
  const char kText[] = "* { pretty; }";

  ResponseHeaders orig_headers;
  DefaultResponseHeaders(kContentTypeCss, 100, &orig_headers);
  orig_headers.Add(HttpAttributes::kSetCookie, "tasty");
  SetFetchResponse("http://test.com/file.css", orig_headers, kText);

  ProxyUrlNamer url_namer;
  server_context()->set_url_namer(&url_namer);
  ResponseHeaders out_headers;
  GoogleString out_text;
  FetchFromProxy(
      StrCat("http://", ProxyUrlNamer::kProxyHost,
             "/test.com/test.com/file.css"),
      true, &out_text, &out_headers);
  EXPECT_STREQ(kText, out_text);
  EXPECT_STREQ(NULL, out_headers.Lookup1(HttpAttributes::kSetCookie));
}

TEST_F(ProxyInterfaceTest, CrossDomainRedirectIfBlacklisted) {
  ProxyUrlNamer url_namer;
  server_context()->set_url_namer(&url_namer);
  ResponseHeaders out_headers;
  GoogleString out_text;
  FetchFromProxy(
      StrCat("http://", ProxyUrlNamer::kProxyHost,
             "/test.com/test1.com/blacklist.css"),
      false, &out_text, &out_headers);
  EXPECT_STREQ("", out_text);
  EXPECT_EQ(HttpStatus::kFound, out_headers.status_code());
  EXPECT_STREQ("http://test1.com/blacklist.css",
               out_headers.Lookup1(HttpAttributes::kLocation));
}

TEST_F(ProxyInterfaceTest, CrossDomainAuthorization) {
  // If we're serving content from evil.com via kProxyHostUrl, we need to make
  // sure we don't propagate through any (non-proxy) authorization headers, as
  // they may have been cached from good.com (as both would look like
  // kProxyHost to the browser).
  ReflectingTestFetcher reflect;
  server_context()->set_default_system_fetcher(&reflect);

  ProxyUrlNamer url_namer;
  server_context()->set_url_namer(&url_namer);

  RequestHeaders request_headers;
  request_headers.Add("Was", "Here");
  request_headers.Add(HttpAttributes::kAuthorization, "Secret");
  request_headers.Add(HttpAttributes::kProxyAuthorization, "OurSecret");

  ResponseHeaders out_headers;
  GoogleString out_text;
  // Using .txt here so we don't try any AJAX rewriting.
  FetchFromProxy(StrCat("http://", ProxyUrlNamer::kProxyHost,
                        "/test.com/test.com/file.txt"),
                 request_headers,  true, &out_text, &out_headers);
  EXPECT_STREQ("Here", out_headers.Lookup1("Was"));
  EXPECT_FALSE(out_headers.Has(HttpAttributes::kAuthorization));
  EXPECT_FALSE(out_headers.Has(HttpAttributes::kProxyAuthorization));
  mock_scheduler()->AwaitQuiescence();
}

TEST_F(ProxyInterfaceTest, CrossDomainHeadersWithUncacheableResourceOnProxy) {
  // Check that we do not propagate cookies from test.com via kProxyHost URL,
  // as in CrossDomainHeaders above.  Also check that we do propagate cache
  // control.
  const char kText[] = "* { pretty; }";

  ResponseHeaders orig_headers;
  DefaultResponseHeaders(kContentTypeCss, 100, &orig_headers);
  orig_headers.Add(HttpAttributes::kSetCookie, "tasty");
  orig_headers.SetDateAndCaching(http_cache()->timer()->NowMs(),
                                 400 * Timer::kSecondMs, ", private");
  orig_headers.ComputeCaching();
  SetFetchResponse("http://test.com/file.css", orig_headers, kText);

  ProxyUrlNamer url_namer;
  server_context()->set_url_namer(&url_namer);
  ResponseHeaders out_headers;
  GoogleString out_text;
  FetchFromProxy(
      StrCat("http://", ProxyUrlNamer::kProxyHost,
             "/test.com/test.com/file.css"),
      true, &out_text, &out_headers);

  // Check that we ate the cookies.
  EXPECT_STREQ(kText, out_text);
  ConstStringStarVector values;
  out_headers.Lookup(HttpAttributes::kSetCookie, &values);
  EXPECT_EQ(0, values.size());

  // Check that the resource Cache-Control has been preserved.
  values.clear();
  out_headers.Lookup(HttpAttributes::kCacheControl, &values);
  ASSERT_EQ(2, values.size());
  EXPECT_STREQ("max-age=400", *values[0]);
  EXPECT_STREQ("private", *values[1]);
}

TEST_F(ProxyInterfaceTest, CrossDomainHeadersWithUncacheableResourceOnFetch) {
  // Check that we do not propagate cookies from test.com via a resource fetch,
  // as in CrossDomainHeaders above.  Also check that we do propagate cache
  // control, and that we run the filter specified in the resource fetch URL.
  // Note that the running of filters at present can only happen if
  // the filter is on the-fly.
  const char kText[] = "* { pretty; }";

  ResponseHeaders orig_headers;
  DefaultResponseHeaders(kContentTypeCss, 100, &orig_headers);
  orig_headers.Add(HttpAttributes::kSetCookie, "tasty");
  orig_headers.SetDateAndCaching(http_cache()->timer()->NowMs(),
                                 400 * Timer::kSecondMs, ", private");
  orig_headers.ComputeCaching();
  SetFetchResponse("http://test.com/file.css", orig_headers, kText);

  ProxyUrlNamer url_namer;
  server_context()->set_url_namer(&url_namer);
  ResponseHeaders out_headers;
  GoogleString out_text;
  FetchFromProxy(Encode(kTestDomain, "ce", "0", "file.css", "css"),
                 true, &out_text, &out_headers);

  // Check that we passed through the CSS.
  EXPECT_STREQ(kText, out_text);
  // Check that we ate the cookies.
  ConstStringStarVector values;
  out_headers.Lookup(HttpAttributes::kSetCookie, &values);
  EXPECT_EQ(0, values.size());

  // Check that the resource Cache-Control has been preserved.
  // max-age actually gets smaller, though, since this also triggers
  // a rewrite failure.
  values.clear();
  out_headers.Lookup(HttpAttributes::kCacheControl, &values);
  ASSERT_EQ(2, values.size());
  EXPECT_STREQ("max-age=300", *values[0]);
  EXPECT_STREQ("private", *values[1]);
}

TEST_F(ProxyInterfaceTest, CrossDomainHeadersWithUncacheableResourceOnFetch2) {
  // Variant of the above with a non-on-the-fly filter.
  const char kText[] = "* { pretty; }";

  ResponseHeaders orig_headers;
  DefaultResponseHeaders(kContentTypeCss, 100, &orig_headers);
  orig_headers.Add(HttpAttributes::kSetCookie, "tasty");
  orig_headers.SetDateAndCaching(http_cache()->timer()->NowMs(),
                                 400 * Timer::kSecondMs, ", private");
  orig_headers.ComputeCaching();
  SetFetchResponse("http://test.com/file.css", orig_headers, kText);

  ProxyUrlNamer url_namer;
  server_context()->set_url_namer(&url_namer);
  ResponseHeaders out_headers;
  GoogleString out_text;
  FetchFromProxy(Encode(kTestDomain, "cf", "0", "file.css", "css"),
                 true, &out_text, &out_headers);
  // Proper output
  EXPECT_STREQ("*{pretty}", out_text);

  // Private.
  ConstStringStarVector values;
  out_headers.Lookup(HttpAttributes::kCacheControl, &values);
  ASSERT_EQ(2, values.size());
  EXPECT_STREQ("max-age=400", *values[0]);
  EXPECT_STREQ("private", *values[1]);

  // Check that we ate the cookies.
  EXPECT_FALSE(out_headers.Has(HttpAttributes::kSetCookie));
}

TEST_F(ProxyInterfaceTest, ProxyResourceQueryOnly) {
  // At one point we had a bug where if we optimized a pagespeed resource
  // whose original name was a bare query, we would loop infinitely when
  // trying to fetch it from a separate-domain proxy.
  const char kUrl[] = "?somestuff";
  SetResponseWithDefaultHeaders(kUrl, kContentTypeJavascript,
                                "var a = 2;// stuff", kHtmlCacheTimeSec * 2);

  ProxyUrlNamer url_namer;
  server_context()->set_url_namer(&url_namer);
  ResponseHeaders out_headers;
  GoogleString out_text;
  FetchFromProxy(
      StrCat("http://", ProxyUrlNamer::kProxyHost,
             "/test.com/test.com/",
             EncodeNormal("", "jm", "0", kUrl, "css")),
      true, &out_text, &out_headers);
  EXPECT_STREQ("var a=2;", out_text);
  CheckBackgroundFetch(out_headers, false);
}

TEST_F(ProxyInterfaceTest, NoRehostIncompatMPS) {
  // Make sure we don't try to interpret a URL from an incompatible
  // mod_pagespeed version at our proxy host level.

  // This url will be rejected by CssUrlEncoder
  const char kOldName[] = "style.css.pagespeed.cf.0.css";
  const char kContent[] = "*     {}";
  SetResponseWithDefaultHeaders(kOldName, kContentTypeCss, kContent, 100);

  ProxyUrlNamer url_namer;
  server_context()->set_url_namer(&url_namer);
  ResponseHeaders out_headers;
  GoogleString out_text;
  FetchFromProxy(
      StrCat("http://", ProxyUrlNamer::kProxyHost,
             "/test.com/test.com/",
             EncodeNormal("", "ce", "0", kOldName, "css")),
      true, &out_text, &out_headers);
  EXPECT_EQ(HttpStatus::kOK, out_headers.status_code());
  EXPECT_STREQ(kContent, out_text);
}

// Test that we serve "Cache-Control: no-store" only when original page did.
TEST_F(ProxyInterfaceTest, NoStore) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_max_html_cache_time_ms(0);
  server_context()->ComputeSignature(options);

  // Most headers get converted to "no-cache, max-age=0".
  EXPECT_STREQ("max-age=0, no-cache",
               RewriteHtmlCacheHeader("empty", ""));
  EXPECT_STREQ("max-age=0, no-cache",
               RewriteHtmlCacheHeader("private", "private, max-age=100"));
  EXPECT_STREQ("max-age=0, no-cache",
               RewriteHtmlCacheHeader("no-cache", "no-cache"));

  // Headers with "no-store", preserve that header as well.
  EXPECT_STREQ("max-age=0, no-cache, no-store",
               RewriteHtmlCacheHeader("no-store", "no-cache, no-store"));
  EXPECT_STREQ("max-age=0, no-cache, no-store",
               RewriteHtmlCacheHeader("no-store2", "no-store, max-age=300"));
}

TEST_F(ProxyInterfaceTest, PropCacheFilter) {
  CreateFilterCallback create_filter_callback;
  factory()->AddCreateFilterCallback(&create_filter_callback);
  EnableDomCohortWritesWithDnsPrefetch();

  SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml,
                                "<div><p></p></div>", 0);
  GoogleString text_out;
  ResponseHeaders headers_out;

  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_EQ("<!-- --><div><p></p></div>", text_out);

  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_EQ("<!-- 2 elements unstable --><div><p></p></div>", text_out);

  // How many refreshes should we require before it's stable?  That
  // tuning can be done in the PropertyCacheTest.  For this
  // system-test just do a hundred blind refreshes and check again for
  // stability.
  const int kFetchIterations = 100;
  for (int i = 0; i < kFetchIterations; ++i) {
    FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  }

  // Must be stable by now!
  EXPECT_EQ("<!-- 2 elements stable --><div><p></p></div>", text_out);

  // In this algorithm we will spend a property-cache-write per fetch.
  //
  // We'll also check that we do no cache writes when there are no properties
  // to save.
  EXPECT_EQ(2 + kFetchIterations, lru_cache()->num_inserts());

  // Now change the HTML and watch the #elements change.
  SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml,
                                "<div><span><p></p></span></div>", 0);
  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_EQ("<!-- 3 elements stable --><div><span><p></p></span></div>",
            text_out);

  ClearStats();

  // Finally, disable the property-cache and note that the element-count
  // annotatation reverts to "unknown mode"
  server_context_->set_enable_property_cache(false);
  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_EQ("<!-- --><div><span><p></p></span></div>", text_out);
}

TEST_F(ProxyInterfaceTest, DomCohortWritten) {
  // Other than the write of DomCohort, there will be no properties added to
  // the cache in this test because we have not enabled the filter with
  //     CreateFilterCallback create_filter_callback;
  //     factory()->AddCreateFilterCallback(&callback);

  DisableAjax();
  GoogleString text_out;
  ResponseHeaders headers_out;

  // No writes should occur if no filter that uses the dom cohort is enabled.
  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(2, lru_cache()->num_misses());  // 1 property-cache + 1 http-cache

  // Enable a filter that uses the dom cohort and make sure property cache is
  // updated.
  ClearStats();
  EnableDomCohortWritesWithDnsPrefetch();
  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(2, lru_cache()->num_misses());  // 1 property-cache + 1 http-cache

  ClearStats();
  server_context_->set_enable_property_cache(false);
  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_misses());  // http-cache only.
}

TEST_F(ProxyInterfaceTest, StatusCodePropertyWritten) {
  DisableAjax();
  EnableDomCohortWritesWithDnsPrefetch();

  GoogleString text_out;
  ResponseHeaders headers_out;

  // Status code 404 gets written when page is not available.
  SetFetchResponse404(kPageUrl);
  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_EQ(HttpStatus::kNotFound,
            GetStatusCodeInPropertyCache(StrCat(kTestDomain, kPageUrl)));

  // Status code 200 gets written when page is available.
  SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml,
                                "<html></html>", kHtmlCacheTimeSec);
  lru_cache()->Clear();
  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_EQ(HttpStatus::kOK,
            GetStatusCodeInPropertyCache(StrCat(kTestDomain, kPageUrl)));
  // Status code 301 gets written when it is a permanent redirect.
  headers_out.Clear();
  text_out.clear();
  headers_out.SetStatusAndReason(HttpStatus::kMovedPermanently);
  SetFetchResponse(StrCat(kTestDomain, kPageUrl), headers_out, text_out);
  lru_cache()->Clear();
  FetchFromProxy(kPageUrl, true, &text_out, &headers_out);
  EXPECT_EQ(HttpStatus::kMovedPermanently,
            GetStatusCodeInPropertyCache(StrCat(kTestDomain, kPageUrl)));
}

TEST_F(ProxyInterfaceTest, PropCacheNoWritesIfHtmlEndsWithTxt) {
  CreateFilterCallback create_filter_callback;
  factory()->AddCreateFilterCallback(&create_filter_callback);

  // There will be no properties added to the cache set in this test because
  // we have not enabled the filter with
  //     CreateFilterCallback create_filter_callback;
  //     factory()->AddCreateFilterCallback(&callback);

  DisableAjax();
  SetResponseWithDefaultHeaders("page.txt", kContentTypeHtml,
                                "<div><p></p></div>", 0);
  GoogleString text_out;
  ResponseHeaders headers_out;

  FetchFromProxy("page.txt", true, &text_out, &headers_out);
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_misses());  // http-cache only

  ClearStats();
  server_context_->set_enable_property_cache(false);
  FetchFromProxy("page.txt", true, &text_out, &headers_out);
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_misses());  // http-cache only
}

TEST_F(ProxyInterfaceTest, PropCacheNoWritesForNonGetRequests) {
  CreateFilterCallback create_filter_callback;
  factory()->AddCreateFilterCallback(&create_filter_callback);

  DisableAjax();
  SetResponseWithDefaultHeaders("page.txt", kContentTypeHtml,
                                "<div><p></p></div>", 0);
  GoogleString text_out;
  ResponseHeaders headers_out;
  RequestHeaders request_headers;
  request_headers.set_method(RequestHeaders::kPost);

  FetchFromProxy("page.txt", true, &text_out, &headers_out);
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_misses());  // http-cache only

  ClearStats();
  server_context_->set_enable_property_cache(false);
  FetchFromProxy("page.txt", true, &text_out, &headers_out);
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_misses());  // http-cache only
}

TEST_F(ProxyInterfaceTest, PropCacheNoWritesIfNonHtmlDelayedCache) {
  DisableAjax();
  TestPropertyCache(kImageFilenameLackingExt, true, false, true);
}

TEST_F(ProxyInterfaceTest, PropCacheNoWritesIfNonHtmlImmediateCache) {
  // Tests rewriting a file that turns out to be a jpeg, but lacks an
  // extension, where the property-cache lookup is delivered immediately.
  DisableAjax();
  TestPropertyCache(kImageFilenameLackingExt, false, false, true);
}

TEST_F(ProxyInterfaceTest, PropCacheNoWritesIfNonHtmlThreadedCache) {
  // Tests rewriting a file that turns out to be a jpeg, but lacks an
  // extension, where the property-cache lookup is delivered in a
  // separate thread.
  DisableAjax();
  ThreadSynchronizer* sync = server_context()->thread_synchronizer();
  sync->EnableForPrefix(ProxyFetch::kCollectorPrefix);
  TestPropertyCache(kImageFilenameLackingExt, true, true, true);
}

TEST_F(ProxyInterfaceTest, StatusCodeUpdateRace) {
  // Tests rewriting a file that turns out to be a jpeg, but lacks an
  // extension, where the property-cache lookup is delivered in a
  // separate thread. Use sync points to ensure that Done() deletes the
  // collector just after the Detach() critical block is executed.
  DisableAjax();
  ThreadSynchronizer* sync = server_context()->thread_synchronizer();
  sync->EnableForPrefix(ProxyFetch::kCollectorDetach);
  sync->EnableForPrefix(ProxyFetch::kCollectorDoneDelete);
  TestPropertyCache(kImageFilenameLackingExt, false, true, true);
}

TEST_F(ProxyInterfaceTest, ThreadedHtml) {
  // Tests rewriting HTML resource where property-cache lookup is delivered
  // in a separate thread.
  DisableAjax();
  EnableDomCohortWritesWithDnsPrefetch();
  ThreadSynchronizer* sync = server_context()->thread_synchronizer();
  sync->EnableForPrefix(ProxyFetch::kCollectorPrefix);
  TestPropertyCache(kPageUrl, true, true, true);
}

TEST_F(ProxyInterfaceTest, ThreadedHtmlFetcherFailure) {
  // Tests rewriting HTML resource where property-cache lookup is delivered
  // in a separate thread, but the HTML lookup fails after emitting the
  // body.
  DisableAjax();
  EnableDomCohortWritesWithDnsPrefetch();
  mock_url_fetcher()->SetResponseFailure(AbsolutifyUrl(kPageUrl));
  TestPropertyCache(kPageUrl, true, true, false);
}

TEST_F(ProxyInterfaceTest, HtmlFetcherFailure) {
  // Tests rewriting HTML resource where property-cache lookup is
  // delivered in a blocking fashion, and the HTML lookup fails after
  // emitting the body.
  DisableAjax();
  EnableDomCohortWritesWithDnsPrefetch();
  mock_url_fetcher()->SetResponseFailure(AbsolutifyUrl(kPageUrl));
  TestPropertyCache(kPageUrl, false, false, false);
}

TEST_F(ProxyInterfaceTest, HeadersSetupRace) {
  //
  // This crash occured where an Idle-callback is used to flush HTML.
  // In this bug, we were connecting the property-cache callback to
  // the ProxyFetch and then mutating response-headers.  The property-cache
  // callback was waking up the QueuedWorkerPool::Sequence used by
  // the ProxyFetch, which was waking up and calling HeadersComplete.
  // If the implementation of HeadersComplete mutated headers itself,
  // we'd have a deadly race.
  //
  // This test uses the ThreadSynchronizer class to induce the desired
  // race, with strategically placed calls to Signal and Wait.
  //
  // Note that the fix for the race means that one of the Signals does
  // not occur at all, so we have to declare it as "Sloppy" so the
  // ThreadSynchronizer class doesn't vomit on destruction.
  const int kIdleCallbackTimeoutMs = 10;
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_idle_flush_time_ms(kIdleCallbackTimeoutMs);
  options->set_flush_html(true);
  server_context()->ComputeSignature(options);
  DisableAjax();
  EnableDomCohortWritesWithDnsPrefetch();
  ThreadSynchronizer* sync = server_context()->thread_synchronizer();
  sync->EnableForPrefix(ProxyFetch::kHeadersSetupRacePrefix);
  ThreadSystem* thread_system = server_context()->thread_system();
  QueuedWorkerPool pool(1, "test", thread_system);
  QueuedWorkerPool::Sequence* sequence = pool.NewSequence();
  WorkerTestBase::SyncPoint sync_point(thread_system);
  sequence->Add(MakeFunction(static_cast<ProxyInterfaceTestBase*>(this),
                             &ProxyInterfaceTest::TestHeadersSetupRace));
  sequence->Add(new WorkerTestBase::NotifyRunFunction(&sync_point));
  sync->TimedWait(ProxyFetch::kHeadersSetupRaceAlarmQueued,
                  ProxyFetch::kTestSignalTimeoutMs);
  {
    // Trigger the idle-callback, if it has been queued.
    ScopedMutex lock(mock_scheduler()->mutex());
    mock_scheduler()->ProcessAlarms(kIdleCallbackTimeoutMs * Timer::kMsUs);
  }
  sync->Wait(ProxyFetch::kHeadersSetupRaceDone);
  sync_point.Wait();
  pool.ShutDown();
  sync->AllowSloppyTermination(ProxyFetch::kHeadersSetupRaceAlarmQueued);
}

// TODO(jmarantz): add a test with a simulated slow cache to see what happens
// when the rest of the system must block, buffering up incoming HTML text,
// waiting for the property-cache lookups to complete.

// Test that we set the Experiment cookie up appropriately.
TEST_F(ProxyInterfaceTest, ExperimentTest) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_ga_id("123-455-2341");
  options->set_running_experiment(true);
  NullMessageHandler handler;
  options->AddExperimentSpec("id=2;enable=extend_cache;percent=100", &handler);
  server_context()->ComputeSignature(options);

  SetResponseWithDefaultHeaders("example.jpg", kContentTypeJpeg,
                                "image data", 300);

  ResponseHeaders headers;
  const char kContent[] = "<html><head></head><body>A very compelling "
      "article with an image: <img src=example.jpg></body></html>";
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  SetFetchResponse(AbsolutifyUrl("text.html"), headers, kContent);
  headers.Clear();

  GoogleString text;
  FetchFromProxy("text.html", true, &text, &headers);
  // Assign all visitors to an experiment_spec.
  EXPECT_TRUE(headers.Has(HttpAttributes::kSetCookie));
  ConstStringStarVector values;
  headers.Lookup(HttpAttributes::kSetCookie, &values);
  bool found = false;
  for (int i = 0, n = values.size(); i < n; ++i) {
    if (values[i]->find(experiment::kExperimentCookie) == 0) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
  // Image cache-extended and including experiment_spec 'a'.
  EXPECT_TRUE(text.find("example.jpg.pagespeed.a.ce") != GoogleString::npos);

  headers.Clear();
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  SetFetchResponse(AbsolutifyUrl("text2.html"), headers, kContent);
  headers.Clear();
  text.clear();

  RequestHeaders req_headers;
  req_headers.Add(HttpAttributes::kCookie, "PageSpeedExperiment=2");

  FetchFromProxy("text2.html", req_headers, true, &text, &headers);
  // Visitor already has cookie with id=2; don't give them a new one.
  EXPECT_FALSE(headers.Has(HttpAttributes::kSetCookie));
  // Image cache-extended and including experiment_spec 'a'.
  EXPECT_TRUE(text.find("example.jpg.pagespeed.a.ce") != GoogleString::npos);

  // Check that we don't include an experiment_spec index in urls for the "no
  // experiment" group (id=0).
  headers.Clear();
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  SetFetchResponse(AbsolutifyUrl("text3.html"), headers, kContent);
  headers.Clear();
  text.clear();

  RequestHeaders req_headers2;
  req_headers2.Add(HttpAttributes::kCookie, "PageSpeedExperiment=0");

  FetchFromProxy("text3.html", req_headers2, true, &text, &headers);
  EXPECT_FALSE(headers.Has(HttpAttributes::kSetCookie));
  EXPECT_TRUE(text.find("example.jpg.pagespeed.ce") != GoogleString::npos);
}

TEST_F(ProxyInterfaceTest, UrlAttributeTest) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->EnableFilter(RewriteOptions::kRewriteDomains);
  options->set_domain_rewrite_hyperlinks(true);
  NullMessageHandler handler;
  options->WriteableDomainLawyer()->AddRewriteDomainMapping(
      "http://dst.example.com", "http://src.example.com", &handler);
  options->AddUrlValuedAttribute(
      "span", "src", semantic_type::kHyperlink);
  options->AddUrlValuedAttribute("hr", "imgsrc", semantic_type::kImage);
  server_context()->ComputeSignature(options);

  SetResponseWithDefaultHeaders(
      "http://src.example.com/null", kContentTypeHtml, "", 0);
  ResponseHeaders headers;
  const char kContent[] = "<html><head></head><body>"
      "<img src=\"http://src.example.com/null\">"
      "<hr imgsrc=\"http://src.example.com/null\">"
      "<span src=\"http://src.example.com/null\"></span>"
      "<other src=\"http://src.example.com/null\"></other></body></html>";
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  SetFetchResponse(AbsolutifyUrl("text.html"), headers, kContent);
  headers.Clear();
  GoogleString text;
  FetchFromProxy("text.html", true, &text, &headers);

  // img.src, hr.imgsrc, and span.src are all rewritten
  EXPECT_TRUE(text.find("<img src=\"http://dst.example.com/null\"") !=
              GoogleString::npos);
  EXPECT_TRUE(text.find("<hr imgsrc=\"http://dst.example.com/null\"") !=
              GoogleString::npos);
  EXPECT_TRUE(text.find("<span src=\"http://dst.example.com/null\"") !=
              GoogleString::npos);
  // other.src not rewritten
  EXPECT_TRUE(text.find("<other src=\"http://src.example.com/null\"") !=
              GoogleString::npos);
}

TEST_F(ProxyInterfaceTest, TestOptionsAndDeviceTypeUsedInCacheKey) {
  TestOptionsAndDeviceTypeUsedInCacheKey(UserAgentMatcher::kMobile);
  TestOptionsAndDeviceTypeUsedInCacheKey(UserAgentMatcher::kDesktop);
}

TEST_F(ProxyInterfaceTest, TestFallbackPropertiesUsageWithQueryParams) {
  GoogleString url("http://www.test.com/a/b.html?withquery=some");
  GoogleString fallback_url("http://www.test.com/a/b.html?withquery=different");
  TestFallbackPageProperties(url, fallback_url);
}

TEST_F(ProxyInterfaceTest, TestFallbackPropertiesUsageWithLeafNode) {
  GoogleString url("http://www.test.com/a/b.html");
  GoogleString fallback_url("http://www.test.com/a/c.html");
  TestFallbackPageProperties(url, fallback_url);
}

TEST_F(ProxyInterfaceTest,
       TestFallbackPropertiesUsageWithLeafNodeHavingTrailingSlash) {
  GoogleString url("http://www.test.com/a/b/");
  GoogleString fallback_url("http://www.test.com/a/c/");
  TestFallbackPageProperties(url, fallback_url);
}

TEST_F(ProxyInterfaceTest, TestNoFallbackCallWithNoLeaf) {
  GoogleUrl gurl("http://www.test.com/");
  options()->set_use_fallback_property_cache_values(true);
  StringAsyncFetch callback(
      RequestContext::NewTestRequestContext(
          server_context()->thread_system()));
  RequestHeaders request_headers;
  callback.set_request_headers(&request_headers);
  scoped_ptr<ProxyFetchPropertyCallbackCollector> callback_collector(
      proxy_interface_->InitiatePropertyCacheLookup(
          false, gurl, options(), &callback, false, NULL));

  PropertyPage* fallback_page = callback_collector->fallback_property_page()
      ->property_page_with_fallback_values();
  // No PropertyPage with fallback values.
  EXPECT_EQ(NULL, fallback_page);
}

TEST_F(ProxyInterfaceTest, TestSkipBlinkCohortLookUp) {
  GoogleUrl gurl("http://www.test.com/");
  StringAsyncFetch callback(
      RequestContext::NewTestRequestContext(server_context()->thread_system()));
  RequestHeaders request_headers;
  callback.set_request_headers(&request_headers);
  scoped_ptr<ProxyFetchPropertyCallbackCollector> callback_collector(
      proxy_interface_->InitiatePropertyCacheLookup(
          false, gurl, options(), &callback, false, NULL));

  // Cache lookup only for dom cohort.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_misses());
}

TEST_F(ProxyInterfaceTest, TestSkipBlinkCohortLookUpInFallbackPage) {
  GoogleUrl gurl("http://www.test.com/1.html?a=b");
  options()->set_use_fallback_property_cache_values(true);
  StringAsyncFetch callback(
      RequestContext::NewTestRequestContext(server_context()->thread_system()));
  RequestHeaders request_headers;
  callback.set_request_headers(&request_headers);
  scoped_ptr<ProxyFetchPropertyCallbackCollector> callback_collector(
      proxy_interface_->InitiatePropertyCacheLookup(
          false, gurl, options(), &callback, true, NULL));

  // Cache lookup for:
  // dom and blink cohort for actual property page.
  // dom cohort for fallback property page.
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(3, lru_cache()->num_misses());
}

TEST_F(ProxyInterfaceTest, BailOutOfParsing) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->EnableExtendCacheFilters();
  options->set_max_html_parse_bytes(60);
  server_context()->ComputeSignature(options);

  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.jpg"), kContentTypeJpeg,
                                "image", kHtmlCacheTimeSec * 2);

  // This is larger than 60 bytes.
  const char kContent[] = "<html><head></head><body>"
      "<img src=\"1.jpg\">"
      "<p>Some very long and very boring text</p>"
      "</body></html>";
  SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml, kContent, 0);
  ResponseHeaders headers;
  GoogleString text;
  FetchFromProxy(kPageUrl, true, &text, &headers);
  // For the first request, we bail out of parsing and insert the redirect. We
  // also update the pcache.
  EXPECT_EQ("<html><script type=\"text/javascript\">"
            "window.location=\"http://test.com/page.html?ModPagespeed=off\";"
            "</script></html>", text);

  headers.Clear();
  text.clear();
  // We look up the pcache and find that we should skip parsing. Hence, we just
  // pass the bytes through.
  FetchFromProxy(kPageUrl, true, &text, &headers);
  EXPECT_EQ(kContent, text);

  // This is smaller than 60 bytes.
  const char kNewContent[] = "<html><head></head><body>"
       "<img src=\"1.jpg\"></body></html>";

  SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml, kNewContent, 0);
  headers.Clear();
  text.clear();
  // We still remember that we should skip parsing. Hence, we pass the bytes
  // through. However, after this request, we update the pcache to indicate that
  // we should no longer skip parsing.
  FetchFromProxy(kPageUrl, true, &text, &headers);
  EXPECT_EQ(kNewContent, text);

  headers.Clear();
  text.clear();
  // This request is rewritten.
  FetchFromProxy(kPageUrl, true, &text, &headers);
  EXPECT_EQ("<html><head></head><body>"
            "<img src=\"http://test.com/1.jpg.pagespeed.ce.0.jpg\">"
            "</body></html>", text);
}

TEST_F(ProxyInterfaceTest, LoggingInfoRewriteInfoMaxSize) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->set_max_rewrite_info_log_size(10);
  server_context()->ComputeSignature(options);

  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.jpg"), kContentTypeJpeg,
                                "image", kHtmlCacheTimeSec * 2);

  GoogleString content = "<html><head></head><body>";
  for (int i = 0; i < 50; ++i) {
    StrAppend(&content, "<img src=\"1.jpg\">");
  }
  StrAppend(&content,  "</body></html>");

  SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml, content, 0);
  ResponseHeaders headers;
  GoogleString text;
  FetchFromProxy(kPageUrl, true, &text, &headers);

  GoogleString expected_response(content);
  GlobalReplaceSubstring("1.jpg", "http://test.com/1.jpg.pagespeed.ce.0.jpg",
                         &expected_response);
  EXPECT_STREQ(expected_response, text);
  EXPECT_EQ(10, logging_info()->rewriter_info_size());
  EXPECT_TRUE(logging_info()->rewriter_info_size_limit_exceeded());
}

TEST_F(ProxyInterfaceTest, WebpImageReconstruction) {
  RewriteOptions* options = server_context()->global_options();
  options->ClearSignatureForTesting();
  options->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  server_context()->ComputeSignature(options);

  AddFileToMockFetcher(StrCat(kTestDomain, "1.jpg"), "Puzzle.jpg",
                       kContentTypeJpeg, 100);
  ResponseHeaders response_headers;
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent, "webp");

  const GoogleString kWebpUrl = Encode(kTestDomain, "ic", "0", "1.jpg", "webp");

  FetchFromProxy(kWebpUrl, request_headers, true, &text, &response_headers);
  response_headers.ComputeCaching();
  EXPECT_STREQ(kContentTypeWebp.mime_type(),
               response_headers.Lookup1(HttpAttributes::kContentType));
  EXPECT_EQ(ServerContext::kGeneratedMaxAgeMs, response_headers.cache_ttl_ms());

  const char kCssWithEmbeddedImage[] = "*{background-image:url(%s)}";
  SetResponseWithDefaultHeaders(
      "embedded.css", kContentTypeCss,
      StringPrintf(kCssWithEmbeddedImage, "1.jpg"), kHtmlCacheTimeSec * 2);

  FetchFromProxy(Encode(kTestDomain, "cf", "0", "embedded.css", "css"),
                 request_headers, true, &text, &response_headers);
  response_headers.ComputeCaching();
  EXPECT_EQ(ServerContext::kGeneratedMaxAgeMs, response_headers.cache_ttl_ms());
  EXPECT_EQ(StringPrintf(kCssWithEmbeddedImage, kWebpUrl.c_str()), text);
}

}  // namespace net_instaweb
