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

// Author: mmohabey@google.com (Megha Mohabey)
//         pulkitg@google.com (Pulkit Goyal)

// Unit-tests for CacheHtmlFlow.

#include "net/instaweb/automatic/public/proxy_interface_test_base.h"

#include "base/logging.h"
#include "net/instaweb/automatic/public/cache_html_flow.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/automatic/public/proxy_interface.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/cache_html_info_finder.h"
#include "net/instaweb/rewriter/public/critical_css_filter.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/delay_images_filter.h"
#include "net/instaweb/rewriter/public/flush_early_info_finder_test_base.h"
#include "net/instaweb/rewriter/public/js_disable_filter.h"
#include "net/instaweb/rewriter/public/mock_critical_css_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/cache_property_store.h"
#include "net/instaweb/util/public/delay_cache.h"
#include "net/instaweb/util/public/dynamic_annotations.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_synchronizer.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/worker_test_base.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/wildcard.h"

namespace net_instaweb {

class AbstractMutex;
class AsyncFetch;

namespace {

const char kTestUrl[] = "http://test.com/text.html";

const char kMockHashValue[] = "MDAwMD";

const char kCssContent[] = "* { display: none; }";

const char kSampleJpgFile[] = "Sample.jpg";

const char kLinuxUserAgent[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/536.5 "
    "(KHTML, like Gecko) Chrome/19.0.1084.46 Safari/536.5";

const char kWindowsUserAgent[] =
    "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:15.0) Gecko/20120427 "
    "Firefox/15.0a1";

const char kBlackListUserAgent[] =
    "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:15.0) Gecko/20120427 Firefox/2.0a1";

const char kWhitespace[] = "                  ";

const char kHtmlInput[] =
    "<html>"
    "<head>"
    "</head>"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
    "</div>"
    "</body></html>";

const char kHtmlInputWithMinifiableJs[] =
    "<html>"
    "<head>"
    "<script type=\"text/javascript\">var a = \"hello\"; </script>"
    "</head>"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
    "</div>"
    "</body></html>";

const char kHtmlInputWithMinifiedJs[] =
    "<html>"
    "<head>"
    "<script pagespeed_orig_type=\"text/javascript\" "
    "type=\"text/psajs\" orig_index=\"0\">var a=\"hello\";</script>"
    "</head>"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
    "</div>"
    "%s<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\"></script>"
    "</body></html>";

const char kHtmlInputWithExtraCommentAndNonCacheable[] =
    "<html>"
    "<head>"
    "</head>"
    "<body>\n"
    "<!-- Hello -->"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
    "</div>"
    "</body></html>";

const char kHtmlInputWithExtraAttribute[] =
    "<html>"
    "<head>"
    "</head>"
    "<body>\n"
    "<div id=\"header\" align=\"center\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
    "</div>"
    "</body></html>";

const char kHtmlInputWithEmptyVisiblePortions[] =
    "<html><body></body></html>";

const char kSmallHtmlInput[] =
    "<html><head></head><body>A small test html.</body></html>";
const char kHtmlInputForNoBlink[] =
    "<html><head></head><body></body></html>";

const char kBlinkOutputCommon[] =
    "<html><head></head><body>"
    "<noscript><meta HTTP-EQUIV=\"refresh\" content=\"0;"
    "url='%s?PageSpeed=noscript'\" />"
    "<style><!--table,div,span,font,p{display:none} --></style>"
    "<div style=\"display:block\">Please click "
    "<a href=\"%s?PageSpeed=noscript\">here</a> "
    "if you are not redirected within a few seconds.</div></noscript>"
    "\n<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
    "<!--GooglePanel begin panel-id-1.0-->"
    "<!--GooglePanel end panel-id-1.0-->"
    "<!--GooglePanel begin panel-id-0.0-->"
    "<!--GooglePanel end panel-id-0.0-->"
    "<!--GooglePanel begin panel-id-0.1-->"
    "<!--GooglePanel end panel-id-0.1-->"
    "</div>"
    "</body></html>"
    "%s<script type=\"text/javascript\" src=\"/psajs/blink.0.js\"></script>"
    "<script type=\"text/javascript\">"
    "pagespeed.panelLoaderInit();</script>\n"
    "<script type=\"text/javascript\">"
    "pagespeed.panelLoader.setRequestFromInternalIp();</script>\n";

const char kCookieScript[] =
    "<script>pagespeed.panelLoader.loadCookies([\"helo=world; path=/\"]);"
    "</script>";

const char kBlinkOutputSuffix[] =
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-1.0\":{\"instance_html\":\"<h2 id=\\\"beforeItems\\\"> This is before Items </h2>\",\"xpath\":\"//div[@id=\\\"container\\\"]/h2[1]\"}}\n);</script>"  // NOLINT
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.0\":{\"instance_html\":\"<div class=\\\"item\\\"><img src=\\\"%s\\\"><img src=\\\"image2\\\"></div>\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[2]\"}}\n);</script>"  // NOLINT
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.1\":{\"instance_html\":\"<div class=\\\"item\\\"><img src=\\\"image3\\\"><div class=\\\"item\\\"><img src=\\\"image4\\\"></div></div>\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[3]\"}}\n);</script>"  // NOLINT
    "<script>pagespeed.panelLoader.bufferNonCriticalData({});</script>";  // NOLINT

const char kBlinkOutputWithCacheablePanelsNoCookiesSuffix[] =
    "<script>pagespeed.panelLoader.bufferNonCriticalData();</script>\n"
    "</body></html>\n";

const char kBlinkOutputWithCacheablePanelsCookiesSuffix[] =
    "</body></html>\n";

const char kFakePngInput[] = "FakePng";

const char kFlushSubresourcesHtmlInput[] =
    "<html>"
    "<head>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"1.css\">"
    "</head>"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
    "</body></html>";

const char kNoBlinkUrl[] =
    "http://test.com/noblink_text.html?PageSpeed=noscript";

const char kNoScriptTextUrl[] = "http://test.com/text.html?PageSpeed=noscript";

// Like ExpectStringAsyncFetch but for asynchronous invocation -- it lets
// one specify a WorkerTestBase::SyncPoint to help block until completion.
class AsyncExpectStringAsyncFetch : public ExpectStringAsyncFetch {
 public:
  AsyncExpectStringAsyncFetch(bool expect_success,
                              WorkerTestBase::SyncPoint* notify,
                              const RequestContextPtr& request_context)
      : ExpectStringAsyncFetch(expect_success, request_context),
        notify_(notify) {
  }

  virtual ~AsyncExpectStringAsyncFetch() {}

  virtual void HandleDone(bool success) {
    ExpectStringAsyncFetch::HandleDone(success);
    notify_->Notify();
  }

 private:
  WorkerTestBase::SyncPoint* notify_;
  DISALLOW_COPY_AND_ASSIGN(AsyncExpectStringAsyncFetch);
};

class ProxyInterfaceWithDelayCache : public ProxyInterface {
 public:
  ProxyInterfaceWithDelayCache(const StringPiece& hostname, int port,
                               ServerContext* manager, Statistics* stats,
                               DelayCache* delay_cache,
                               TestRewriteDriverFactory* factory)
      : ProxyInterface(hostname, port, manager, stats),
        manager_(manager),
        delay_cache_(delay_cache),
        key_(""),
        factory_(factory) {
  }

  // Initiates the PropertyCache look up.
  virtual ProxyFetchPropertyCallbackCollector* InitiatePropertyCacheLookup(
      bool is_resource_fetch,
      const GoogleUrl& request_url,
      RewriteOptions* options,
      AsyncFetch* async_fetch,
      const bool requires_blink_cohort,
      bool* added_page_property_callback) {
    GoogleString options_signature_hash;
    if (options != NULL) {
      manager_->ComputeSignature(options);
      options_signature_hash =
          manager_->GetRewriteOptionsSignatureHash(options);
    }
    PropertyCache* pcache = manager_->page_property_cache();
    const PropertyCache::Cohort* cohort =
        pcache->GetCohort(BlinkUtil::kBlinkCohort);
    key_ = factory_->cache_property_store()->CacheKey(
        request_url.Spec(),
        options_signature_hash,
        UserAgentMatcher::DeviceTypeSuffix(UserAgentMatcher::kDesktop),
        cohort);
    delay_cache_->DelayKey(key_);
    if (added_page_property_callback != NULL) {
      *added_page_property_callback = true;
    }
    return ProxyFetchFactory::InitiatePropertyCacheLookup(
        is_resource_fetch, request_url, server_context_, options, async_fetch,
        requires_blink_cohort, added_page_property_callback);
  }

  const GoogleString& key() const { return key_; }

 private:
  ServerContext* manager_;
  DelayCache* delay_cache_;
  GoogleString key_;
  TestRewriteDriverFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyInterfaceWithDelayCache);
};

}  // namespace

// RequestContext that overrides NewSubordinateLogRecord to return a
// CopyOnWriteLogRecord that copies to a logging_info given at construction
// time.
class TestRequestContext : public RequestContext {
 public:
  TestRequestContext(ThreadSystem* threads, LoggingInfo* logging_info)
      : RequestContext(threads->NewMutex(), NULL),
        logging_info_copy_(logging_info) {}

