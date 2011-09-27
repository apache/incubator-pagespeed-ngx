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

// Unit-tests for ProxyInterface

#include "net/instaweb/automatic/public/proxy_interface.h"

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/resource_manager_test_base.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

namespace {

const char kCssContent[] = "* { display: none; }";
const char kMinimizedCssContent[] = "*{display:none}";

// Like ExpectCallback but for asynchronous invocation -- it lets
// one specify a WorkerTestBase::SyncPoint to help block until completion.
class AsyncExpectCallback : public ExpectCallback {
 public:
  AsyncExpectCallback(bool expect_success, WorkerTestBase::SyncPoint* notify)
      : ExpectCallback(expect_success), notify_(notify) {}

  virtual ~AsyncExpectCallback() {}

  virtual void Done(bool success) {
    ExpectCallback::Done(success);
    notify_->Notify();
  }

 private:
  WorkerTestBase::SyncPoint* notify_;
  DISALLOW_COPY_AND_ASSIGN(AsyncExpectCallback);
};

class MockUrlNamer : public UrlNamer {
 public:
  explicit MockUrlNamer(RewriteOptions* options) : options_(options) {}
  virtual RewriteOptions* DecodeOptions(const GoogleUrl& request_url,
                                        const RequestHeaders& request_headers,
                                        MessageHandler* handler) {
    return options_->Clone();
  }

 private:
  RewriteOptions* options_;
  DISALLOW_COPY_AND_ASSIGN(MockUrlNamer);
};

// TODO(morlovich): This currently relies on ResourceManagerTestBase to help
// setup fetchers; and also indirectly to prevent any rewrites from timing out
// (as it runs the tests with real scheduler but mock timer). It would probably
// be better to port this away to use TestRewriteDriverFactory directly.
class ProxyInterfaceTest : public ResourceManagerTestBase {
 protected:
  static const int kHtmlCacheTimeSec = 5000;

  ProxyInterfaceTest() {}
  virtual ~ProxyInterfaceTest() {}

  virtual void SetUp() {
    resource_manager()->options()->EnableFilter(RewriteOptions::kRewriteCss);
    resource_manager()->options()->set_html_cache_time_ms(
        kHtmlCacheTimeSec * Timer::kSecondMs);
    ResourceManagerTestBase::SetUp();
    proxy_interface_.reset(
        new ProxyInterface("localhost", 80, resource_manager(), statistics()));
    start_time_ms_ = mock_timer()->NowMs();
  }

  virtual void TearDown() {
    // Make sure all the jobs are over before we check for leaks ---
    // someone might still be trying to clean themselves up.
    mock_scheduler()->AwaitQuiescence();
    EXPECT_EQ(0, resource_manager()->num_active_rewrite_drivers());
    ResourceManagerTestBase::TearDown();
  }

  void FetchFromProxy(const StringPiece& url,
                      bool expect_success,
                      GoogleString* string_out,
                      ResponseHeaders* headers_out) {
    StringWriter writer(string_out);
    RequestHeaders request_headers;

    WorkerTestBase::SyncPoint sync(resource_manager()->thread_system());
    AsyncExpectCallback callback(expect_success, &sync);
    bool already_done =
        proxy_interface_->StreamingFetch(
            AbsolutifyUrl(url), request_headers, headers_out, &writer,
            message_handler(), &callback);
    if (already_done) {
      EXPECT_TRUE(callback.done());
    } else {
      sync.Wait();
    }
  }

  void CheckHeaders(const ResponseHeaders& headers,
                    const ContentType& expect_type) {
    ASSERT_TRUE(headers.has_status_code());
    EXPECT_EQ(HttpStatus::kOK, headers.status_code());
    EXPECT_STREQ(expect_type.mime_type(),
                 headers.Lookup1(HttpAttributes::kContentType));
  }

  RewriteOptions* GetCustomOptions(const StringPiece& url,
                                   const RequestHeaders& request_headers) {
    // The default url_namer does not yield any name-derived options, and we
    // have not specified any URL params or request-headers, so there will be
    // no custom options, and no errors.
    GoogleUrl gurl(url);
    ProxyInterface::OptionsBoolPair options_success =
        proxy_interface_->GetCustomOptions(gurl, request_headers,
                                           message_handler());
    EXPECT_TRUE(options_success.second);
    return options_success.first;
  }

  scoped_ptr<ProxyInterface> proxy_interface_;
  int64 start_time_ms_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyInterfaceTest);
};

TEST_F(ProxyInterfaceTest, FetchFailure) {
  GoogleString text;
  ResponseHeaders headers;

  // We don't want fetcher to fail the test, merely the fetch.
  SetFetchFailOnUnexpected(false);
  FetchFromProxy("invalid", false, &text, &headers);
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

  InitResponseHeaders("text.txt", kContentTypeText, kContent,
                      kHtmlCacheTimeSec * 2);
  FetchFromProxy("text.txt", true, &text, &headers);
  CheckHeaders(headers, kContentTypeText);
  EXPECT_EQ(kContent, text);
}

