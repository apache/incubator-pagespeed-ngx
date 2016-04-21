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

// Author: mmohabey@google.com (Megha Mohabey)

#ifndef PAGESPEED_AUTOMATIC_PROXY_INTERFACE_TEST_BASE_H_
#define PAGESPEED_AUTOMATIC_PROXY_INTERFACE_TEST_BASE_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "pagespeed/automatic/proxy_interface.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/empty_html_filter.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/opt/http/property_cache.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

class MockCriticalImagesFinder;

const char kPageUrl[] = "page.html";
const char kBackgroundFetchHeader[] = "X-Background-Fetch";

// Creates a proxy URL naming rule that encodes an "owner" domain and an
// "origin" domain, all inside a fixed proxy-domain.
class ProxyUrlNamer : public UrlNamer {
 public:
  static const char kProxyHost[];

  ProxyUrlNamer() : authorized_(true) {}

  // Given the request_url, generate the original url.
  bool Decode(const GoogleUrl& gurl,
              const RewriteOptions* rewrite_options,
              GoogleString* decoded) const override;

  virtual bool IsAuthorized(const GoogleUrl& gurl,
                            const RewriteOptions& options) const {
    return authorized_;
  }

  void set_authorized(bool authorized) { authorized_ = authorized; }

 private:
  bool authorized_;
  DISALLOW_COPY_AND_ASSIGN(ProxyUrlNamer);
};

// Mock filter which gets passed to the new rewrite driver created in
// proxy_fetch.
//
// This is used to check the flow for injecting data into filters via the
// ProxyInterface, including:
//     property_cache.
class MockFilter : public EmptyHtmlFilter {
 public:
  explicit MockFilter(RewriteDriver* driver)
      : driver_(driver),
        num_elements_(0),
        num_elements_property_(NULL) {
  }

  virtual void StartDocument();

  virtual void StartElement(HtmlElement* element);

  virtual void EndDocument();

  virtual const char* Name() const { return "MockFilter"; }

 private:
  RewriteDriver* driver_;
  int num_elements_;
  PropertyValue* num_elements_property_;
  DISALLOW_COPY_AND_ASSIGN(MockFilter);
};

// Hook provided to TestRewriteDriverFactory to add a new filter when
// a rewrite_driver is created.
class CreateFilterCallback
    : public TestRewriteDriverFactory::CreateFilterCallback {
 public:
  CreateFilterCallback() {}
  virtual ~CreateFilterCallback() {}

  virtual HtmlFilter* Done(RewriteDriver* driver) {
    return new MockFilter(driver);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateFilterCallback);
};

// Subclass of AsyncFetch that adds a response header indicating whether the
// fetch is for a user-facing request, or a background rewrite.
class BackgroundFetchCheckingAsyncFetch : public SharedAsyncFetch {
 public:
  explicit BackgroundFetchCheckingAsyncFetch(AsyncFetch* base_fetch)
      : SharedAsyncFetch(base_fetch),
        async_fetch_(base_fetch) {}
  virtual ~BackgroundFetchCheckingAsyncFetch() {}

  virtual void HandleHeadersComplete() {
    SharedAsyncFetch::HandleHeadersComplete();
    response_headers()->Add(kBackgroundFetchHeader,
                            async_fetch_->IsBackgroundFetch() ? "1" : "0");
    // Call ComputeCaching again since Add sets cache_fields_dirty_ to true.
    response_headers()->ComputeCaching();
  }

  virtual void HandleDone(bool success) {
    SharedAsyncFetch::HandleDone(success);
    delete this;
  }

 private:
  AsyncFetch* async_fetch_;
  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchCheckingAsyncFetch);
};