  virtual AbstractLogRecord* NewSubordinateLogRecord(
      AbstractMutex* logging_mutex) {
    return new CopyOnWriteLogRecord(logging_mutex, logging_info_copy_);
  }

 private:
  LoggingInfo* logging_info_copy_;  // Not owned by us.

  DISALLOW_COPY_AND_ASSIGN(TestRequestContext);
};
typedef RefCountedPtr<TestRequestContext> TestRequestContextPtr;

// TODO(nikhilmadan): Test cookies, fetch failures, 304 responses etc.
// TODO(nikhilmadan): Test 304 responses etc.
class CacheHtmlFlowTest : public ProxyInterfaceTestBase {
 protected:
  static const int kHtmlCacheTimeSec = 5000;

  CacheHtmlFlowTest() : test_request_context_(TestRequestContextPtr(
      new TestRequestContext(server_context()->thread_system(),
                             &cache_html_logging_info_))) {
    ConvertTimeToString(MockTimer::kApr_5_2010_ms, &start_time_string_);
  }

  // These must be run prior to the calls to 'new CustomRewriteDriverFactory'
  // in the constructor initializer above.  Thus the calls to Initialize() in
  // the base class are too late.
  static void SetUpTestCase() {
    RewriteOptions::Initialize();
  }
  static void TearDownTestCase() {
    RewriteOptions::Terminate();
  }

  void InitializeOutputs(RewriteOptions* options) {
    blink_output_partial_ = StringPrintf(
        kBlinkOutputCommon, kTestUrl, kTestUrl,
        GetJsDisableScriptSnippet(options).c_str());
    blink_output_ = StrCat(blink_output_partial_, kCookieScript,
                           StringPrintf(kBlinkOutputSuffix, "image1"));
    noblink_output_ = StrCat("<html><head></head><body>",
                             StringPrintf(kNoScriptRedirectFormatter,
                                          kNoBlinkUrl, kNoBlinkUrl),
                             "</body></html>");
    blink_output_with_cacheable_panels_no_cookies_ =
        StrCat(StringPrintf(kBlinkOutputCommon, "http://test.com/flaky.html",
                            "http://test.com/flaky.html",
                            GetJsDisableScriptSnippet(options).c_str()),
               kBlinkOutputWithCacheablePanelsNoCookiesSuffix);
    blink_output_with_cacheable_panels_cookies_ =
        StrCat(StringPrintf(kBlinkOutputCommon, "http://test.com/cache.html",
                            "http://test.com/cache.html",
                            GetJsDisableScriptSnippet(options).c_str()),
               kBlinkOutputWithCacheablePanelsCookiesSuffix);
  }

  GoogleString GetJsDisableScriptSnippet(RewriteOptions* options) {
    if (options->enable_defer_js_experimental()) {
      return StrCat("<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
                    JsDisableFilter::kEnableJsExperimental,
                    "</script>");
    } else {
      return "";
    }
  }

  virtual void SetUp() {
    const PropertyCache::Cohort* blink_cohort =
        SetupCohort(server_context_->page_property_cache(),
                    BlinkUtil::kBlinkCohort);
    server_context_->set_blink_cohort(blink_cohort);
    server_context_->set_enable_property_cache(true);
    InitHasher();
    ThreadSynchronizer* sync = server_context()->thread_synchronizer();
    sync->EnableForPrefix(CacheHtmlFlow::kBackgroundComputationDone);
    sync->EnableForPrefix(ProxyFetch::kCollectorFinish);
    sync->AllowSloppyTermination(CacheHtmlFlow::kBackgroundComputationDone);
    sync->AllowSloppyTermination(ProxyFetch::kCollectorFinish);
    flush_early_info_finder_ = new MeaningfulFlushEarlyInfoFinder;
    server_context()->set_flush_early_info_finder(flush_early_info_finder_);
    options_.reset(server_context()->NewOptions());
    options_->EnableFilter(RewriteOptions::kCachePartialHtml);
    options_->EnableFilter(RewriteOptions::kRewriteJavascript);
    options_->set_non_cacheables_for_cache_partial_html(
        "class=item,id=beforeItems");

    options_->Disallow("*blacklist*");

    InitializeOutputs(options_.get());
    SetRewriteOptions(options_.get());

    server_context()->ComputeSignature(options_.get());

    ProxyInterfaceTestBase::SetUp();
    ProxyInterface::InitStats(statistics());
    proxy_interface_.reset(
        new ProxyInterface("localhost", 80, server_context(), statistics()));

    server_context()->url_namer()->set_proxy_domain("http://proxy-domain");

    server_context()->set_cache_html_info_finder(new CacheHtmlInfoFinder());

    SetTimeMs(MockTimer::kApr_5_2010_ms);
    SetFetchFailOnUnexpected(false);

    response_headers_.SetStatusAndReason(HttpStatus::kOK);
    response_headers_.Add(HttpAttributes::kContentType,
                          kContentTypePng.mime_type());
    SetFetchResponse("http://test.com/test.png", response_headers_,
                     kFakePngInput);
    response_headers_.Remove(HttpAttributes::kContentType,
                             kContentTypePng.mime_type());

    response_headers_.SetStatusAndReason(HttpStatus::kNotFound);
    response_headers_.Add(HttpAttributes::kContentType,
                          kContentTypeText.mime_type());
    SetFetchResponse("http://test.com/404.html", response_headers_,
                     kHtmlInput);

    response_headers_.SetStatusAndReason(HttpStatus::kOK);
    response_headers_.SetDateAndCaching(MockTimer::kApr_5_2010_ms,
                                       1 * Timer::kSecondMs);
    response_headers_.ComputeCaching();
    SetFetchResponse("http://test.com/plain.html", response_headers_,
                     kHtmlInput);

    SetFetchResponse("http://test.com/blacklist.html", response_headers_,
                     kHtmlInput);

    response_headers_.Replace(HttpAttributes::kContentType,
                              "text/html; charset=utf-8");
    response_headers_.Add(HttpAttributes::kSetCookie, "helo=world; path=/");
    SetFetchResponse("http://test.com/text.html", response_headers_,
                     kHtmlInput);
    SetFetchResponse("http://test.com/minifiable_text.html", response_headers_,
                     kHtmlInputWithMinifiableJs);
    SetFetchResponse("http://test.com/smalltest.html", response_headers_,
                     kSmallHtmlInput);
    SetFetchResponse("http://test.com/noblink_text.html", response_headers_,
                     kHtmlInputForNoBlink);
    SetFetchResponse("https://test.com/noblink_text.html", response_headers_,
                     kHtmlInputForNoBlink);
    SetFetchResponse("http://test.com/cache.html", response_headers_,
                     kHtmlInput);
    SetFetchResponse("http://test.com/non_html.html", response_headers_,
                     kFakePngInput);
    SetFetchResponse("http://test.com/ws_text.html", response_headers_,
                     StrCat(kWhitespace, kHtmlInput));
    SetFetchResponse("http://test.com/flush_subresources.html",
                     response_headers_, kFlushSubresourcesHtmlInput);
    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.css"), kContentTypeCss,
                                  kCssContent, kHtmlCacheTimeSec * 2);
    AddFileToMockFetcher(StrCat(kTestDomain, "image1"), kSampleJpgFile,
                         kContentTypeJpeg, 100);
  }