TEST_F(ProxyInterfaceTest, RewriteHtml) {
  GoogleString text;
  ResponseHeaders headers;

  InitResponseHeaders("page.html", kContentTypeHtml,
                      CssLinkHref("a.css"), kHtmlCacheTimeSec * 2);
  InitResponseHeaders("a.css", kContentTypeCss, kCssContent,
                      kHtmlCacheTimeSec * 2);

  FetchFromProxy("page.html", true, &text, &headers);
  CheckHeaders(headers, kContentTypeHtml);
  EXPECT_EQ(CssLinkHref(AbsolutifyUrl("a.css.pagespeed.cf.0.css")), text);
  headers.ComputeCaching();
  EXPECT_LE(start_time_ms_ + kHtmlCacheTimeSec * Timer::kSecondMs,
            headers.CacheExpirationTimeMs());

  // Fetch the rewritten resource as well.
  text.clear();
  FetchFromProxy(AbsolutifyUrl("a.css.pagespeed.cf.0.css"), true,
                 &text, &headers);
  CheckHeaders(headers, kContentTypeCss);
  headers.ComputeCaching();
  EXPECT_LE(start_time_ms_ + Timer::kYearMs, headers.CacheExpirationTimeMs());
  EXPECT_EQ(kMinimizedCssContent, text);
}

TEST_F(ProxyInterfaceTest, ReconstructResource) {
  GoogleString text;
  ResponseHeaders headers;

  // Fetching of a rewritten resource we did not just create
  // after an HTML rewrite.
  InitResponseHeaders("a.css", kContentTypeCss, kCssContent,
                      kHtmlCacheTimeSec * 2);
  FetchFromProxy("a.css.pagespeed.cf.0.css", true, &text, &headers);
  CheckHeaders(headers, kContentTypeCss);
  headers.ComputeCaching();
  EXPECT_LE(start_time_ms_ + Timer::kYearMs, headers.CacheExpirationTimeMs());
  EXPECT_EQ(kMinimizedCssContent, text);
}

TEST_F(ProxyInterfaceTest, CustomOptionsWithNoUrlNamerOptions) {
  // The default url_namer does not yield any name-derived options, and we
  // have not specified any URL params or request-headers, so there will be
  // no custom options, and no errors.
  RequestHeaders request_headers;
  scoped_ptr<RewriteOptions> options(
      GetCustomOptions("http://example.com/", request_headers));
  ASSERT_TRUE(options.get() == NULL);

  // Now put a query-param in, just turning on PageSpeed.  The core filters
  // should be enabled.
  options.reset(GetCustomOptions(
      "http://example.com/?ModPagespeed=on",
      request_headers));
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kExtendCache));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineJavascript));

  // Now explicitly enable a filter, which should disable others.
  options.reset(GetCustomOptions(
      "http://example.com/?ModPagespeedFilters=extend_cache",
      request_headers));
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->Enabled(RewriteOptions::kExtendCache));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineJavascript));

  // Now put a request-header in, turning off pagespeed.  request-headers get
  // priority over query-params.
  request_headers.Add("ModPagespeed", "off");
  options.reset(GetCustomOptions(
      "http://example.com/?ModPagespeed=on",
      request_headers));
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_FALSE(options->enabled());

  // Now explicitly enable a bogus filter, which should will cause the
  // options to be uncomputable.
  GoogleUrl gurl("http://example.com/?ModPagespeedFilters=bogus_filter");
  EXPECT_FALSE(proxy_interface_->GetCustomOptions(gurl, request_headers,
                                                  message_handler()).second);
}

TEST_F(ProxyInterfaceTest, CustomOptionsWithUrlNamerOptions) {
  // Inject a url-namer that will establish a domain configuration.
  RewriteOptions namer_options;
  namer_options.EnableFilter(RewriteOptions::kCombineJavascript);
  MockUrlNamer mock_url_namer(&namer_options);
  resource_manager()->set_url_namer(&mock_url_namer);

  RequestHeaders request_headers;
  scoped_ptr<RewriteOptions> options(
      GetCustomOptions("http://example.com/", request_headers));
  // Even with no query-params or request-headers, we get the custom
  // options generated from the UrlNamer.
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_FALSE(options->Enabled(RewriteOptions::kExtendCache));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineJavascript));

  // Now combine with query params, which turns core-filters on.
  options.reset(GetCustomOptions(
      "http://example.com/?ModPagespeed=on",
      request_headers));
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_TRUE(options->Enabled(RewriteOptions::kExtendCache));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineJavascript));

  // Explicitly enable a filter in query-params, which will turn off
  // the core filters that have not been explicitly enabled.  Note
  // that explicit filter-setting in query-params overrides completely
  // the options set from the UrlNamer.
  options.reset(GetCustomOptions(
      "http://example.com/?ModPagespeedFilters=combine_css",
      request_headers));
  ASSERT_TRUE(options.get() != NULL);
  EXPECT_TRUE(options->enabled());
  EXPECT_FALSE(options->Enabled(RewriteOptions::kExtendCache));
  EXPECT_TRUE(options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options->Enabled(RewriteOptions::kCombineJavascript));

  // Now explicitly enable a bogus filter, which should will cause the
  // options to be uncomputable.
  GoogleUrl gurl("http://example.com/?ModPagespeedFilters=bogus_filter");
  EXPECT_FALSE(proxy_interface_->GetCustomOptions(gurl, request_headers,
                                                  message_handler()).second);
}

}  // namespace

}  // namespace net_instaweb
