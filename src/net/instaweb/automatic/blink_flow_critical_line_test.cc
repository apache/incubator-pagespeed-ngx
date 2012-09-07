/*
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

// Author: pulkitg@google.com (Pulkit Goyal)

// Unit-tests for BlinkFlowCriticalLine.

#include "net/instaweb/automatic/public/blink_flow_critical_line.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/automatic/public/proxy_interface.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher_test.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/blink_critical_line_data_finder.h"
#include "net/instaweb/rewriter/blink_critical_line_data.pb.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/delay_cache.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/mock_scheduler.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/null_message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_synchronizer.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {

class Function;
class MessageHandler;

namespace {

const char kLinuxUserAgent[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/536.5 "
    "(KHTML, like Gecko) Chrome/19.0.1084.46 Safari/536.5";

const char kWindowsUserAgent[] =
    "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:15.0) Gecko/20120427 "
    "Firefox/15.0a1";

const char kBlackListUserAgent[] =
    "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:15.0) Gecko/20120427 Firefox/2.0a1";
const char kNumPrepareRequestCalls[] = "num_prepare_request_calls";

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
    "</body></html>";

const char kLazyLoadHtml[] =
    "<html>"
    "<head>"
    "</head>"
    "<body>%s\n"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is before Items </h2>"
      "<div class=\"item\">%s"
         "<img pagespeed_lazy_src=\"image1\" src=\"data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH+A1BTQQAsAAAAAAEAAQAAAgJEAQA7\" onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\">"
         "<img pagespeed_lazy_src=\"image2\" src=\"data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH+A1BTQQAsAAAAAAEAAQAAAgJEAQA7\" onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\">"
         "</div>"
         "<div class=\"item\">"
           "<img pagespeed_lazy_src=\"image3\" src=\"data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH+A1BTQQAsAAAAAAEAAQAAAgJEAQA7\" onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\">"
           "<div class=\"item\">"
             "<img pagespeed_lazy_src=\"image4\" src=\"data:image/gif;base64,R0lGODlhAQABAIAAAP///////yH+A1BTQQAsAAAAAAEAAQAAAgJEAQA7\" onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\">"
          "</div>"
      "</div>"
    "</body></html>";

const char kHtmlInputWithExtraCommentAndNonCacheable[] =
    "<html>"
    "<head>"
    "</head>"
    "<body>\n"
    "<!-- Hello -->"
    "<div id=\"header\"> This is the header </div>"
    "<div id=\"container\" class>"
      "<h2 id=\"beforeItems\"> This is extra before Items </h2>"
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
    "</body></html>";

const char kHtmlInputWithEmptyVisiblePortions[] =
    "<html><body></body></html>";

const char kSmallHtmlInput[] =
    "<html><head></head><body>A small test html.</body></html>";
const char kHtmlInputForNoBlink[] =
    "<html><head></head><body></body></html>";

const char kBlinkOutputCommon[] =
    "<html><body>"
    "<noscript><meta HTTP-EQUIV=\"refresh\" content=\"0;"
    "url='http://test.com/%s?ModPagespeed=noscript'\">"
    "<style><!--table,div,span,font,p{display:none} --></style>"
    "<div style=\"display:block\">Please click "
    "<a href=\"http://test.com/%s?ModPagespeed=noscript\">here</a> "
    "if you are not redirected within a few seconds.</div></noscript>"
    "critical_html"
    "<script>pagespeed.panelLoaderInit();</script>"
    "<script>pagespeed.panelLoader.setRequestFromInternalIp();</script>"
    "<script>pagespeed.panelLoader.loadCriticalData({});</script>"
    "<script>pagespeed.panelLoader.addCsiTiming(\"BLINK_FLOW_START\", 0)</script>"
    "<script>pagespeed.panelLoader.addCsiTiming(\"BLINK_DATA_LOOK_UP_DONE\", 0)</script>"
    "<script>pagespeed.panelLoader.loadImagesData();</script>";

const char kBlinkOutputSuffix[] =
    "<script>pagespeed.panelLoader.loadCookies([\"helo=world; path=/\"]);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-1.0\":{\"instance_html\":\"<h2 id=\\\"beforeItems\\\"> This is before Items </h2>\",\"xpath\":\"//div[@id=\\\"container\\\"]/h2[1]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.0\":{\"instance_html\":\"<div class=\\\"item\\\"><img src=\\\"image1\\\"><img src=\\\"image2\\\"></div>\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[2]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.1\":{\"instance_html\":\"<div class=\\\"item\\\"><img src=\\\"image3\\\"><div class=\\\"item\\\"><img src=\\\"image4\\\"></div></div>\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[3]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.bufferNonCriticalData();</script>\n"
    "</body></html>\n";

const char kBlinkOutputWithExtraNonCacheableSuffix[] =
    "<script>pagespeed.panelLoader.loadCookies([\"helo=world; path=/\"]);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-1.0\":{\"instance_html\":\"<h2 id=\\\"beforeItems\\\"> This is extra before Items </h2>\",\"xpath\":\"//div[@id=\\\"container\\\"]/h2[1]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.0\":{\"instance_html\":\"<div class=\\\"item\\\"><img src=\\\"image1\\\"><img src=\\\"image2\\\"></div>\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[2]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.loadNonCacheableObject({\"panel-id-0.1\":{\"instance_html\":\"<div class=\\\"item\\\"><img src=\\\"image3\\\"><div class=\\\"item\\\"><img src=\\\"image4\\\"></div></div>\",\"xpath\":\"//div[@id=\\\"container\\\"]/div[3]\"}}\n);</script>"
    "<script>pagespeed.panelLoader.bufferNonCriticalData();</script>\n"
    "</body></html>\n";

const char kBlinkOutputWithCacheablePanelsNoCookiesSuffix[] =
    "<script>pagespeed.panelLoader.bufferNonCriticalData();</script>\n"
    "</body></html>\n";

const char kBlinkOutputWithCacheablePanelsCookiesSuffix[] =
    "<script>pagespeed.panelLoader.bufferNonCriticalData();</script>"
    "<script>pagespeed.panelLoader.loadCookies([\"helo=world; path=/\"]);</script>\n"
    "</body></html>\n";

const char kCriticalHtml[] =
    "<html><body>"
    "<!--GooglePanel **** Start body ****-->"
    "critical_html"
    "</body></html>";

const char kFakePngInput[] = "FakePng";

const char kNoBlinkUrl[] =
    "http://test.com/noblink_text.html?ModPagespeed=noscript";

const char kNoScriptTextUrl[] =
    "http://test.com/text.html?ModPagespeed=noscript";

// Like ExpectStringAsyncFetch but for asynchronous invocation -- it lets
// one specify a WorkerTestBase::SyncPoint to help block until completion.
class AsyncExpectStringAsyncFetch : public ExpectStringAsyncFetch {
 public:
  AsyncExpectStringAsyncFetch(bool expect_success,
                              WorkerTestBase::SyncPoint* notify)
      : ExpectStringAsyncFetch(expect_success), notify_(notify) {}

  virtual ~AsyncExpectStringAsyncFetch() {}

  virtual void HandleDone(bool success) {
    ExpectStringAsyncFetch::HandleDone(success);
    notify_->Notify();
  }

 private:
  WorkerTestBase::SyncPoint* notify_;
  DISALLOW_COPY_AND_ASSIGN(AsyncExpectStringAsyncFetch);
};

// This class creates a proxy URL naming rule that encodes an "owner" domain
// and an "origin" domain, all inside a fixed proxy-domain.
class FakeUrlNamer : public UrlNamer {
 public:
  explicit FakeUrlNamer(Statistics* statistics)
      : options_(NULL),
        num_prepare_request_calls_(
            statistics->GetVariable(kNumPrepareRequestCalls)) {
    set_proxy_domain("http://proxy-domain");
  }

  // Given the request url and request headers, generate the rewrite options.
  virtual void DecodeOptions(const GoogleUrl& request_url,
                             const RequestHeaders& request_headers,
                             Callback* callback,
                             MessageHandler* handler) const {
    callback->Done((options_ == NULL) ? NULL : options_->Clone());
  }

  virtual void PrepareRequest(const RewriteOptions* rewrite_options,
                              GoogleString* url,
                              RequestHeaders* request_headers,
                              bool* success,
                              Function* func, MessageHandler* handler) {
    num_prepare_request_calls_->Add(1);
    UrlNamer::PrepareRequest(rewrite_options, url, request_headers, success,
                             func, handler);
  }

  void set_options(RewriteOptions* options) { options_ = options; }

 private:
  RewriteOptions* options_;
  Variable* num_prepare_request_calls_;
  DISALLOW_COPY_AND_ASSIGN(FakeUrlNamer);
};

// This class is used to simulate HandleDone(false).
class FlakyFakeUrlNamer : public FakeUrlNamer {
 public:
  explicit FlakyFakeUrlNamer(Statistics* statistics)
    : FakeUrlNamer(statistics) {}

  virtual bool Decode(const GoogleUrl& request_url,
                      GoogleUrl* owner_domain,
                      GoogleString* decoded) const {
    return true;
  }

  virtual bool IsAuthorized(const GoogleUrl& request_url,
                            const RewriteOptions& options) const {
    return false;
  }
};

class FakeBlinkCriticalLineDataFinder : public BlinkCriticalLineDataFinder {
 public:
  FakeBlinkCriticalLineDataFinder()
      : num_compute_calls_(0), pcache_(NULL) {}

  void set_property_cache(PropertyCache* pcache) { pcache_ = pcache; }

  // Gets BlinkCriticalLineData from the given PropertyPage.
  virtual BlinkCriticalLineData* ExtractBlinkCriticalLineData(
      int64 cache_time_ms, PropertyPage* page, int64 now_ms,
      bool diff_enabled) {
    if (pcache_ == NULL) {
      return blink_critical_line_data_.release();
    }
    const PropertyCache::Cohort* cohort = pcache_->GetCohort(
        BlinkCriticalLineDataFinder::kBlinkCohort);
    if (page == NULL || cohort == NULL) {
      return NULL;
    }
    PropertyValue* pvalue = page->GetProperty(cohort,
                                              "blink_critical_line_data");
    if (!pvalue->has_value() || pcache_->IsExpired(pvalue, cache_time_ms)) {
      return NULL;
    }
    ArrayInputStream input(pvalue->value().data(), pvalue->value().size());
    BlinkCriticalLineData* response = new BlinkCriticalLineData;
    if (!response->ParseFromZeroCopyStream(&input)) {
      LOG(DFATAL) << "Parsing value from cache into BlinkCriticalLineData "
                  << "failed.";
      delete response;
      return NULL;
    }
    return response;
  }

  void set_blink_critical_line_data(BlinkCriticalLineData* data) {
    blink_critical_line_data_.reset(data);
  }

  virtual void ComputeBlinkCriticalLineData(
      const GoogleString& computed_hash,
      const GoogleString& computed_hash_smart_diff,
      const StringPiece html_content,
      const ResponseHeaders* response_headers,
      RewriteDriver* driver) {
    ++num_compute_calls_;
    html_content_ = html_content.as_string();
    if (pcache_ == NULL || blink_critical_line_data_ == NULL) {
      return;
    }
    PropertyPage* page = driver->property_page();
    const PropertyCache::Cohort* cohort = pcache_->GetCohort(
        BlinkCriticalLineDataFinder::kBlinkCohort);
    if (page == NULL || cohort == NULL) {
      LOG(ERROR) << "PropertyPage or Cohort goes missing for url: "
                 << driver->url();
      return;
    }
    GoogleString buf;
    blink_critical_line_data_->SerializeToString(&buf);
    PropertyValue* pvalue = page->GetProperty(cohort,
                                              "blink_critical_line_data");
    pcache_->UpdateValue(buf, pvalue);
    pcache_->WriteCohort(cohort, page);
  }

  int num_compute_calls() { return num_compute_calls_; }

  GoogleString& html_content() { return html_content_; }

 private:
  int num_compute_calls_;
  PropertyCache* pcache_;
  GoogleString html_content_;
  scoped_ptr<BlinkCriticalLineData> blink_critical_line_data_;
  DISALLOW_COPY_AND_ASSIGN(FakeBlinkCriticalLineDataFinder);
};

class CustomRewriteDriverFactory : public TestRewriteDriverFactory {
 public:
  explicit CustomRewriteDriverFactory(MockUrlFetcher* url_fetcher)
      : TestRewriteDriverFactory(GTestTempDir(), url_fetcher) {
    InitializeDefaultOptions();
  }

  virtual void SetupCaches(ServerContext* resource_manager) {
    TestRewriteDriverFactory::SetupCaches(resource_manager);
    resource_manager->page_property_cache()->AddCohort(
        RewriteDriver::kDomCohort);
    resource_manager->page_property_cache()->AddCohort(
        BlinkCriticalLineDataFinder::kBlinkCohort);
    resource_manager->set_enable_property_cache(true);
  }

  LogRecord* NewLogRecord() {
    logging_info_.reset(new LoggingInfo());
    return new LogRecord(logging_info_.get());
  }

  LoggingInfo* logging_info() { return logging_info_.get(); }

 private:
  scoped_ptr<LoggingInfo> logging_info_;

  BlinkCriticalLineDataFinder* DefaultBlinkCriticalLineDataFinder(
      PropertyCache* pcache) {
    return new FakeBlinkCriticalLineDataFinder();
  }

  DISALLOW_COPY_AND_ASSIGN(CustomRewriteDriverFactory);
};

}  // namespace

class ProxyInterfaceWithDelayCache : public ProxyInterface {
 public:
  ProxyInterfaceWithDelayCache(const StringPiece& hostname, int port,
                               ServerContext* manager, Statistics* stats,
                               DelayCache* delay_cache)
      : ProxyInterface(hostname, port, manager, stats),
        manager_(manager),
        delay_cache_(delay_cache),
        key_("") {
  }

  // Initiates the PropertyCache look up.
  virtual ProxyFetchPropertyCallbackCollector* InitiatePropertyCacheLookup(
      bool is_resource_fetch,
      const GoogleUrl& request_url,
      RewriteOptions* options,
      AsyncFetch* async_fetch) {
    GoogleString key_base(request_url.Spec().as_string());
    if (options != NULL) {
      manager_->ComputeSignature(options);
      key_base = StrCat(request_url.Spec(), "_", options->signature());
    }
    PropertyCache* pcache = manager_->page_property_cache();
    const PropertyCache::Cohort* cohort =
        pcache->GetCohort(BlinkCriticalLineDataFinder::kBlinkCohort);
    key_ = pcache->CacheKey(key_base, cohort);
    delay_cache_->DelayKey(key_);
    return ProxyInterface::InitiatePropertyCacheLookup(
        is_resource_fetch, request_url, options, async_fetch);
  }

  const GoogleString& key() const { return key_; }

 private:
  ServerContext* manager_;
  DelayCache* delay_cache_;
  GoogleString key_;

  DISALLOW_COPY_AND_ASSIGN(ProxyInterfaceWithDelayCache);
};

// TODO(nikhilmadan): Test cookies, fetch failures, 304 responses etc.
// TODO(nikhilmadan): Refactor to share common code with ProxyInterfaceTest.
class BlinkFlowCriticalLineTest : public RewriteTestBase {
 protected:
  static const int kHtmlCacheTimeSec = 5000;

  BlinkFlowCriticalLineTest()
      : RewriteTestBase(
          new CustomRewriteDriverFactory(&mock_url_fetcher_),
          new CustomRewriteDriverFactory(&mock_url_fetcher_)),
        blink_output_(StrCat(StringPrintf(
            kBlinkOutputCommon, "text.html", "text.html"), kBlinkOutputSuffix)),
        blink_output_with_extra_non_cacheable_(StrCat(StringPrintf(
            kBlinkOutputCommon, "text.html", "text.html"),
            kBlinkOutputWithExtraNonCacheableSuffix)),
        blink_output_with_cacheable_panels_no_cookies_(StrCat(StringPrintf(
            kBlinkOutputCommon, "flaky.html", "flaky.html"),
            kBlinkOutputWithCacheablePanelsNoCookiesSuffix)),
        blink_output_with_cacheable_panels_cookies_(StrCat(StringPrintf(
            kBlinkOutputCommon, "cache.html", "cache.html"),
            kBlinkOutputWithCacheablePanelsCookiesSuffix)) {
    noblink_output_ = StrCat("<html><head></head><body>",
                             StringPrintf(kNoScriptRedirectFormatter,
                                          kNoBlinkUrl, kNoBlinkUrl),
                             "</body></html>");
    StringPiece lazyload_js_code =
        resource_manager()->static_javascript_manager()->GetJsSnippet(
            StaticJavascriptManager::kLazyloadImagesJs, options());
    noblink_output_with_lazy_load_ = StringPrintf(kLazyLoadHtml,
        StringPrintf(kNoScriptRedirectFormatter,
                     kNoScriptTextUrl, kNoScriptTextUrl).c_str(),
        StrCat("<script type=\"text/javascript\">",
               lazyload_js_code, "\npagespeed.lazyLoadInit(false, \"",
               LazyloadImagesFilter::kBlankImageSrc,
               "\");\n</script>").c_str());
    blink_output_with_lazy_load_ = StrCat(StringPrintf(
        kBlinkOutputCommon, "text.html", "text.html"),
        "<script type=\"text/javascript\">",
        lazyload_js_code, "\npagespeed.lazyLoadInit(false, \"",
        LazyloadImagesFilter::kBlankImageSrc, "\");\n</script>",
        kBlinkOutputSuffix);
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

  virtual void SetUp() {
    UseMd5Hasher();
    ThreadSynchronizer* sync = resource_manager()->thread_synchronizer();
    sync->EnableForPrefix(BlinkFlowCriticalLine::kBackgroundComputationDone);
    sync->AllowSloppyTermination(
        BlinkFlowCriticalLine::kBackgroundComputationDone);
    sync->EnableForPrefix(BlinkFlowCriticalLine::kUpdateResponseCodeDone);
    sync->AllowSloppyTermination(
        BlinkFlowCriticalLine::kUpdateResponseCodeDone);
    fake_blink_critical_line_data_finder_ =
        static_cast<FakeBlinkCriticalLineDataFinder*> (
            resource_manager_->blink_critical_line_data_finder());
    options_.reset(resource_manager()->NewOptions());
    options_->set_enable_blink_critical_line(true);
    options_->set_passthrough_blink_for_last_invalid_response_code(true);
    options_->EnableFilter(RewriteOptions::kPrioritizeVisibleContent);
    options_->AddBlinkCacheableFamily("/text.html", 1000 * Timer::kSecondMs,
                                      "class=item,id=beforeItems");
    options_->AddBlinkCacheableFamily("*html", 1000 * Timer::kSecondMs, "");

    // Force disable filters that will be enabled in the blink flow
    // since we want to test that they get enabled in the blink flow.
    // We don't force enable some of the other rewriters in the test since
    // they manipulate the passthru case.
    options_->DisableFilter(RewriteOptions::kComputePanelJson);
    options_->DisableFilter(RewriteOptions::kDisableJavascript);
    options_->ForceEnableFilter(RewriteOptions::kHtmlWriterFilter);
    options_->ForceEnableFilter(RewriteOptions::kConvertMetaTags);
    options_->ForceEnableFilter(RewriteOptions::kCombineCss);
    options_->ForceEnableFilter(RewriteOptions::kCombineJavascript);
    options_->ForceEnableFilter(RewriteOptions::kDelayImages);

    options_->Disallow("*blacklist*");

    resource_manager()->ComputeSignature(options_.get());

    RewriteTestBase::SetUp();
    ProxyInterface::Initialize(statistics());
    proxy_interface_.reset(
        new ProxyInterface("localhost", 80, resource_manager(), statistics()));

    statistics()->AddVariable(kNumPrepareRequestCalls);
    fake_url_namer_.reset(new FakeUrlNamer(statistics()));
    fake_url_namer_->set_options(options_.get());
    flaky_fake_url_namer_.reset(new FlakyFakeUrlNamer(statistics()));
    flaky_fake_url_namer_->set_options(options_.get());

    resource_manager()->set_url_namer(fake_url_namer_.get());

    mock_timer()->SetTimeUs(MockTimer::kApr_5_2010_ms * Timer::kMsUs);
    mock_url_fetcher_.set_fail_on_unexpected(false);

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
    SetFetchResponse("http://test.com/smalltest.html", response_headers_,
                     kSmallHtmlInput);
    SetFetchResponse("http://test.com/noblink_text.html", response_headers_,
                     kHtmlInputForNoBlink);
    SetFetchResponse("http://test.com/cache.html", response_headers_,
                     kHtmlInput);
    SetFetchResponse("http://test.com/non_html.html", response_headers_,
                     kFakePngInput);
    SetFetchResponse("http://test.com/ws_text.html", response_headers_,
                     StrCat(kWhitespace, kHtmlInput));
  }

  virtual void TearDown() {
    EXPECT_EQ(0, resource_manager()->num_active_rewrite_drivers());
    RewriteTestBase::TearDown();
  }

  void InitializeFuriousSpec() {
    options_->set_running_furious_experiment(true);
    NullMessageHandler handler;
    ASSERT_TRUE(options_->AddFuriousSpec("id=3;percent=100;default", &handler));
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

  void VerifyNonBlinkResponse(ResponseHeaders* response_headers) {
    ConstStringStarVector values;
    EXPECT_TRUE(response_headers->Lookup(HttpAttributes::kCacheControl,
                                         &values));
    EXPECT_STREQ("max-age=0", *(values[0]));
    EXPECT_STREQ("no-cache", *(values[1]));
  }

  void VerifyBlinkResponse(ResponseHeaders* response_headers) {
    ConstStringStarVector v;
    EXPECT_STREQ("text/html; charset=utf-8",
                 response_headers->Lookup1(HttpAttributes::kContentType));
    EXPECT_TRUE(response_headers->Lookup(HttpAttributes::kCacheControl, &v));
    EXPECT_EQ("max-age=0", *v[0]);
    EXPECT_EQ("private", *v[1]);
    EXPECT_EQ("no-cache", *v[2]);
  }

  void FetchFromProxyWaitForUpdateResponseCode(
      const StringPiece& url, bool expect_success, GoogleString* string_out,
      ResponseHeaders* headers_out) {
    RequestHeaders request_headers;
    GetDefaultRequestHeaders(&request_headers);
    FetchFromProxy(url, expect_success, request_headers, string_out,
                   headers_out, NULL, false, true);
  }

  void FetchFromProxyWaitForUpdateResponseCode(
      const StringPiece& url, bool expect_success,
      const RequestHeaders& request_headers, GoogleString* string_out,
      ResponseHeaders* headers_out) {
    FetchFromProxy(url, expect_success, request_headers, string_out,
                   headers_out, NULL, false, true);
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
    FetchFromProxy(url, expect_success, request_headers, string_out,
                   headers_out, user_agent_out,
                   wait_for_background_computation, false);
  }

  void FetchFromProxy(const StringPiece& url,
                      bool expect_success,
                      const RequestHeaders& request_headers,
                      GoogleString* string_out,
                      ResponseHeaders* headers_out,
                      GoogleString* user_agent_out,
                      bool wait_for_background_computation,
                      bool wait_for_update_response_code) {
    FetchFromProxyNoQuiescence(url, expect_success, request_headers,
                               string_out, headers_out, user_agent_out);
    if (wait_for_background_computation) {
      ThreadSynchronizer* sync = resource_manager()->thread_synchronizer();
      sync->Wait(BlinkFlowCriticalLine::kBackgroundComputationDone);
    }
    if (wait_for_update_response_code) {
      ThreadSynchronizer* sync = resource_manager()->thread_synchronizer();
      sync->Wait(BlinkFlowCriticalLine::kUpdateResponseCodeDone);
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
    WorkerTestBase::SyncPoint sync(resource_manager()->thread_system());
    AsyncExpectStringAsyncFetch callback(expect_success, &sync);
    TimingInfo timing_info = callback.logging_info()->timing_info();
    timing_info.set_request_start_ms(resource_manager()->timer()->NowMs());
    callback.set_response_headers(headers_out);
    callback.request_headers()->CopyFrom(request_headers);
    bool already_done = proxy_interface_->Fetch(AbsolutifyUrl(url),
                                                message_handler(), &callback);
    CHECK(resource_manager()->thread_synchronizer() != NULL);
    if (already_done) {
      EXPECT_TRUE(callback.done());
    } else {
      sync.Wait();
    }
    *string_out = callback.buffer();
    if (user_agent_out != NULL &&
        callback.request_headers()->Lookup1(HttpAttributes::kUserAgent)
        != NULL) {
      user_agent_out->assign(
          callback.request_headers()->Lookup1(HttpAttributes::kUserAgent));
    }
    if (callback.logging_info() != NULL) {
      logging_info_.CopyFrom(*(callback.logging_info()));
    }
  }

  void FetchFromProxyWithDelayCache(
      const StringPiece& url, bool expect_success,
      const RequestHeaders& request_headers,
      ProxyInterfaceWithDelayCache* proxy_interface,
      GoogleString* string_out,
      ResponseHeaders* headers_out) {
    WorkerTestBase::SyncPoint sync(resource_manager()->thread_system());
    AsyncExpectStringAsyncFetch callback(expect_success, &sync);
    callback.set_response_headers(headers_out);
    callback.request_headers()->CopyFrom(request_headers);
    bool already_done = proxy_interface->Fetch(AbsolutifyUrl(url),
                                               message_handler(), &callback);
    CHECK(resource_manager()->thread_synchronizer() != NULL);
    delay_cache()->ReleaseKey(proxy_interface->key());
    if (already_done) {
      EXPECT_TRUE(callback.done());
    } else {
      sync.Wait();
    }
    *string_out = callback.buffer();
    ThreadSynchronizer* ts = resource_manager()->thread_synchronizer();
    ts->Wait(BlinkFlowCriticalLine::kBackgroundComputationDone);
    mock_scheduler()->AwaitQuiescence();
  }

  void CheckHeaders(const ResponseHeaders& headers,
                    const ContentType& expect_type) {
    ASSERT_TRUE(headers.has_status_code());
    EXPECT_EQ(HttpStatus::kOK, headers.status_code());
    EXPECT_STREQ(expect_type.mime_type(),
                 headers.Lookup1(HttpAttributes::kContentType));
  }

  // Verifies the fields of BlinkInfo proto being logged.
  BlinkInfo* VerifyBlinkInfo(int blink_request_flow, const char* url) {
    CustomRewriteDriverFactory* factory =
        static_cast<CustomRewriteDriverFactory*>(resource_manager()->factory());
    BlinkInfo* blink_info = factory->logging_info()->mutable_blink_info();
    EXPECT_EQ(blink_request_flow, blink_info->blink_request_flow());
    EXPECT_EQ("1345815119391831", blink_info->request_event_id_time_usec());
    EXPECT_STREQ(url, blink_info->url());
    return blink_info;
  }

  BlinkInfo* VerifyBlinkInfo(int blink_request_flow, bool html_match,
                             const char* url) {
    BlinkInfo* blink_info = VerifyBlinkInfo(blink_request_flow, url);
    EXPECT_EQ(html_match, blink_info->html_match());
    return blink_info;
  }

  scoped_ptr<ProxyInterface> proxy_interface_;
  scoped_ptr<FakeUrlNamer> fake_url_namer_;
  scoped_ptr<FlakyFakeUrlNamer> flaky_fake_url_namer_;
  scoped_ptr<RewriteOptions> options_;
  int64 start_time_ms_;
  GoogleString start_time_string_;

  void UnEscapeString(GoogleString* str) {
    GlobalReplaceSubstring("__psa_lt;", "<", str);
    GlobalReplaceSubstring("__psa_gt;", ">", str);
  }

  int num_compute_calls() {
    return fake_blink_critical_line_data_finder_->num_compute_calls();
  }

  GoogleString& html_content() {
    return fake_blink_critical_line_data_finder_->html_content();
  }

  void set_blink_critical_line_data(BlinkCriticalLineData* data) {
    fake_blink_critical_line_data_finder_->set_blink_critical_line_data(data);
  }

  void SetBlinkCriticalLineData() {
    SetBlinkCriticalLineData(true, "", "");
  }

  void SetBlinkCriticalLineData(bool value) {
    SetBlinkCriticalLineData(value, "", "");
  }

  void SetBlinkCriticalLineData(bool value, const GoogleString& hash,
                                const GoogleString& hash_smart_diff) {
    BlinkCriticalLineData* data = new BlinkCriticalLineData();
    data->set_url("url");
    if (value) {
      data->set_critical_html(kCriticalHtml);
    }
    if (hash != "") {
      data->set_hash(hash);
    }
    if (hash_smart_diff != "") {
      data->set_hash_smart_diff(hash_smart_diff);
    }
    fake_blink_critical_line_data_finder_->set_blink_critical_line_data(data);
  }

  void SetFetchHtmlResponseWithStatus(const char* url,
                                      HttpStatus::Code status) {
    ResponseHeaders response_headers;
    response_headers.SetStatusAndReason(status);
    response_headers.Add(HttpAttributes::kContentType, "text/html");
    SetFetchResponse(url, response_headers, kHtmlInput);
  }

  void TestBlinkHtmlChangeDetection(bool just_logging) {
    options_->ClearSignatureForTesting();
    options_->set_enable_blink_html_change_detection(!just_logging);
    options_->set_enable_blink_html_change_detection_logging(just_logging);
    resource_manager()->ComputeSignature(options_.get());

    GoogleString text;
    ResponseHeaders response_headers;
    FetchFromProxyWaitForBackground(
        "text.html", true, &text, &response_headers);

    EXPECT_STREQ(kHtmlInput, text);
    EXPECT_EQ(1, num_compute_calls());
    EXPECT_EQ(kHtmlInput, text);
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlCacheMisses)->Get());
    EXPECT_EQ(0, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMismatches)->Get());
    EXPECT_EQ(0, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlSmartdiffMismatches)->Get());
    response_headers.Clear();
    ClearStats();

    // Hashes not set. Results in mismatches.
    SetBlinkCriticalLineData(true, "", "");
    FetchFromProxyWaitForBackground(
        "text.html", true, &text, &response_headers);

    UnEscapeString(&text);
    EXPECT_STREQ(blink_output_, text);
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMismatches)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlSmartdiffMismatches)->Get());
    EXPECT_EQ(just_logging ? 0 : 1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMismatchesCacheDeletes)->Get());
    EXPECT_EQ(just_logging ? 0 : 1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits)->Get());
    VerifyBlinkInfo(BlinkInfo::BLINK_CACHE_HIT, false,
                    "http://test.com/text.html");
    ClearStats();

    // Hashes set. No mismatches.
    SetBlinkCriticalLineData(true, "5SmNjVuPwO", "iWAZTRzhFW");
    FetchFromProxyWaitForBackground(
        "text.html", true, &text, &response_headers);

    UnEscapeString(&text);
    EXPECT_STREQ(blink_output_, text);
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMatches)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlSmartdiffMatches)->Get());
    EXPECT_EQ(0, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits)->Get());
    VerifyBlinkInfo(BlinkInfo::BLINK_CACHE_HIT, true,
                    "http://test.com/text.html");
    ClearStats();

    // Input with an extra comment. We strip out comments before taking hash,
    // so there should be no mismatches.
    SetFetchResponse("http://test.com/text.html", response_headers_,
                     kHtmlInputWithExtraCommentAndNonCacheable);
    SetBlinkCriticalLineData(true, "5SmNjVuPwO", "iWAZTRzhFW");
    FetchFromProxyWaitForBackground(
        "text.html", true, &text, &response_headers);

    UnEscapeString(&text);
    EXPECT_STREQ(blink_output_with_extra_non_cacheable_, text);
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMatches)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlSmartdiffMatches)->Get());
    EXPECT_EQ(0, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits)->Get());
    VerifyBlinkInfo(BlinkInfo::BLINK_CACHE_HIT, true,
                    "http://test.com/text.html");

    ClearStats();
    // Input with extra attributes. This should result in a mismatch with
    // full-diff but a match with smart-diff.
    SetFetchResponse("http://test.com/text.html", response_headers_,
                     kHtmlInputWithExtraAttribute);
    SetBlinkCriticalLineData(true, "5SmNjVuPwO", "iWAZTRzhFW");
    FetchFromProxyWaitForBackground(
        "text.html", true, &text, &response_headers);

    UnEscapeString(&text);
    EXPECT_STREQ(blink_output_, text);
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMismatches)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlSmartdiffMatches)->Get());
    EXPECT_EQ(0, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMatches)->Get());
    EXPECT_EQ(0, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlSmartdiffMismatches)->Get());
    EXPECT_EQ(just_logging ? 0 : 1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits)->Get());
    VerifyBlinkInfo(BlinkInfo::BLINK_CACHE_HIT, false,
                    "http://test.com/text.html");
    ClearStats();

    // Input with empty visible portions. Diff calculation should not trigger.
    SetFetchResponse("http://test.com/text.html", response_headers_,
                     kHtmlInputWithEmptyVisiblePortions);
    SetBlinkCriticalLineData(true, "5SmNjVuPwO", "iWAZTRzhFW");
    FetchFromProxyWaitForBackground(
        "text.html", true, &text, &response_headers);

    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMismatches)->Get());
    EXPECT_EQ(0, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlSmartdiffMatches)->Get());
    EXPECT_EQ(0, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlMatches)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlSmartdiffMismatches)->Get());
    EXPECT_EQ(1, statistics()->FindVariable(
        BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits)->Get());
  }

  ResponseHeaders response_headers_;
  GoogleString noblink_output_;
  GoogleString noblink_output_with_lazy_load_;
  GoogleString blink_output_with_lazy_load_;
  FakeBlinkCriticalLineDataFinder* fake_blink_critical_line_data_finder_;
  LoggingInfo logging_info_;
  const GoogleString blink_output_;
  const GoogleString blink_output_with_extra_non_cacheable_;
  const GoogleString blink_output_with_cacheable_panels_no_cookies_;
  const GoogleString blink_output_with_cacheable_panels_cookies_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlinkFlowCriticalLineTest);
};

TEST_F(BlinkFlowCriticalLineTest, TestFlakyNon200ResponseCodeValidHitAfter404) {
  GoogleString text;
  ResponseHeaders response_headers_out;
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html", HttpStatus::kOK);

  // Caches miss.
  FetchFromProxyWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);

  EXPECT_STREQ(kHtmlInput, text);
  // Cache lookup for original plain text, BlinkCriticalLineData and Dom Cohort
  // in property cache.
  VerifyBlinkInfo(BlinkInfo::BLINK_CACHE_MISS_TRIGGERED_REWRITE,
                  "http://test.com/flaky.html");
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, num_compute_calls());

  ClearStats();
  response_headers_out.Clear();
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html",
                                 HttpStatus::kNotFound);
  SetBlinkCriticalLineData();

  // Cache hit.  Origin gives 404.
  FetchFromProxyNoWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_with_cacheable_panels_no_cookies_, text);
  EXPECT_EQ(1, num_compute_calls());

  ClearStats();
  response_headers_out.Clear();
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html", HttpStatus::kOK);
  SetBlinkCriticalLineData();

  // Cache hit with previous response being 404 -- passthrough.  Current
  // response is 200.
  FetchFromProxyWaitForUpdateResponseCode(
      "flaky.html", true, &text, &response_headers_out);
  UnEscapeString(&text);
  EXPECT_STREQ(kHtmlInput, text);
  VerifyBlinkInfo(BlinkInfo::FOUND_LAST_STATUS_CODE_NON_OK,
                  "http://test.com/flaky.html");
  EXPECT_EQ(1, num_compute_calls());

  ClearStats();
  response_headers_out.Clear();
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html", HttpStatus::kOK);
  SetBlinkCriticalLineData();
  // Cache hit with previous response being 200.
  FetchFromProxyNoWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_with_cacheable_panels_no_cookies_, text);
  // Normal Hit case.
  VerifyBlinkInfo(BlinkInfo::BLINK_CACHE_HIT,
                  "http://test.com/flaky.html");
  EXPECT_EQ(1, num_compute_calls());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkInfoErrorScenarios) {
  GoogleString text;
  ResponseHeaders response_headers_out;
  resource_manager()->set_url_namer(flaky_fake_url_namer_.get());
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html",
      HttpStatus::kOK);
  FetchFromProxyWaitForBackground(
      "flaky.html", false, &text, &response_headers_out);

  // HandleDone(False) case.
  VerifyBlinkInfo(BlinkInfo::BLINK_CACHE_MISS_FETCH_NON_OK,
                  "http://test.com/flaky.html");

  ClearStats();
  response_headers_out.Clear();
  resource_manager()->set_url_namer(fake_url_namer_.get());
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html",
                                 HttpStatus::kNotFound);
  SetBlinkCriticalLineData(false);
  FetchFromProxyWaitForUpdateResponseCode(
      "flaky.html", true, &text, &response_headers_out);
  UnEscapeString(&text);
  // Malformed HTML case.
  VerifyBlinkInfo(BlinkInfo::FOUND_MALFORMED_HTML,
                  "http://test.com/flaky.html");
}

TEST_F(BlinkFlowCriticalLineTest,
       TestFlakyNon200ResponseCodeDoNotWriteResponseCode) {
  options_->ClearSignatureForTesting();
  options_->set_passthrough_blink_for_last_invalid_response_code(false);
  resource_manager()->ComputeSignature(options_.get());

  GoogleString text;
  ResponseHeaders response_headers_out;

  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html", HttpStatus::kOK);

  // Caches miss.
  FetchFromProxyWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);
  EXPECT_STREQ(kHtmlInput, text);
  EXPECT_EQ(1, num_compute_calls());  // Cache miss -- insert in cache.

  ClearStats();
  response_headers_out.Clear();
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html",
                                 HttpStatus::kNotFound);
  SetBlinkCriticalLineData();

  // Cache hit.  Origin gives 404.
  FetchFromProxyNoWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_with_cacheable_panels_no_cookies_, text);

  ClearStats();
  response_headers_out.Clear();
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html", HttpStatus::kOK);
  SetBlinkCriticalLineData();

  // Cache hit with previous response being 404 -- we serve from cache since
  // passthrough_blink_for_last_invalid_response_code is false.
  FetchFromProxyNoWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_with_cacheable_panels_no_cookies_, text);
}

TEST_F(BlinkFlowCriticalLineTest,
       TestFlakyNon200ResponseCodeValidMissAfter404) {
  GoogleString text;
  ResponseHeaders response_headers_out;

  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html", HttpStatus::kOK);

  // Cache miss.
  FetchFromProxyWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);

  ClearStats();
  response_headers_out.Clear();
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html",
                                 HttpStatus::kNotFound);
  SetBlinkCriticalLineData();

  // Cache hit.  Origin gives 404.
  FetchFromProxyNoWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);

  ClearStats();
  response_headers_out.Clear();
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html", HttpStatus::kOK);

  // Cache miss with previous response being 404.  Current request gives 200.
  FetchFromProxyWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);
  EXPECT_STREQ(kHtmlInput, text);
  // Cache lookup for  plain text, BlinkCriticalLineData in property cache.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, num_compute_calls());

  ClearStats();
  response_headers_out.Clear();
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html", HttpStatus::kOK);
  SetBlinkCriticalLineData();

  // Cache hit.
  FetchFromProxyNoWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_with_cacheable_panels_no_cookies_, text);
  EXPECT_EQ(2, num_compute_calls());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkCacheMissFuriousSetCookie) {
  options_->ClearSignatureForTesting();
  InitializeFuriousSpec();
  resource_manager()->ComputeSignature(options_.get());
  GoogleString text;
  ResponseHeaders response_headers;

  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);

  ConstStringStarVector values;
  EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kSetCookie, &values));
  EXPECT_EQ(2, values.size());
  EXPECT_STREQ("_GFURIOUS=3", (*(values[1])).substr(0, 11));
  VerifyNonBlinkResponse(&response_headers);
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkCacheHitFuriousSetCookie) {
  options_->ClearSignatureForTesting();
  InitializeFuriousSpec();
  resource_manager()->ComputeSignature(options_.get());
  GoogleString text;
  ResponseHeaders response_headers;

  SetBlinkCriticalLineData();
  FetchFromProxyNoWaitForBackground("text.html", true, &text,
                                    &response_headers);

  ConstStringStarVector values;
  EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kSetCookie, &values));
  EXPECT_EQ(1, values.size());
  EXPECT_STREQ("_GFURIOUS=3", (*(values[0])).substr(0, 11));
  VerifyBlinkResponse(&response_headers);
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkFuriousCookieHandling) {
  options_->ClearSignatureForTesting();
  InitializeFuriousSpec();
  resource_manager()->ComputeSignature(options_.get());
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  GetDefaultRequestHeaders(&request_headers);
  request_headers.Add(HttpAttributes::kCookie, "_GFURIOUS=3");

  SetBlinkCriticalLineData();
  FetchFromProxy("text.html", true, request_headers,
                 &text, &response_headers, false);

  EXPECT_FALSE(response_headers.Has(HttpAttributes::kSetCookie));
  VerifyBlinkResponse(&response_headers);
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkPassthruAndNonPassthru) {
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);
  EXPECT_EQ(BlinkInfo::BLINK_DESKTOP_WHITELIST,
            logging_info_.blink_info().blink_user_agent());
  ConstStringStarVector values;
  EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kSetCookie, &values));
  EXPECT_EQ(1, values.size());
  if ((*(values[0])).size() >= 11) {
    // 11 is the minimum size of the GFURIOUS cookie.
    EXPECT_NE("_GFURIOUS=3", (*(values[0])).substr(0, 11));
  }
  VerifyNonBlinkResponse(&response_headers);

  EXPECT_STREQ(kHtmlInput, text);
  EXPECT_STREQ("text/html; charset=utf-8",
               response_headers.Lookup1(HttpAttributes::kContentType));

  // Cache lookup for original plain text, BlinkCriticalLineData and Dom Cohort
  // in property cache.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
  EXPECT_EQ(1, num_compute_calls());
  EXPECT_EQ(kHtmlInput, text);
  ConstStringStarVector psa_rewriter_header_values;
  EXPECT_FALSE(response_headers.Lookup(kPsaRewriterHeader,
                                       &psa_rewriter_header_values));
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkSharedFetchesStarted)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkSharedFetchesCompleted)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlCacheMisses)->Get());
  response_headers.Clear();
  ClearStats();

  SetBlinkCriticalLineData();
  FetchFromProxyNoWaitForBackground(
      "text.html", true, &text, &response_headers);

  EXPECT_STREQ("OK", response_headers.reason_phrase());
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  VerifyBlinkResponse(&response_headers);

  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_, text);
  EXPECT_TRUE(response_headers.Lookup(kPsaRewriterHeader,
                                      &psa_rewriter_header_values));
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkSharedFetchesStarted)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkSharedFetchesCompleted)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits)->Get());
  ClearStats();

  // Request from external ip
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kUserAgent, kLinuxUserAgent);
  request_headers.Add(HttpAttributes::kXForwardedFor, "64.236.24.12");
  SetBlinkCriticalLineData(false);
  FetchFromProxyWaitForUpdateResponseCode(
      "text.html", true, request_headers, &text, &response_headers);
  EXPECT_EQ(GoogleString::npos,
            text.find("pagespeed.panelLoader.setRequestFromInternalIp()"));
  EXPECT_EQ(1, statistics()->GetVariable(
            BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkUrlCacheInvalidation) {
  GoogleString text;
  ResponseHeaders response_headers;
  fake_blink_critical_line_data_finder_->set_property_cache(
      page_property_cache());

  SetBlinkCriticalLineData();
  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);

  EXPECT_STREQ(kHtmlInput, text);
  // Cache lookup for original plain text, BlinkCriticalLineData and Dom Cohort
  // in property cache, all miss.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(2, lru_cache()->num_inserts());  // Both cohorts in pcache.
  EXPECT_EQ(0, lru_cache()->num_deletes());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(1, num_compute_calls());
  response_headers.Clear();
  ClearStats();

  set_blink_critical_line_data(NULL);

  // Property cache hit.
  FetchFromProxyNoWaitForBackground(
      "text.html", true, &text, &response_headers);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_, text);
  EXPECT_EQ(1, lru_cache()->num_misses());   // Original plain text.
  EXPECT_EQ(2, lru_cache()->num_hits());     // pcache, two cohorts
  // The status code value in Dom cohort is unchanged, and so the PropertyValue
  // has num_writes bumped to 1.  Thus the value seen by the underlying lru
  // cache changes.  Hence a delete and insert.
  // blink cohort value is neither updated or written.
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_deletes());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(1, num_compute_calls());
  ClearStats();

  // Invalidate the cache for some URL other than 'text.html'.
  options_->ClearSignatureForTesting();
  options_->AddUrlCacheInvalidationEntry(
      AbsolutifyUrl("foo.bar"), mock_timer()->NowMs(), true);
  resource_manager()->ComputeSignature(options_.get());

  // Property cache hit.
  FetchFromProxyNoWaitForBackground(
      "text.html", true, &text, &response_headers);
  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_, text);
  EXPECT_EQ(1, lru_cache()->num_misses());   // Original plain text.
  EXPECT_EQ(2, lru_cache()->num_hits());     // pcache, two cohorts
  // The status code value in Dom cohort is unchanged, and so the PropertyValue
  // has num_writes bumped to 2.  Thus the value seen by the underlying lru
  // cache changes.  Hence a delete and insert.
  // blink cohort value is neither updated or written.
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_deletes());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(1, num_compute_calls());
  ClearStats();

  // Invalidate the cache.
  options_->ClearSignatureForTesting();
  options_->AddUrlCacheInvalidationEntry(
      AbsolutifyUrl("text.html"), mock_timer()->NowMs(), true);
  resource_manager()->ComputeSignature(options_.get());

  SetBlinkCriticalLineData();
  // Property cache hit, but invalidated.  Hence treated as a miss and
  // passthrough by blink.
  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);

  EXPECT_STREQ(kHtmlInput, text);
  EXPECT_EQ(1, lru_cache()->num_misses());   // Original plain text.
  EXPECT_EQ(2, lru_cache()->num_hits());     // pcache, two cohorts
  // The invalidation results in both the PropertyValues (status code in dom
  // cohort and critical line data in blink cohort) not getting populated in
  // PropertyPage.  Thus on update the status code value has its PropertyValue's
  // num_writes being reset.  This means the underlying lru cache seems a
  // different value, and hence a delete and write for the dom cohort write.
  // For the update of critical line data the same reset of PropertyValue
  // num_writes happens, but since there was only one write for this earlier
  // (so, the num_writes was already 0) the actual value seen by lru cache is
  // the same.  Hence for blink cohort, we see an identical_reinsert in lru
  // cache.
  EXPECT_EQ(1, lru_cache()->num_inserts());  // dom cohort
  EXPECT_EQ(1, lru_cache()->num_deletes());  // dom cohort
  EXPECT_EQ(1, lru_cache()->num_identical_reinserts());  // blink cohort
  EXPECT_EQ(2, num_compute_calls());         // One more now.
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkWithHeadRequest) {
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kUserAgent, kLinuxUserAgent);
  request_headers.set_method(RequestHeaders::kHead);
  FetchFromProxy("text.html", true, request_headers,
                 &text, &response_headers, false);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
}

// TODO(rahulbansal): Reproduce and uncomment this out.
/*
TEST_F(BlinkFlowCriticalLineTest, TestBlinkCriticalLineLoadShed) {
  // Make sure things behave when the computation gets load-shed.
  resource_manager()->low_priority_rewrite_workers()->
      SetLoadSheddingThreshold(1);

  // Wedge the low-priority rewrite queue, so that the blink
  // rewrite gets dropped.
  WorkerTestBase::SyncPoint sync1(resource_manager()->thread_system());
  WorkerTestBase::SyncPoint sync2(resource_manager()->thread_system());

  QueuedWorkerPool* work_pool =
      resource_manager()->low_priority_rewrite_workers();
  work_pool->NewSequence()->Add(new WorkerTestBase::WaitRunFunction(&sync1));
  work_pool->NewSequence()->Add(new WorkerTestBase::WaitRunFunction(&sync2));

  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  GetDefaultRequestHeaders(&request_headers);
  FetchFromProxyNoQuiescence("text.html", true, request_headers,
                             &text, &response_headers);
  EXPECT_STREQ(kHtmlInput, text);

  // At this point, the computation task is stuck at the end of the queue.
  EXPECT_EQ(0, num_compute_calls());

  // Try again.
  FetchFromProxyNoQuiescence("text.html", true, request_headers,
                             &text, &response_headers);
  EXPECT_STREQ(kHtmlInput, text);
  // Once we get here, the first computation task actually got dropped
  // already, and second is stuck at the end of the work queue.
  EXPECT_EQ(0, num_compute_calls());

  // Unwedge the thread.
  sync1.Notify();
  sync2.Notify();
  ThreadSynchronizer* ts = resource_manager()->thread_synchronizer();
  ts->Wait(BlinkFlowCriticalLine::kBackgroundComputationDone);
  mock_scheduler()->AwaitQuiescence();

  // The second computation ought to have completed now.
  EXPECT_EQ(1, num_compute_calls());
}
*/