  virtual void InitHasher() {
    UseMd5Hasher();
  }

  virtual RequestContextPtr CreateRequestContext() {
    return RequestContextPtr(test_request_context_);
  }

  void InitializeExperimentSpec() {
    options_->set_running_experiment(true);
    NullMessageHandler handler;
    ASSERT_TRUE(options_->AddExperimentSpec("id=3;percent=100;default",
                                            &handler));
  }

  void GetDefaultRequestHeaders(RequestHeaders* request_headers) {
    // Request from an internal ip.
    request_headers->Add(HttpAttributes::kUserAgent, kLinuxUserAgent);
    request_headers->Add(HttpAttributes::kXForwardedFor, "127.0.0.1");
    request_headers->Add(HttpAttributes::kXGoogleRequestEventId,
                         "1345815119391831");
  }

  void FetchFromProxyWaitForBackground(const StringPiece& url,
                                       bool expect_success,
                                       GoogleString* string_out,
                                       ResponseHeaders* headers_out) {
    FetchFromProxy(url, expect_success, string_out, headers_out, true);
  }
  void FetchFromProxyWaitForBackground(const StringPiece& url,
                                       bool expect_success,
                                       const RequestHeaders& request_headers,
                                       GoogleString* string_out,
                                       ResponseHeaders* headers_out,
                                       GoogleString* user_agent_out,
                                       bool wait_for_background_computation) {
  FetchFromProxy(url, expect_success, request_headers, string_out,
                 headers_out, user_agent_out,
                 wait_for_background_computation);
  }