// Subclass of UrlAsyncFetcher that wraps the AsyncFetch with a
// BackgroundFetchCheckingAsyncFetch.
class BackgroundFetchCheckingUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  explicit BackgroundFetchCheckingUrlAsyncFetcher(UrlAsyncFetcher* fetcher)
      : base_fetcher_(fetcher),
        num_background_fetches_(0) {}
  virtual ~BackgroundFetchCheckingUrlAsyncFetcher() {}

  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch) {
    if (fetch->IsBackgroundFetch()) {
      num_background_fetches_++;
    }
    BackgroundFetchCheckingAsyncFetch* new_fetch =
        new BackgroundFetchCheckingAsyncFetch(fetch);
    base_fetcher_->Fetch(url, message_handler, new_fetch);
  }

  int num_background_fetches() { return num_background_fetches_; }
  void clear_num_background_fetches() { num_background_fetches_ = 0; }

 private:
  UrlAsyncFetcher* base_fetcher_;
  int num_background_fetches_;
  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchCheckingUrlAsyncFetcher);
};

class ProxyInterfaceTestBase : public RewriteTestBase {
 public:
  void TestHeadersSetupRace();

 protected:
  static const int kHtmlCacheTimeSec = 5000;

  ProxyInterfaceTestBase();
  virtual void SetUp();
  virtual void TearDown();

  void FetchFromProxy(
      const StringPiece& url,
      const RequestHeaders& request_headers,
      bool expect_success,
      GoogleString* string_out,
      ResponseHeaders* headers_out,
      bool proxy_fetch_property_callback_collector_created);

  void FetchFromProxy(const StringPiece& url,
                      const RequestHeaders& request_headers,
                      bool expect_success,
                      GoogleString* string_out,
                      ResponseHeaders* headers_out);

  void FetchFromProxy(const StringPiece& url,
                      bool expect_success,
                      GoogleString* string_out,
                      ResponseHeaders* headers_out);

  void FetchFromProxyLoggingFlushes(const StringPiece& url,
                                    bool expect_success,
                                    GoogleString* string_out);

  void FetchFromProxyNoWait(const StringPiece& url,
                            const RequestHeaders& request_headers,
                            bool expect_success,
                            bool log_flush,
                            ResponseHeaders* headers_out);

  void WaitForFetch(bool proxy_fetch_property_callback_collector_created);

  void TestPropertyCache(const StringPiece& url,
                         bool delay_pcache, bool thread_pcache,
                         bool expect_success);

  void TestPropertyCacheWithHeadersAndOutput(
      const StringPiece& url, bool delay_pcache, bool thread_pcache,
      bool expect_success, bool check_stats, bool add_create_filter_callback,
      bool expect_detach_before_pcache, const RequestHeaders& request_headers,
      ResponseHeaders* response_headers, GoogleString* output);

  void SetCriticalImagesInFinder(StringSet* critical_images);
  void SetCssCriticalImagesInFinder(StringSet* css_critical_images);

  // We retain our own request_context_ that outlives the RewriteDriver
  // created temporarily for the proxy fetch.  This allows us to reset
  // the request-context on each new ProxyFetch, potentially changing
  // request-headers that affect webp/gzip bits in the RequestContext.
  virtual RequestContextPtr request_context();

  // Setting a nonzero header-latency advances the scheduler every
  // time we initiate a new request, so that there's a latency
  // recorded in the request_context_->logging_info().
  void SetHeaderLatencyMs(int64 header_latency_ms) {
    header_latency_ms_ = header_latency_ms;
  }

  scoped_ptr<ProxyInterface> proxy_interface_;
  RequestContextPtr request_context_;
  scoped_ptr<WorkerTestBase::SyncPoint> sync_;
  ResponseHeaders callback_response_headers_;
  GoogleString callback_buffer_;
  bool callback_done_value_;
  int64 header_latency_ms_;

 private:
  friend class FilterCallback;

  MockCriticalImagesFinder* mock_critical_images_finder_;
};

}  // namespace net_instaweb
#endif  // PAGESPEED_AUTOMATIC_PROXY_INTERFACE_TEST_BASE_H_