TEST_F(BlinkFlowCriticalLineTest, TestBlinkHtmlWithWhitespace) {
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyWaitForBackground(
      "ws_text.html", true, &text, &response_headers);
  EXPECT_EQ(1, num_compute_calls());
  EXPECT_EQ(kWhitespace, html_content().substr(0, strlen(kWhitespace)));
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkCriticalLineDataMissDelayCache) {
  GoogleString text;
  ResponseHeaders response_headers;
  ProxyInterfaceWithDelayCache* proxy_interface =
      new ProxyInterfaceWithDelayCache("localhost", 80,
                                       resource_manager(), statistics(),
                                       delay_cache());
  proxy_interface_.reset(proxy_interface);
  RequestHeaders request_headers;
  GetDefaultRequestHeaders(&request_headers);
  FetchFromProxyWithDelayCache(
      "text.html", true, request_headers, proxy_interface,
      &text, &response_headers);

  EXPECT_STREQ(kHtmlInput, text);
  EXPECT_STREQ("text/html; charset=utf-8",
               response_headers.Lookup1(HttpAttributes::kContentType));

  // Cache lookup for original plain text, BlinkCriticalLineData and Dom Cohort
  // in property cache.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
  EXPECT_EQ(1, num_compute_calls());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkWithBlacklistUrls) {
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kUserAgent, kBlackListUserAgent);
  FetchFromProxy("blacklist.html", true, request_headers, &text,
                 &response_headers, false);
  // unassigned user agent
  EXPECT_EQ(BlinkInfo::NOT_SET, logging_info_.blink_info().blink_user_agent());
  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_STREQ(kHtmlInput, text);
  // Three cache lookup - for the original html and two for property cache.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  // No fetch for background computation is triggered here.
  // Only original html is fetched from fetcher.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // No blink flow should have happened.
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkWithBlacklistUserAgents) {
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kUserAgent, kBlackListUserAgent);
  FetchFromProxy("plain.html", true, request_headers, &text,
                 &response_headers, false);
  EXPECT_EQ(BlinkInfo::BLINK_DESKTOP_BLACKLIST,
            logging_info_.blink_info().blink_user_agent());
  EXPECT_STREQ(kHtmlInput, text);
  // No fetch for background computation is triggered here.
  // Only original html is fetched from fetcher.
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  // No blink flow should have happened.
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkHtmlOverThreshold) {
  // Content type is more than the limit to buffer in secondary fetch.
  int64 size_of_small_html = arraysize(kSmallHtmlInput) - 1;
  int64 html_buffer_threshold = size_of_small_html - 1;
  options_->ClearSignatureForTesting();
  options_->set_blink_max_html_size_rewritable(html_buffer_threshold);
  resource_manager()->ComputeSignature(options_.get());

  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyWaitForBackground(
      "smalltest.html", true, &text, &response_headers);

  EXPECT_STREQ(kSmallHtmlInput, text);
  VerifyBlinkInfo(BlinkInfo::FOUND_CONTENT_LENGTH_OVER_THRESHOLD,
                  "http://test.com/smalltest.html");
  // Cache lookup for original html, BlinkCriticalLineData and Dom Cohort
  // in property cache.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());

  ClearStats();
  text.clear();
  response_headers.Clear();
  options_->ClearSignatureForTesting();
  html_buffer_threshold = size_of_small_html + 1;
  options_->set_blink_max_html_size_rewritable(html_buffer_threshold);
  resource_manager()->ComputeSignature(options_.get());

  FetchFromProxyWaitForBackground(
      "smalltest.html", true, &text, &response_headers);

  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
  EXPECT_EQ(1, num_compute_calls());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkHtmlHeaderOverThreshold) {
  int64 size_of_small_html = arraysize(kSmallHtmlInput) - 1;
  int64 html_buffer_threshold = size_of_small_html;
  options_->ClearSignatureForTesting();
  options_->set_blink_max_html_size_rewritable(html_buffer_threshold);
  resource_manager()->ComputeSignature(options_.get());

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
  FetchFromProxyNoWaitForBackground(
      "smalltest.html", true, &text, &response_headers);

  VerifyBlinkInfo(BlinkInfo::FOUND_CONTENT_LENGTH_OVER_THRESHOLD,
                  "http://test.com/smalltest.html");
  // Cache lookup for original html, BlinkCriticalLineData and Dom Cohort
  // in property cache.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, NonHtmlContent) {
  // Content type is non html.
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyNoWaitForBackground(
      "plain.html", true, &text, &response_headers);

  EXPECT_STREQ(kHtmlInput, text);
  EXPECT_STREQ("text/plain",
               response_headers.Lookup1(HttpAttributes::kContentType));
  VerifyBlinkInfo(BlinkInfo::BLINK_CACHE_MISS_FOUND_RESOURCE,
                  "http://test.com/plain.html");
  // Cache lookup for original plain text, BlinkCriticalLineData and Dom Cohort
  // in property cache.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(1, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkSharedFetchesStarted)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkSharedFetchesCompleted)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlCacheMisses)->Get());

  ClearStats();
  text.clear();
  response_headers.Clear();

  FetchFromProxyNoWaitForBackground(
      "plain.html", true, &text, &response_headers);
  // Cache lookup failed for BlinkCriticalLineData and Dom Cohort in property
  // cache.
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkSharedFetchesStarted)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkSharedFetchesCompleted)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlCacheMisses)->Get());

  // Content type is html but the actual content is non html.
  FetchFromProxyNoWaitForBackground(
      "non_html.html", true, &text, &response_headers);
  EXPECT_EQ(0, num_compute_calls());
  FetchFromProxyNoWaitForBackground(
      "non_html.html", true, &text, &response_headers);
  EXPECT_EQ(0, num_compute_calls());
}