  void VerifyNonCacheHtmlResponse(const ResponseHeaders& response_headers) {
    ConstStringStarVector values;
    EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kCacheControl,
                                         &values));
    EXPECT_EQ(2, values.size());
    EXPECT_STREQ("max-age=0", *(values[0]));
    EXPECT_STREQ("no-cache", *(values[1]));
  }

  void VerifyCacheHtmlResponse(const ResponseHeaders& response_headers) {
    EXPECT_STREQ("OK", response_headers.reason_phrase());
    EXPECT_STREQ(start_time_string_,
                 response_headers.Lookup1(HttpAttributes::kDate));
    ConstStringStarVector v;
    EXPECT_STREQ("text/html; charset=utf-8",
                 response_headers.Lookup1(HttpAttributes::kContentType));
    EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kCacheControl, &v));
    EXPECT_EQ("max-age=0", *v[0]);
    EXPECT_EQ("private", *v[1]);
    EXPECT_EQ("no-cache", *v[2]);
  }

  void FetchFromProxyNoWaitForBackground(const StringPiece& url,
                                         bool expect_success,
                                         GoogleString* string_out,
                                         ResponseHeaders* headers_out) {
    FetchFromProxy(url, expect_success, string_out, headers_out, false);
  }

  void FetchFromProxy(const StringPiece& url,
                      bool expect_success,
                      GoogleString* string_out,
                      ResponseHeaders* headers_out,
                      bool wait_for_background_computation) {
    RequestHeaders request_headers;
    GetDefaultRequestHeaders(&request_headers);
    FetchFromProxy(url, expect_success, request_headers,
                   string_out, headers_out, wait_for_background_computation);
  }

  void FetchFromProxy(const StringPiece& url,
                      bool expect_success,
                      const RequestHeaders& request_headers,
                      GoogleString* string_out,
                      ResponseHeaders* headers_out,
                      bool wait_for_background_computation) {
    FetchFromProxy(url, expect_success, request_headers,
                   string_out, headers_out, NULL,
                   wait_for_background_computation);
  }

  void FetchFromProxy(const StringPiece& url,
                        bool expect_success,
                        const RequestHeaders& request_headers,
                        GoogleString* string_out,
                        ResponseHeaders* headers_out,
                        GoogleString* user_agent_out,
                        bool wait_for_background_computation) {
    FetchFromProxy(url,
                   expect_success,
                   request_headers,
                   string_out,
                   headers_out,
                   NULL,
                   wait_for_background_computation,
                   true);
  }

  void FetchFromProxy(const StringPiece& url,
                      bool expect_success,
                      const RequestHeaders& request_headers,
                      GoogleString* string_out,
                      ResponseHeaders* headers_out,
                      GoogleString* user_agent_out,
                      bool wait_for_background_computation,
                      bool proxy_fetch_property_callback_collector_created) {
    FetchFromProxyNoQuiescence(url, expect_success, request_headers,
                               string_out, headers_out, user_agent_out);
    if (proxy_fetch_property_callback_collector_created) {
      ThreadSynchronizer* thread_synchronizer =
          server_context()->thread_synchronizer();
      thread_synchronizer->Wait(ProxyFetch::kCollectorFinish);
    }
    if (wait_for_background_computation) {
      ThreadSynchronizer* sync = server_context()->thread_synchronizer();
      sync->Wait(CacheHtmlFlow::kBackgroundComputationDone);
    }
  }

  void FetchFromProxyNoQuiescence(const StringPiece& url,
                                  bool expect_success,
                                  const RequestHeaders& request_headers,
                                  GoogleString* string_out,
                                  ResponseHeaders* headers_out) {
    FetchFromProxyNoQuiescence(url, expect_success, request_headers,
                               string_out, headers_out, NULL);
  }

  void FetchFromProxyNoQuiescence(const StringPiece& url,
                                  bool expect_success,
                                  const RequestHeaders& request_headers,
                                  GoogleString* string_out,
                                  ResponseHeaders* headers_out,
                                  GoogleString* user_agent_out) {
    WorkerTestBase::SyncPoint sync(server_context()->thread_system());
    AsyncExpectStringAsyncFetch callback(
        expect_success, &sync, rewrite_driver()->request_context());
    callback.set_response_headers(headers_out);
    callback.request_headers()->CopyFrom(request_headers);
    proxy_interface_->Fetch(AbsolutifyUrl(url), message_handler(), &callback);
    CHECK(server_context()->thread_synchronizer() != NULL);
    sync.Wait();
    EXPECT_TRUE(callback.done());

    *string_out = callback.buffer();
    if (user_agent_out != NULL &&
        callback.request_headers()->Lookup1(HttpAttributes::kUserAgent)
        != NULL) {
      user_agent_out->assign(
          callback.request_headers()->Lookup1(HttpAttributes::kUserAgent));
    }
  }

  void FetchFromProxyWithDelayCache(
      const StringPiece& url, bool expect_success,
      const RequestHeaders& request_headers,
      ProxyInterfaceWithDelayCache* proxy_interface,
      GoogleString* string_out,
      ResponseHeaders* headers_out) {
    WorkerTestBase::SyncPoint sync(server_context()->thread_system());
    AsyncExpectStringAsyncFetch callback(
        expect_success, &sync, rewrite_driver()->request_context());
    callback.set_response_headers(headers_out);
    callback.request_headers()->CopyFrom(request_headers);
    proxy_interface->Fetch(AbsolutifyUrl(url), message_handler(), &callback);
    CHECK(server_context()->thread_synchronizer() != NULL);
    delay_cache()->ReleaseKey(proxy_interface->key());
    sync.Wait();
    EXPECT_TRUE(callback.done());
    *string_out = callback.buffer();
    ThreadSynchronizer* ts = server_context()->thread_synchronizer();
    ts->Wait(CacheHtmlFlow::kBackgroundComputationDone);
    mock_scheduler()->AwaitQuiescence();
  }

  void CheckHeaders(const ResponseHeaders& headers,
                    const ContentType& expect_type) {
    ASSERT_TRUE(headers.has_status_code());
    EXPECT_EQ(HttpStatus::kOK, headers.status_code());
    EXPECT_STREQ(expect_type.mime_type(),
                 headers.Lookup1(HttpAttributes::kContentType));
  }

  // Verifies the fields of CacheHtmlFlow Info proto being logged.
  CacheHtmlLoggingInfo* VerifyCacheHtmlLoggingInfo(
      int cache_html_request_flow, const char* url) {
    CacheHtmlLoggingInfo* cache_html_logging_info =
        cache_html_logging_info_.mutable_cache_html_logging_info();
    EXPECT_EQ(cache_html_request_flow,
              cache_html_logging_info->cache_html_request_flow());
    EXPECT_EQ("1345815119391831",
              cache_html_logging_info->request_event_id_time_usec());
    EXPECT_STREQ(url, cache_html_logging_info->url());
    return cache_html_logging_info;
  }

  CacheHtmlLoggingInfo* VerifyCacheHtmlLoggingInfo(
      int cache_html_request_flow, bool html_match, const char* url) {
    CacheHtmlLoggingInfo* cache_html_logging_info =
        VerifyCacheHtmlLoggingInfo(cache_html_request_flow, url);
    EXPECT_EQ(html_match, cache_html_logging_info->html_match());
    return cache_html_logging_info;
  }

  void VerifyBlacklistUserAgent(const ResponseHeaders& response_headers) {
    ConstStringStarVector v;
    EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kCacheControl, &v));
    EXPECT_STREQ("text/plain",
                 response_headers.Lookup1(HttpAttributes::kContentType));
    EXPECT_STREQ("max-age=1", *v[0]);
  }

  void VerifyFlushSubresourcesResponse(GoogleString text,
                                       bool is_applied_expected) {
    // If FlushSubresources Filter is applied then the response has
    // rel="subresource".
    bool is_applied = false;
    const char pattern[] = "rel=\"stylesheet\"";
    int pattern_position = text.find(pattern);
    if (pattern_position != GoogleString::npos) {
      is_applied = true;
    }
    EXPECT_EQ(is_applied_expected, is_applied);
  }

  void UnEscapeString(GoogleString* str) {
    GlobalReplaceSubstring("__psa_lt;", "<", str);
    GlobalReplaceSubstring("__psa_gt;", ">", str);
  }

  // TODO(nikhilmadan): This is super fragile as RewriteTestBase also has
  // an options_ member.
  scoped_ptr<RewriteOptions> options_;
  GoogleString start_time_string_;

  void SetFetchHtmlResponseWithStatus(const char* url,
                                      HttpStatus::Code status) {
    ResponseHeaders response_headers;
    response_headers.SetStatusAndReason(status);
    response_headers.Add(HttpAttributes::kContentType, "text/html");
    SetFetchResponse(url, response_headers, kHtmlInput);
  }

  void CheckStats(int diff_matches, int diff_mismatches,
                  int smart_diff_matches, int smart_diff_mismatches,
                  int hits, int misses) {
    EXPECT_EQ(diff_matches, statistics()->FindVariable(
        CacheHtmlFlow::kNumCacheHtmlMatches)->Get());
    EXPECT_EQ(diff_mismatches, statistics()->FindVariable(
        CacheHtmlFlow::kNumCacheHtmlMismatches)->Get());
    EXPECT_EQ(smart_diff_matches, statistics()->FindVariable(
        CacheHtmlFlow::kNumCacheHtmlSmartdiffMatches)->Get());
    EXPECT_EQ(smart_diff_mismatches, statistics()->FindVariable(
        CacheHtmlFlow::kNumCacheHtmlSmartdiffMismatches)->Get());
    EXPECT_EQ(hits, statistics()->FindVariable(
        CacheHtmlFlow::kNumCacheHtmlHits)->Get());
    EXPECT_EQ(misses, statistics()->FindVariable(
        CacheHtmlFlow::kNumCacheHtmlMisses)->Get());
  }

  void TestCacheHtmlChangeDetection(bool use_smart_diff) {
    options_->ClearSignatureForTesting();
    options_->set_enable_blink_html_change_detection(true);
    server_context()->ComputeSignature(options_.get());
    GoogleString text;
    ResponseHeaders response_headers;

    // Hashes not set. Results in mismatches.
    FetchFromProxyWaitForBackground("text.html", true, &text,
                                    &response_headers);
    VerifyCacheHtmlLoggingInfo(
        CacheHtmlLoggingInfo::CACHE_HTML_MISS_TRIGGERED_REWRITE, false,
        "http://test.com/text.html");
    // Diff Match: 0, Diff Mismatch: 0,
    // Smart Diff Match: 0, Smart Diff Mismatch: 0
    // Hits: 0, Misses: 1
    CheckStats(0, 0, 0, 0, 0, 1);
    ClearStats();
    response_headers.Clear();
    // Hashes set. No mismatches.
    FetchFromProxyWaitForBackground("text.html", true, &text,
                                    &response_headers);
    // Diff Match: 1, Diff Mismatch: 0,
    // Smart Diff Match: 1, Smart Diff Mismatch: 0
    // Hits: 1, Misses: 0
    CheckStats(1, 0, 1, 0, 1, 0);
    VerifyCacheHtmlResponse(response_headers);
    UnEscapeString(&text);
    EXPECT_STREQ(blink_output_, text);
    VerifyCacheHtmlLoggingInfo(CacheHtmlLoggingInfo::CACHE_HTML_HIT, true,
                               "http://test.com/text.html");
    ClearStats();
    response_headers.Clear();

    // Input with an extra comment. We strip out comments before taking hash,
    // so there should be no mismatches.
    SetFetchResponse(kTestUrl, response_headers_,
                     kHtmlInputWithExtraCommentAndNonCacheable);
    FetchFromProxyWaitForBackground("text.html", true, &text,
                                    &response_headers);
    // Diff Match: 1, Diff Mismatch: 0,
    // Smart Diff Match: 1, Smart Diff Mismatch: 0
    // Hits: 1, Misses: 0
    CheckStats(1, 0, 1, 0, 1, 0);
    VerifyCacheHtmlResponse(response_headers);
    UnEscapeString(&text);
    EXPECT_STREQ(blink_output_, text);
    VerifyCacheHtmlLoggingInfo(CacheHtmlLoggingInfo::CACHE_HTML_HIT, true,
                               "http://test.com/text.html");
    ClearStats();
    response_headers.Clear();

    // Input with extra attributes. This should result in a mismatch with
    // full-diff but a match with smart-diff.
    SetFetchResponse(kTestUrl, response_headers_,
                     kHtmlInputWithExtraAttribute);
    FetchFromProxyWaitForBackground("text.html", true, &text,
                                    &response_headers);
    VerifyCacheHtmlLoggingInfo(CacheHtmlLoggingInfo::CACHE_HTML_HIT, false,
                               "http://test.com/text.html");
    // Diff Match: 0, Diff Mismatch: 1,
    // Smart Diff Match: 1, Smart Diff Mismatch: 0
    // Hits: 1, Misses: 0
    CheckStats(0, 1, 1, 0, 1, 0);
    ClearStats();

    // Input with empty visible portions. Diff calculation should not trigger.
    SetFetchResponse(kTestUrl, response_headers_,
                     kHtmlInputWithEmptyVisiblePortions);
    FetchFromProxyWaitForBackground("text.html", true, &text,
                                    &response_headers);
    // Diff Match: 0, Diff Mismatch: 1,
    // Smart Diff Match: 0, Smart Diff Mismatch: 1
    // Hits: 1, Misses: 0
    CheckStats(0, 1, 0, 1, 1, 0);
  }

  GoogleString GetImageOnloadScriptBlock() const {
    return StrCat("<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
                  DelayImagesFilter::kImageOnloadJsSnippet,
                  "</script>");
  }

  LoggingInfo cache_html_logging_info_;
  ResponseHeaders response_headers_;
  GoogleString noblink_output_;
  GoogleString noblink_output_with_lazy_load_;
  GoogleString blink_output_with_lazy_load_;
  GoogleString blink_output_partial_;
  GoogleString blink_output_;
  GoogleString blink_output_with_cacheable_panels_cookies_;
  GoogleString blink_output_with_cacheable_panels_no_cookies_;
  MeaningfulFlushEarlyInfoFinder* flush_early_info_finder_;
  TestRequestContextPtr test_request_context_;
  MockCriticalCssFinder* critical_css_finder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheHtmlFlowTest);
};

TEST_F(CacheHtmlFlowTest, TestCacheHtmlCacheMiss) {
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyWaitForBackground("minifiable_text.html", true, &text,
                                  &response_headers);
  ConstStringStarVector values;
  EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kSetCookie, &values));
  EXPECT_EQ(1, values.size());
  VerifyNonCacheHtmlResponse(response_headers);
  VerifyCacheHtmlLoggingInfo(
      CacheHtmlLoggingInfo::CACHE_HTML_MISS_TRIGGERED_REWRITE, false,
      "http://test.com/minifiable_text.html");
  EXPECT_STREQ(StringPrintf(
      kHtmlInputWithMinifiedJs, GetJsDisableScriptSnippet(
          options_.get()).c_str()), text);
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlCacheMissAndHit) {
  GoogleString text;
  ResponseHeaders response_headers;
  // First request updates the property cache with cached html.
  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);
  VerifyNonCacheHtmlResponse(response_headers);
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());
  VerifyCacheHtmlLoggingInfo(
      CacheHtmlLoggingInfo::CACHE_HTML_MISS_TRIGGERED_REWRITE, false,
      "http://test.com/text.html");
  CheckStats(0, 0, 0, 0, 0, 1);
  ClearStats();
  // Cache Html hit case.
  response_headers.Clear();
  FetchFromProxyNoWaitForBackground("text.html", true, &text,
                                    &response_headers);
  VerifyCacheHtmlLoggingInfo(CacheHtmlLoggingInfo::CACHE_HTML_HIT, false,
                             "http://test.com/text.html");
  CheckStats(0, 0, 0, 0, 1, 0);
  ClearStats();
  VerifyCacheHtmlResponse(response_headers);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_, text);
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlChangeDetection) {
  TestCacheHtmlChangeDetection(false);
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlChangeDetectionWithSmartDiffOn) {
  TestCacheHtmlChangeDetection(true);
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlMissExperimentSetCookie) {
  options_->ClearSignatureForTesting();
  options_->set_experiment_cookie_duration_ms(1000);
  SetTimeMs(MockTimer::kApr_5_2010_ms);
  InitializeExperimentSpec();
  server_context()->ComputeSignature(options_.get());
  GoogleString text;
  ResponseHeaders response_headers;

  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);

  ConstStringStarVector values;
  EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kSetCookie, &values));
  ASSERT_EQ(2, values.size());
  EXPECT_STREQ("PageSpeedExperiment=3", (*(values[1])).substr(0, 21));
  GoogleString expires_str;
  ConvertTimeToString(MockTimer::kApr_5_2010_ms + 1000, &expires_str);
  EXPECT_NE(GoogleString::npos, ((*(values[1])).find(expires_str)));
  VerifyNonCacheHtmlResponse(response_headers);
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlHitExperimentSetCookie) {
  options_->ClearSignatureForTesting();
  InitializeExperimentSpec();
  server_context()->ComputeSignature(options_.get());
  GoogleString text;
  ResponseHeaders response_headers;

  // Populate the property cache in first request.
  FetchFromProxyWaitForBackground("text.html", true, &text,
                                  &response_headers);

  response_headers.Clear();
  FetchFromProxyNoWaitForBackground("text.html", true, &text,
                                    &response_headers);

  ConstStringStarVector values;
  EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kSetCookie, &values));
  EXPECT_EQ(1, values.size());
  EXPECT_STREQ("PageSpeedExperiment=3", (*(values[0])).substr(0, 21));
  VerifyCacheHtmlResponse(response_headers);
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlExperimentCookieHandling) {
  options_->ClearSignatureForTesting();
  InitializeExperimentSpec();
  server_context()->ComputeSignature(options_.get());
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  GetDefaultRequestHeaders(&request_headers);
  request_headers.Add(HttpAttributes::kCookie, "PageSpeedExperiment=3");

  // Populate the property cache in first request.
  FetchFromProxyWaitForBackground("text.html", true, &text,
                                  &response_headers);

  response_headers.Clear();
  FetchFromProxy("text.html", true, request_headers,
                 &text, &response_headers, false);

  EXPECT_FALSE(response_headers.Has(HttpAttributes::kSetCookie));
  VerifyCacheHtmlResponse(response_headers);
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlCacheHitWithInlinePreviewImages) {
  const char kInlinePreviewHtmlInput[] =
      "<html>"
      "<head>"
      "</head>"
      "<body>\n"
      "<div id=\"header\"> This is the header </div>"
      "<div id=\"container\" class>"
        "<h2 id=\"beforeItems\"> This is before Items </h2>"
        "<div class=\"item1\">"
           "<img src=\"image1\">"
           "<img src=\"image2\">"
        "</div>"
        "<div class=\"item\">"
           "<img src=\"image3\">"
            "<div class=\"item\">"
               "<img src=\"image4\">"
            "</div>"
        "</div>"
      "</div>"
      "</body></html>";
  SetFetchResponse("http://test.com/text.html", response_headers_,
                   kInlinePreviewHtmlInput);

  StringSet* critical_images = new StringSet;
  critical_images->insert(StrCat(kTestDomain, "image1"));
  SetCriticalImagesInFinder(critical_images);
  options_->ClearSignatureForTesting();
  options_->EnableFilter(RewriteOptions::kDelayImages);
  server_context()->ComputeSignature(options_.get());

  GoogleString text;
  ResponseHeaders response_headers;
  // First request updates the property cache with cached html.
  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);
  VerifyNonCacheHtmlResponse(response_headers);
  EXPECT_EQ(-1, logging_info()->num_html_critical_images());
  EXPECT_EQ(-1, logging_info()->num_css_critical_images());
  // Cache Html hit case.
  response_headers.Clear();
  FetchFromProxyNoWaitForBackground("text.html", true, &text,
                                    &response_headers);

  VerifyCacheHtmlResponse(response_headers);
  UnEscapeString(&text);

  const char kBlinkOutputWithInlinePreviewImages[] =
      "<html><head></head><body>"
      "<noscript><meta HTTP-EQUIV=\"refresh\" content=\"0;"
      "url='%s?PageSpeed=noscript'\" />"
      "<style><!--table,div,span,font,p{display:none} --></style>"
      "<div style=\"display:block\">Please click "
      "<a href=\"%s?PageSpeed=noscript\">here</a> "
      "if you are not redirected within a few seconds.</div></noscript>"
      "\n<div id=\"header\"> This is the header </div>"
      "<div id=\"container\" class>"
      "<!--GooglePanel begin panel-id-1.0-->"
      "<!--GooglePanel end panel-id-1.0-->"
      "<div class=\"item1\">%s"  // Inlined Image tag with script.
      "%s"  // image-onload js snippet.
      "<img src=\"image2\">"
      "</div>"
      "<!--GooglePanel begin panel-id-0.0-->"
      "<!--GooglePanel end panel-id-0.0-->"
      "</div>"
      "</body></html>"
      "%s<script type=\"text/javascript\" src=\"/psajs/blink.0.js\"></script>"
      "<script type=\"text/javascript\">"
      "pagespeed.panelLoaderInit();</script>\n"
      "<script type=\"text/javascript\">"
      "pagespeed.panelLoader.setRequestFromInternalIp();</script>\n"
      "%s"  // kCookieScript
      "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-1.0\":{\"instance_html\":\"<h2 id=\\\"beforeItems\\\"> This is before Items </h2>\",\"xpath\":\"//div[@id=\\\"container\\\"]/h2[1]\"}}\n);</script>"  // NOLINT
      "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.0\":{\"instance_html\":\"<div class=\\\"item\\\"><img src=\\\"image3\\\"><div class=\\\"item\\\"><img src=\\\"image4\\\"></div></div>\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[3]\"}}\n);</script>"  // NOLINT
      "<script>pagespeed.panelLoader.bufferNonCriticalData({});</script>";  // NOLINT

  GoogleString inlined_image_wildcard =
      StringPrintf(kBlinkOutputWithInlinePreviewImages, kTestUrl, kTestUrl,
                  GetImageOnloadScriptBlock().c_str(),
                   "<img pagespeed_high_res_src=\"image1\" "
                   "src=\"data:image/jpeg;base64*",
                   GetJsDisableScriptSnippet(options_.get()).c_str(),
                   kCookieScript);
  EXPECT_TRUE(Wildcard(inlined_image_wildcard).Match(text))
      << "Expected:\n" << inlined_image_wildcard << "\n\nGot:\n" << text;
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlOverThreshold) {
  options_->ClearSignatureForTesting();
  // Content type is more than the limit to buffer in secondary fetch.
  int64 size_of_small_html = arraysize(kSmallHtmlInput) - 1;
  int64 html_buffer_threshold = size_of_small_html - 1;
  options_->ClearSignatureForTesting();
  options_->set_blink_max_html_size_rewritable(html_buffer_threshold);
  server_context()->ComputeSignature(options_.get());
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyWaitForBackground(
      "smalltest.html", true, &text, &response_headers);

  GoogleString SmallHtmlOutput =
      StrCat("<html><head></head><body>A small test html.",
             GetJsDisableScriptSnippet(options_.get()),
             "<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\">"
             "</script></body></html>");
  EXPECT_STREQ(SmallHtmlOutput, text);
  VerifyCacheHtmlLoggingInfo(
      CacheHtmlLoggingInfo::FOUND_CONTENT_LENGTH_OVER_THRESHOLD,
      "http://test.com/smalltest.html");
  // 1 Miss for original plain text,
  // 1 Miss for Blink Cohort.
  EXPECT_EQ(2, lru_cache()->num_misses());

  CheckStats(0, 0, 0, 0, 0, 1);
  ClearStats();
  text.clear();
  response_headers.Clear();
  options_->ClearSignatureForTesting();
  html_buffer_threshold = size_of_small_html + 1;
  options_->set_blink_max_html_size_rewritable(html_buffer_threshold);
  server_context()->ComputeSignature(options_.get());

  FetchFromProxyWaitForBackground(
      "smalltest.html", true, &text, &response_headers);
  EXPECT_EQ(2, lru_cache()->num_misses());

  CheckStats(0, 0, 0, 0, 0, 1);
  ClearStats();
  text.clear();
  response_headers.Clear();
  options_->ClearSignatureForTesting();
  FetchFromProxyNoWaitForBackground(
      "smalltest.html", true, &text, &response_headers);
  CheckStats(0, 0, 0, 0, 1, 0);
  EXPECT_EQ(1, lru_cache()->num_misses());
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlHeaderOverThreshold) {
  options_->ClearSignatureForTesting();
  InitializeExperimentSpec();
  int64 size_of_small_html = arraysize(kSmallHtmlInput) - 1;
  int64 html_buffer_threshold = size_of_small_html;
  options_->ClearSignatureForTesting();
  options_->set_blink_max_html_size_rewritable(html_buffer_threshold);
  server_context()->ComputeSignature(options_.get());

  GoogleString text;
  ResponseHeaders response_headers;
  // Setting a higher content length to verify if the header's content length
  // is checked before rewriting.
  response_headers.Add(HttpAttributes::kContentLength,
                       IntegerToString(size_of_small_html + 1));
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  response_headers.Add(HttpAttributes::kContentType,
                       "text/html; charset=utf-8");
  SetFetchResponse("http://test.com/smalltest.html", response_headers,
                   kSmallHtmlInput);
  FetchFromProxyWaitForBackground(
      "smalltest.html", true, &text, &response_headers);

  VerifyCacheHtmlLoggingInfo(
      CacheHtmlLoggingInfo::FOUND_CONTENT_LENGTH_OVER_THRESHOLD,
      "http://test.com/smalltest.html");
  // 1 Miss for original plain text,
  // 1 Miss for Blink Cohort.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());
}