TEST_F(BlinkFlowCriticalLineTest, Non200StatusCode) {
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyNoWaitForBackground("404.html", true, &text, &response_headers);
  EXPECT_STREQ(kHtmlInput, text);
  EXPECT_STREQ("text/plain",
               response_headers.Lookup1(HttpAttributes::kContentType));
  VerifyBlinkInfo(BlinkInfo::BLINK_CACHE_MISS_FETCH_NON_OK,
                  "http://test.com/404.html");
  // Cache lookup for original plain text, BlinkCriticalLineData and Dom Cohort
  // in property cache.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());

  ClearStats();
  text.clear();
  response_headers.Clear();

  FetchFromProxyWaitForBackground("404.html", true, &text, &response_headers);
  // Cache lookup for original plain text, BlinkCriticalLineData and Dom Cohort
  // in property cache. Nothing get cached.
  EXPECT_EQ(3, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());

  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkBlacklistUserAgent) {
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kUserAgent, "BlacklistUserAgent");
  FetchFromProxy("noblink_text.html", true, request_headers, &text,
                 &response_headers, false);
  EXPECT_EQ(BlinkInfo::NOT_SUPPORT_BLINK,
            logging_info_.blink_info().blink_user_agent());
  ConstStringStarVector values;
  EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kCacheControl, &values));
  EXPECT_STREQ("max-age=0", *(values[0]));
  EXPECT_STREQ("no-cache", *(values[1]));

  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_STREQ(noblink_output_, text);
  // No blink flow should have happened.
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, TestFixedUserAgentForDesktop) {
  options_->ClearSignatureForTesting();
  options_->set_use_fixed_user_agent_for_blink_cache_misses(true);
  options_->set_blink_desktop_user_agent(kLinuxUserAgent);
  resource_manager()->ComputeSignature(options_.get());
  GoogleString text;
  GoogleString user_agent;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kUserAgent, kWindowsUserAgent);
  FetchFromProxy("text.html", true, request_headers, &text,
                 &response_headers, &user_agent, true);
  EXPECT_STREQ(kLinuxUserAgent, user_agent);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, TestNoFixedUserAgentForDesktop) {
  options_->ClearSignatureForTesting();
  options_->set_use_fixed_user_agent_for_blink_cache_misses(false);
  options_->set_blink_desktop_user_agent(kLinuxUserAgent);
  resource_manager()->ComputeSignature(options_.get());
  GoogleString text;
  GoogleString user_agent;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kUserAgent, kWindowsUserAgent);
  FetchFromProxy("text.html", true, request_headers, &text,
                 &response_headers, &user_agent, true);
  EXPECT_STREQ(kWindowsUserAgent, user_agent);
  EXPECT_EQ(1, counting_url_async_fetcher()->fetch_count());
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkMobileUserAgent) {
  GoogleString text;
  GoogleString user_agent;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  options_->ClearSignatureForTesting();
  options_->set_enable_blink_for_mobile_devices(true);
  resource_manager()->ComputeSignature(options_.get());
  request_headers.Add(HttpAttributes::kUserAgent,
                      UserAgentStrings::kIPhone4Safari);  // Mobile Request.
  FetchFromProxyWaitForBackground("text.html", true, request_headers, &text,
                                  &response_headers, &user_agent, true);
  EXPECT_EQ(BlinkInfo::BLINK_MOBILE,
            logging_info_.blink_info().blink_user_agent());
  ConstStringStarVector values;
  EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kCacheControl, &values));
  EXPECT_STREQ("max-age=0", *(values[0]));
  EXPECT_STREQ("no-cache", *(values[1]));

  EXPECT_STREQ(start_time_string_,
               response_headers.Lookup1(HttpAttributes::kDate));
  EXPECT_STREQ(kHtmlInput, text);
  EXPECT_EQ(1, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, TestNullUserAgentAndEmptyUserAgent) {
  GoogleString text;
  ResponseHeaders response_headers;
  RequestHeaders request_headers;
  request_headers.Add(HttpAttributes::kUserAgent, NULL);
  FetchFromProxy("noblink_text.html", true, request_headers, &text,
                 &response_headers, false);
  EXPECT_EQ(BlinkInfo::NULL_OR_EMPTY,
            logging_info_.blink_info().blink_user_agent());
  EXPECT_STREQ(noblink_output_, text);
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());

  request_headers.Replace(HttpAttributes::kUserAgent, "");
  FetchFromProxy("noblink_text.html", true, request_headers, &text,
                 &response_headers, false);
  EXPECT_EQ(BlinkInfo::NULL_OR_EMPTY,
            logging_info_.blink_info().blink_user_agent());
  EXPECT_STREQ(noblink_output_, text);
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkHtmlChangeDetection) {
  TestBlinkHtmlChangeDetection(false);
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkHtmlChangeDetectionLogging) {
  TestBlinkHtmlChangeDetection(true);
}