TEST_F(CacheHtmlFlowTest, Non200StatusCode) {
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyWaitForBackground("404.html", true, &text, &response_headers);
  EXPECT_STREQ(kHtmlInput, text);
  EXPECT_STREQ("text/plain",
               response_headers.Lookup1(HttpAttributes::kContentType));
  VerifyCacheHtmlLoggingInfo(CacheHtmlLoggingInfo::CACHE_HTML_MISS_FETCH_NON_OK,
                             "http://test.com/404.html");
  // 1 Miss for original plain text,
  // 1 Miss for Blink Cohort.
  EXPECT_EQ(2, lru_cache()->num_misses());
  CheckStats(0, 0, 0, 0, 0, 1);
}

TEST_F(CacheHtmlFlowTest, NonHtmlContent) {
  // Content type is non html.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyNoWaitForBackground(
      "plain.html", true, &text, &response_headers);

  EXPECT_STREQ(kHtmlInput, text);
  EXPECT_STREQ("text/plain",
               response_headers.Lookup1(HttpAttributes::kContentType));
  VerifyCacheHtmlLoggingInfo(
      CacheHtmlLoggingInfo::CACHE_HTML_MISS_FOUND_RESOURCE,
      "http://test.com/plain.html");
  // 1 Miss for Blink Cohort.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_inserts());

  CheckStats(0, 0, 0, 0, 0, 1);
  ClearStats();
  text.clear();
  response_headers.Clear();

  FetchFromProxyNoWaitForBackground(
      "plain.html", true, &text, &response_headers);
  VerifyCacheHtmlLoggingInfo(
      CacheHtmlLoggingInfo::CACHE_HTML_MISS_FOUND_RESOURCE,
      "http://test.com/plain.html");
  // 1 Miss for Blink Cohort.
  CheckStats(0, 0, 0, 0, 0, 1);
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_inserts());

  // Content type is html but the actual content is non html.
  FetchFromProxyNoWaitForBackground(
      "non_html.html", true, &text, &response_headers);
  FetchFromProxyWaitForBackground(
      "non_html.html", true, &text, &response_headers);
  VerifyCacheHtmlLoggingInfo(
      CacheHtmlLoggingInfo::CACHE_HTML_MISS_FOUND_RESOURCE,
      "http://test.com/non_html.html");
  CheckStats(0, 0, 0, 0, 0, 3);
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlWithWebp) {
  if (RunningOnValgrind()) {
    return;
  }
  rewrite_driver_->server_context()->set_hasher(factory_->mock_hasher());
  AddFileToMockFetcher(StrCat(kTestDomain, "image1"), "Puzzle.jpg",
                       kContentTypeJpeg, 100);
  options_->ClearSignatureForTesting();
  options_->EnableFilter(RewriteOptions::kConvertJpegToWebp);
  server_context()->ComputeSignature(options_.get());
  GoogleString text;
  ResponseHeaders response_headers;
  // First request updates the property cache with cached html.
  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);
  VerifyNonCacheHtmlResponse(response_headers);
  ClearStats();
  // Cache Html hit case.
  response_headers.Clear();
  FetchFromProxyNoWaitForBackground("text.html", true, &text,
                                    &response_headers);
  ClearStats();
  VerifyCacheHtmlResponse(response_headers);
  UnEscapeString(&text);
  GoogleString correct_url =
      Encode("", RewriteOptions::kImageCompressionId, "0", "image1", "webp");

  GoogleString blink_output_with_webp =
      StrCat(blink_output_partial_, kCookieScript,
             StringPrintf(kBlinkOutputSuffix, correct_url.c_str()));
  EXPECT_STREQ(blink_output_with_webp, text);
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlWithHttpsUrl) {
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  GetDefaultRequestHeaders(&request_headers);

  FetchFromProxy("https://test.com/noblink_text.html", true, request_headers,
                 &text, &response_headers, false);
  EXPECT_STREQ(
      StrCat("<html><head></head><body>",
             GetJsDisableScriptSnippet(options_.get()),
             "<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\">"
             "</script></body></html>"), text);
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlWithWhitespace) {
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyWaitForBackground(
      "ws_text.html", true, &text, &response_headers);
  EXPECT_EQ(0, statistics()->FindVariable(
      CacheHtmlFlow::kNumCacheHtmlHits)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      CacheHtmlFlow::kNumCacheHtmlMisses)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlFlushSubresources) {
  // FlushSubresources is applied when blink is enabled and user agent
  // does not support blink.
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_rel_subresource");
  ResponseHeaders response_headers;
  FetchFromProxy("http://test.com/flush_subresources.html"
                 "?PageSpeedFilters=+extend_cache_css,-inline_css", true,
                 request_headers, &text, &response_headers, NULL, false);
  VerifyNonCacheHtmlResponse(response_headers);
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());

  // Requesting again.
  flush_early_info_finder_->Clear();
  response_headers.Clear();
  FetchFromProxy("http://test.com/flush_subresources.html"
                 "?PageSpeedFilters=+extend_cache_css,-inline_css", true,
                 request_headers, &text, &response_headers, NULL, false);
  VerifyFlushSubresourcesResponse(text, true);
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlFlowUrlCacheInvalidation) {
  GoogleString text;
  ResponseHeaders response_headers;
  GoogleString htmlOutput = StrCat(
    "<html><head></head>"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
    "</div>",
    GetJsDisableScriptSnippet(options_.get()),
    "<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\"></script>"
    "</body></html>");

  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);
  EXPECT_STREQ(htmlOutput, text);

  // Cache lookup for original plain text and Blink Cohort
  // all miss.
  // ie., 1 + 1 (Blink Cohort).
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  response_headers.Clear();
  ClearStats();

  // Property cache hit.
  FetchFromProxyNoWaitForBackground(
      "text.html", true, &text, &response_headers);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_, text);
  // 1 Miss for original plain text
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  ClearStats();

  // Invalidate the cache for some URL other than 'text.html'.
  options_->ClearSignatureForTesting();
  options_->AddUrlCacheInvalidationEntry(
      AbsolutifyUrl("foo.bar"), timer()->NowMs(), true);
  server_context()->ComputeSignature(options_.get());

  // Property cache hit.
  FetchFromProxyNoWaitForBackground(
      "text.html", true, &text, &response_headers);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_, text);
  // 1 Miss for original plain text
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  ClearStats();

  // Invalidate the cache.
  options_->ClearSignatureForTesting();
  options_->AddUrlCacheInvalidationEntry(
      AbsolutifyUrl("text.html"), timer()->NowMs(), true);
  server_context()->ComputeSignature(options_.get());

  // Property cache hit, but invalidated.  Hence treated as a miss and
  // passthrough by blink.
  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);

  EXPECT_STREQ(htmlOutput, text);
  // 1 Miss for original plain text
  EXPECT_EQ(1, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());
  EXPECT_EQ(1, lru_cache()->num_identical_reinserts());  // identical insert
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlFlowWithHeadRequest) {
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kUserAgent, kLinuxUserAgent);
  request_headers.set_method(RequestHeaders::kHead);
  FetchFromProxy("text.html", true, request_headers,
                 &text, &response_headers, false);
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlFlowDataMissDelayCache) {
  GoogleString text;
  ResponseHeaders response_headers;
  ProxyInterfaceWithDelayCache* proxy_interface =
      new ProxyInterfaceWithDelayCache("localhost", 80,
                                       server_context(), statistics(),
                                       delay_cache(), factory());
  proxy_interface_.reset(proxy_interface);
  RequestHeaders request_headers;
  GetDefaultRequestHeaders(&request_headers);
  FetchFromProxyWithDelayCache(
      "text.html", true, request_headers, proxy_interface,
      &text, &response_headers);
  GoogleString htmlOutput = StrCat(
    "<html><head></head>"
    "<body>\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"item\">"
         "<img src=\"image1\">"
         "<img src=\"image2\">"
      "</div>"
      "<div class=\"item\">"
         "<img src=\"image3\">"
          "<div class=\"item\">"
             "<img src=\"image4\">"
          "</div>"
      "</div>"
    "</div>",
    GetJsDisableScriptSnippet(options_.get()),
    "<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\"></script>"
    "</body></html>");

  EXPECT_STREQ(htmlOutput, text);

  EXPECT_STREQ("text/html; charset=utf-8",
               response_headers.Lookup1(HttpAttributes::kContentType));

  // 1 Miss for original plain text,
  // 1 miss for BlinkCohort
  VerifyNonCacheHtmlResponse(response_headers);
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());
}