TEST_F(BlinkFlowCriticalLineTest, TestSetBlinkCriticalLineDataFalse) {
  options_->ClearSignatureForTesting();
  options_->set_enable_blink_critical_line(false);
  resource_manager()->ComputeSignature(options_.get());
  GoogleString text;
  ResponseHeaders response_headers;
  FetchFromProxyNoWaitForBackground(
      "noblink_text.html", true, &text, &response_headers);

  EXPECT_STREQ(noblink_output_, text);
  EXPECT_STREQ("text/html; charset=utf-8",
               response_headers.Lookup1(HttpAttributes::kContentType));

  // No blink flow should have happened.
  EXPECT_EQ(0, statistics()->FindVariable(
      ProxyInterface::kBlinkCriticalLineRequestCount)->Get());
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkNoNonCacheableWithCookies) {
  GoogleString text;
  ResponseHeaders response_headers;
  SetBlinkCriticalLineData();
  FetchFromProxyNoWaitForBackground(
      "cache.html", true, &text, &response_headers);
  EXPECT_STREQ(blink_output_with_cacheable_panels_cookies_, text);
}

TEST_F(BlinkFlowCriticalLineTest, TestBlinkWithLazyLoad) {
  options_->ClearSignatureForTesting();
  options_->EnableFilter(RewriteOptions::kLazyloadImages);
  options_->set_enable_lazyload_in_blink(true);
  resource_manager()->ComputeSignature(options_.get());
  GoogleString text;
  ResponseHeaders response_headers;

  // Blink Cache Miss case.
  FetchFromProxyWaitForBackground("text.html", true, &text, &response_headers);
  EXPECT_STREQ(noblink_output_with_lazy_load_, text);
  EXPECT_STREQ("text/html; charset=utf-8",
               response_headers.Lookup1(HttpAttributes::kContentType));

  ClearStats();
  // Blink Cache Hit case.
  SetBlinkCriticalLineData();
  FetchFromProxyNoWaitForBackground(
      "text.html", true, &text, &response_headers);

  UnEscapeString(&text);
  EXPECT_STREQ(blink_output_with_lazy_load_, text);
  ConstStringStarVector psa_rewriter_header_values;
  EXPECT_TRUE(response_headers.Lookup(kPsaRewriterHeader,
                                      &psa_rewriter_header_values));
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumComputeBlinkCriticalLineDataCalls)->Get());
  EXPECT_EQ(1, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlCacheHits)->Get());
}

TEST_F(BlinkFlowCriticalLineTest,
       TestBlinkHtmlChangeDetectionNon200StatusCode) {
  options_->ClearSignatureForTesting();
  options_->set_enable_blink_html_change_detection(true);
  resource_manager()->ComputeSignature(options_.get());

  GoogleString text;
  ResponseHeaders response_headers_out;

  // Cache miss case. Origin gives 404. Diff should not trigger.
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html",
                                 HttpStatus::kNotFound);
  FetchFromProxyWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlMatches)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlMismatches)->Get());

  // Cache hit case. Origin gives 404. Diff should not trigger.
  SetBlinkCriticalLineData(true, "", "");
  SetFetchHtmlResponseWithStatus("http://test.com/flaky.html",
                                 HttpStatus::kNotFound);
  FetchFromProxyWaitForBackground(
      "flaky.html", true, &text, &response_headers_out);
  EXPECT_STREQ(blink_output_with_cacheable_panels_no_cookies_, text);
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlMatches)->Get());
  EXPECT_EQ(0, statistics()->FindVariable(
      BlinkFlowCriticalLine::kNumBlinkHtmlMismatches)->Get());
}

}  // namespace net_instaweb