TEST_F(CacheHtmlFlowTest, TestCacheHtmlFlowWithDifferentUserAgents) {
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;

  // Blacklisted User Agent.
  request_headers.Add(HttpAttributes::kUserAgent, kBlackListUserAgent);
  FetchFromProxy("blacklist.html", true, request_headers, &text,
                 &response_headers, NULL, false, false);
  EXPECT_STREQ(kHtmlInput, text);
  VerifyBlacklistUserAgent(response_headers);
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());
  ClearStats();

  // NULL User Agent.
  request_headers.Add(HttpAttributes::kUserAgent, NULL);
  FetchFromProxy("noblink_text.html", true, request_headers, &text,
                 &response_headers, false);
  EXPECT_STREQ(
      StrCat("<html><head>"
             "</head><body>",
             GetJsDisableScriptSnippet(options_.get()),
             "<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\">"
             "</script></body></html>"), text);
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());
  ClearStats();

  // Empty User Agent.
  request_headers.Replace(HttpAttributes::kUserAgent, "");
  FetchFromProxy("noblink_text.html", true, request_headers, &text,
                 &response_headers, false);
  EXPECT_STREQ(
      StrCat("<html><head>"
             "</head><body>",
             GetJsDisableScriptSnippet(options_.get()),
             "<script type=\"text/javascript\" src=\"/psajs/js_defer.0.js\">"
             "</script></body></html>"), text);
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());
  ClearStats();

  // Mobile User Agent.
  request_headers.Clear();
  request_headers.Add(
      HttpAttributes::kUserAgent,
      UserAgentMatcherTestBase::kIPhone4Safari);  // Mobile Request.
  request_headers.Add(HttpAttributes::kXForwardedFor, "127.0.0.1");

  FetchFromProxy("text.html", true, request_headers, &text, &response_headers,
                 true);
  VerifyNonCacheHtmlResponse(response_headers);
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kCacheHtmlRequestCount)->Get());

  ClearStats();
  // Hit case.
  response_headers.Clear();
  FetchFromProxy("text.html", true, request_headers, &text, &response_headers,
                 false);
  VerifyCacheHtmlResponse(response_headers);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_, text);
}

class CacheHtmlPrioritizeCriticalCssTest : public CacheHtmlFlowTest {
 public:
  virtual void SetUp() {
    CacheHtmlFlowTest::SetUp();
    SetOptions();
    InitializeResponses();
  }

  virtual void InitHasher() {}  // avoid UseMd5Hasher() in base class

  void SetOptions() {
    // Enable FlushSubresourcesFilter filter.
    options_->ClearSignatureForTesting();
    options_->EnableFilter(RewriteOptions::kCachePartialHtml);
    options_->EnableFilter(RewriteOptions::kPrioritizeCriticalCss);
    options_->DisableFilter(RewriteOptions::kRewriteJavascript);
    options_->set_non_cacheables_for_cache_partial_html(
        "class=item,id=beforeItems");
    options_->set_in_place_rewriting_enabled(true);
    options_->set_use_selectors_for_critical_css(false);
    options_->ComputeSignature();
  }

  void InitializeResponses() {
    // Some weird but valid CSS.
    SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                  "div,span,*::first-letter { display: block; }"
                                  "p { display: inline; }",
                                  kHtmlCacheTimeSec * 2);
    SetResponseWithDefaultHeaders("b.css?x=1&y=2", kContentTypeCss,
                                  "@media screen,print { * { margin: 0px; } }",
                                  kHtmlCacheTimeSec * 2);
  }

  GoogleString InputHtml() {
    return GoogleString(
        "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
        "<html><head>"
        "<title>Flush Subresources Early example</title>"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"a.css\">"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"b.css?x=1&y=2\">"
        "</head>"
        "<body>"
        "Hello, mod_pagespeed!"
        "</body></html>");
  }

  GoogleString ExpectedHtml(GoogleString full_styles_html) {
    GoogleString expected_html = StrCat(
        "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
        "<html><head>"
        "<title>Flush Subresources Early example</title>",
        "<style>div,*::first-letter{display:block}</style>"  // from a.css
        "<style>@media screen{*{margin:0px}}</style>"  // from b.css
        "</head>");
    StrAppend(&expected_html,
        "<body>",
        StringPrintf(kNoScriptRedirectFormatter,
                     kNoScriptTextUrl, kNoScriptTextUrl),
        "Hello, mod_pagespeed!",
        full_styles_html,
        "</body></html>");
    StrAppend(&expected_html,
        GetJsDisableScriptSnippet(options_.get()),
        "<script type=\"text/javascript\" src=\"/psajs/blink.0.js\"></script>"
        "<script type=\"text/javascript\">"
        "pagespeed.panelLoaderInit();</script>\n"
        "<script type=\"text/javascript\">"
        "pagespeed.panelLoader.setRequestFromInternalIp();</script>\n",
        kCookieScript,
        "<script>pagespeed.panelLoader.bufferNonCriticalData({});</script>");
    return expected_html;
  }

  GoogleString CssLinkEncodedHref(GoogleString url) {
    return StrCat(
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"",
        Encode("", "cf", kMockHashValue, url, "css"),
        "\">");
  }

  GoogleString url() { return kTestUrl; }

  void ValidateCacheHtml(const StringPiece& case_id,
                          const GoogleString& input_html,
                          const GoogleString& expected_html) {
    SetFetchResponse(url(), response_headers_, input_html);
    ResponseHeaders headers;
    GoogleString actual_html;
    // First request updates the property cache with cached html.
    FetchFromProxyWaitForBackground(
        url(), true /* expect_success */, &actual_html, &headers);
    VerifyNonCacheHtmlResponse(headers);
    headers.Clear();

    // Fetch the url again (with no wait) and expect a cache html hit.
    FetchFromProxyNoWaitForBackground(
        url(), true /* expect_success */, &actual_html, &headers);
    VerifyCacheHtmlResponse(headers);
    UnEscapeString(&actual_html);

    EXPECT_EQ(expected_html, actual_html) << "Test id:" << case_id;
  }
};

TEST_F(CacheHtmlPrioritizeCriticalCssTest, CacheHtmlWithCriticalCss) {
  // Add critical css rules.
  MockCriticalCssFinder* critical_css_finder =
      new MockCriticalCssFinder(rewrite_driver(), statistics());
  server_context()->set_critical_css_finder(critical_css_finder);
  critical_css_finder->AddCriticalCss(
      "http://test.com/a.css", "div,*::first-letter{display:block}", 100);
  critical_css_finder->AddCriticalCss(
      "http://test.com/b.css?x=1&y=2", "@media screen{*{margin:0px}}", 100);

  GoogleString full_styles_html = StrCat(
      "<noscript class=\"psa_add_styles\">"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"a.css\">"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"b.css?x=1&y=2\">"
      "</noscript>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      CriticalCssFilter::kAddStylesScript,
      "window['pagespeed'] = window['pagespeed'] || {};"
      "window['pagespeed']['criticalCss'] = {"
      "  'total_critical_inlined_size': 62,"
      "  'total_original_external_size': 200,"
      "  'total_overhead_size': 62,"
      "  'num_replaced_links': 2,"
      "  'num_unreplaced_links': 0};"
      "</script>");
  ValidateCacheHtml(
      "critical_css", InputHtml(), ExpectedHtml(full_styles_html));
}

class TestCriticalSelectorFinder : public CriticalSelectorFinder {
 public:
  TestCriticalSelectorFinder(const PropertyCache::Cohort* cohort,
                             Statistics* stats)
      : CriticalSelectorFinder(cohort, NULL /* nonce_generator */, stats) {}

  virtual ~TestCriticalSelectorFinder() {}

  virtual int SupportInterval() const { return 1; }

 protected:
  virtual bool ShouldReplacePriorResult() const { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCriticalSelectorFinder);
};

TEST_F(CacheHtmlPrioritizeCriticalCssTest, CacheHtmlWithCriticalSelectors) {
    SetMockHashValue("00000");  // Base64 encodes to kMockHashValue.
  server_context()->set_enable_property_cache(true);
  PropertyCache* pcache = server_context()->page_property_cache();
  const PropertyCache::Cohort* dom_cohort =
      SetupCohort(pcache, RewriteDriver::kDomCohort);
  const PropertyCache::Cohort* beacon_cohort =
      SetupCohort(pcache, RewriteDriver::kBeaconCohort);
  server_context()->set_dom_cohort(dom_cohort);
  server_context()->set_beacon_cohort(beacon_cohort);

  rewrite_driver()->Clear();
  rewrite_driver()->set_request_context(
      RequestContext::NewTestRequestContext(factory()->thread_system()));

  MockPropertyPage* page = NewMockPage(
      url(), kMockHashValue /* hash */, UserAgentMatcher::kDesktop);
  rewrite_driver()->set_property_page(page);
  pcache->Read(page);

  options_->ClearSignatureForTesting();
  options_->set_use_selectors_for_critical_css(true);
  options_->ComputeSignature();

  server_context()->set_critical_selector_finder(new TestCriticalSelectorFinder(
      server_context()->beacon_cohort(), statistics()));

  // Write critical selectors to property cache
  StringSet selectors;
  selectors.insert("div");
  selectors.insert("*");
  CriticalSelectorFinder* finder = server_context()->critical_selector_finder();
  finder->WriteCriticalSelectorsToPropertyCache(
      selectors, "" /* last_nonce */, rewrite_driver());
  rewrite_driver()->property_page()->
      WriteCohort(server_context()->beacon_cohort());

  EXPECT_TRUE(finder->IsCriticalSelector(rewrite_driver(), "div"));
  EXPECT_TRUE(finder->IsCriticalSelector(rewrite_driver(), "*"));

  GoogleString full_styles_html = StrCat(
      "<noscript class=\"psa_add_styles\">",
      // URLs are encoded because CSS rewrite is enabled with selectors filter.
      CssLinkEncodedHref("a.css"), CssLinkEncodedHref("b.css?x=1&y=2"),
      "</noscript>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">",
      rewrite_driver()->server_context()->static_asset_manager()->GetAsset(
          StaticAssetManager::kCriticalCssLoaderJs,
          rewrite_driver()->options()),
      "pagespeed.CriticalCssLoader.Run();</script>");
  ValidateCacheHtml(
      "critical_selector", InputHtml(), ExpectedHtml(full_styles_html));
}

}  // namespace net_instaweb
