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

// Unit-tests for FlushEarlyFlow.

#include "net/instaweb/automatic/public/proxy_interface_test_base.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/js_disable_filter.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/split_html_filter.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class MessageHandler;
class Statistics;

namespace {

const char kCssContent[] = "* { display: none; }";

const char kFlushEarlyHtml[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"1.css\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"2.css\">"
    "<script src=\"1.js\"></script>"
    "<script src=\"2.js\"></script>"
    "<img src=\"1.jpg\"/>"
    "<script src=\"http://test.com/private.js\"></script>"
    "<script src=\"http://www.domain1.com/private.js\"></script>"
    "</head>"
    "<body>"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"3.css\">"
    "<script src=\"http://www.domain2.com/private.js\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "</body>"
    "</html>";
const char kFlushEarlyMoreResourcesInputHtml[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"1.css\">"
    "</head>"
    "<body>"
    "<script src=\"1.js\"></script>"
    "Hello, mod_pagespeed!"
    "</body>"
    "</html>";
const char kRewrittenHtml[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"%s\"></script>"
    "<script src=\"%s\"></script>"
    "<img src=\"%s\"/>"
    "<script src=\"http://test.com/private.js\"></script>"
    "<script src=\"http://www.domain1.com/private.js\"></script>"
    "</head>"
    "<body>"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"http://www.domain2.com/private.js\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "</body>"
    "</html>";
const char kFlushEarlyRewrittenHtmlImageTag[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<script type=\"text/javascript\">(function(){"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";})()</script>"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 5</script>"
    "%s"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"%s\"></script>"
    "<script src=\"%s\"></script>"
    "<img src=\"%s\"/>"
    "<script src=\"http://test.com/private.js\"></script>"
    "<script src=\"http://www.domain1.com/private.js\"></script>"
    "</head>"
    "<body>%s"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"http://www.domain2.com/private.js\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "</body>"
    "</html>";
const char kFlushEarlyRewrittenHtmlImageTagInsertDnsPrefetch[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<script type=\"text/javascript\">(function(){"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";})()</script>"
    "<link rel=\"dns-prefetch\" href=\"//test.com\">"
    "<link rel=\"dns-prefetch\" href=\"//www.domain1.com\">"
    "<link rel=\"dns-prefetch\" href=\"//www.domain2.com\">"
    "<link rel=\"dns-prefetch\" href=\"//www.domain3.com\">"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 5</script>"
    "%s"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"%s\"></script>"
    "<script src=\"%s\"></script>"
    "<img src=\"%s\"/>"
    "<script src=\"http://test.com/private.js\"></script>"
    "<script src=\"http://www.domain1.com/private.js\"></script>"
    "</head>"
    "<body>%s"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"http://www.domain2.com/private.js\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "</body>"
    "</html>";
const char kFlushEarlyRewrittenHtmlLinkRelSubresource[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 5</script>"
    "%s"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"%s\"></script>"
    "<script src=\"%s\"></script>"
    "<img src=\"%s\"/>"
    "<script src=\"http://test.com/private.js\"></script>"
    "<script src=\"http://www.domain1.com/private.js\"></script>"
    "</head>"
    "<body>%s"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"http://www.domain2.com/private.js\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "</body>"
    "</html>";
const char kFlushEarlyRewrittenHtmlWithScriptPsaOff[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 5</script>"
    "<script type=\"text/javascript\">"
    "window.location.replace(\"http://test.com/?ModPagespeed=noscript\")"
    "</script>"
    "</head><body></body></html>";
const char kFlushEarlyRewrittenHtmlLinkScript[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<link rel=\"stylesheet\" href=\"%s\" media=\"print\" disabled=\"true\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\" media=\"print\" disabled=\"true\"/>\n"
    "<script type=\"psa_prefetch\" src=\"%s\"></script>\n"
    "<script type=\"psa_prefetch\" src=\"%s\"></script>\n"
    "<link rel=\"stylesheet\" href=\"%s\" media=\"print\" disabled=\"true\"/>\n"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 5</script>"
    "%s"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"%s\"></script>"
    "<script src=\"%s\"></script>"
    "<img src=\"%s\"/>"
    "<script src=\"http://test.com/private.js\"></script>"
    "<script src=\"http://www.domain1.com/private.js\"></script>"
    "</head>"
    "<body>%s"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"http://www.domain2.com/private.js\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "</body>"
    "</html>";
const char kRewrittenHtmlLazyloadDeferJsScriptFlushedEarly[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<link rel=\"stylesheet\" href=\"%s\" media=\"print\" disabled=\"true\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\" media=\"print\" disabled=\"true\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\" media=\"print\" disabled=\"true\"/>\n"
    "<script type=\"psa_prefetch\" src=\"/psajs/js_defer.0.js\"></script>\n"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 4</script>"
    "<script type=\"text/javascript\">%s</script>"
    "%s"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script pagespeed_orig_src=\"%s\" type=\"text/psajs\" orig_index=\"0\">"
    "</script>"
    "<script pagespeed_orig_src=\"%s\" type=\"text/psajs\" orig_index=\"1\">"
    "</script>"
    "<img pagespeed_lazy_src=\"%s\""
    " src=\"data:image/gif;"
    "base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==\""
    " onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\"/>"
    "<script type=\"text/javascript\" pagespeed_no_defer=\"\">"
    "pagespeed.lazyLoadImages.overrideAttributeFunctions();</script>"
    "<script pagespeed_orig_src=\"http://test.com/private.js\""
    " type=\"text/psajs\""
    " orig_index=\"2\"></script>"
    "<script pagespeed_orig_src=\"http://www.domain1.com/private.js\""
    " type=\"text/psajs\" orig_index=\"3\"></script>"
    "%s</head>"
    "<body>%s"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script pagespeed_orig_src=\"http://www.domain2.com/private.js\""
    " type=\"text/psajs\" orig_index=\"4\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "</body>"
    "</html>%s";
const char kRewrittenSplitHtmlWithLazyloadScriptFlushedEarly[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<link rel=\"stylesheet\" href=\"%s\" media=\"print\" disabled=\"true\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\" media=\"print\" disabled=\"true\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\" media=\"print\" disabled=\"true\"/>\n"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 3</script>"
    "%s"
    "%s"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script pagespeed_orig_src=\"%s\" type=\"text/psajs\" orig_index=\"0\">"
    "</script>"
    "<script pagespeed_orig_src=\"%s\" type=\"text/psajs\" orig_index=\"1\">"
    "</script>"
    "%s"
    "<script pagespeed_orig_src=\"http://test.com/private.js\""
    " type=\"text/psajs\""
    " orig_index=\"2\"></script>"
    "<script pagespeed_orig_src=\"http://www.domain1.com/private.js\""
    " type=\"text/psajs\" orig_index=\"3\"></script>%s"
    "</head>"
    "<body>%s"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script pagespeed_orig_src=\"http://www.domain2.com/private.js\""
    " type=\"text/psajs\" orig_index=\"4\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "</body>"
    "</html>"
    "<script type=\"text/javascript\">pagespeed.num_low_res_images_inlined=0;"
    "</script><script type=\"text/javascript\" src=\"/psajs/blink.js\">"
    "</script>"
    "<script type=\"text/javascript\">"
      "pagespeed.panelLoaderInit();"
      "pagespeed.panelLoader.invokedFromSplit();"
      "pagespeed.panelLoader.loadCriticalData({});"
      "pagespeed.panelLoader.bufferNonCriticalData({});"
    "</script>\n</body></html>\n";
const char kRewrittenPageSpeedLazyImg[] = "<img pagespeed_lazy_src=\"%s\""
    " src=\"data:image/gif;"
    "base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==\""
    " onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\"/>"
    "<script type=\"text/javascript\" pagespeed_no_defer=\"\">"
    "pagespeed.lazyLoadImages.overrideAttributeFunctions();</script>";
const char kRewrittenPageSpeedImg[] = "<img src=\"%s\"/>";
const char kFlushEarlyRewrittenHtmlImageTagWithDeferJs[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<script type=\"text/javascript\">(function(){"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";})()</script>"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 3</script>"
    "%s"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"%s\"></script>"
    "<script src=\"%s\"></script>"
    "<img src=\"%s\"/>"
    "<script src=\"http://test.com/private.js\"></script>"
    "<script src=\"http://www.domain1.com/private.js\"></script>"
    "</head>"
    "<body>%s"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"http://www.domain2.com/private.js\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "</body>"
    "</html>";
const char kFlushEarlyRewrittenHtmlLinkRelSubresourceWithDeferJs[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<link rel=\"subresource\" href=\"%s\"/>\n"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 3</script>"
    "%s"
    "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
    "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
    "<meta charset=\"UTF-8\"/>"
    "<title>Flush Subresources Early example</title>"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"%s\"></script>"
    "<script src=\"%s\"></script>"
    "<img src=\"%s\"/>"
    "<script src=\"http://test.com/private.js\"></script>"
    "<script src=\"http://www.domain1.com/private.js\"></script>"
    "</head>"
    "<body>%s"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script src=\"http://www.domain2.com/private.js\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "</body>"
    "</html>";

class FakeCriticalImagesFinder : public CriticalImagesFinder {
 public:
  explicit FakeCriticalImagesFinder(Statistics* stats)
      : CriticalImagesFinder(stats) {}
  ~FakeCriticalImagesFinder() {}

  virtual bool IsMeaningful(const RewriteDriver* driver) const { return true; }

  virtual void UpdateCriticalImagesSetInDriver(RewriteDriver* driver) {
    if (css_critical_images_ != NULL) {
      StringSet* css_critical_images = new StringSet;
      *css_critical_images = *css_critical_images_;
      driver->set_css_critical_images(css_critical_images);
    }
  }

  virtual void ComputeCriticalImages(StringPiece url,
                                     RewriteDriver* driver) {
    // Do Nothing
  }

  virtual const char* GetCriticalImagesCohort() const {
    return "critical_images";
  }

  void set_css_critical_images(StringSet* css_critical_images) {
    css_critical_images_.reset(css_critical_images);
  }

 private:
  scoped_ptr<StringSet> css_critical_images_;
  DISALLOW_COPY_AND_ASSIGN(FakeCriticalImagesFinder);
};

class LatencyUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  explicit LatencyUrlAsyncFetcher(UrlAsyncFetcher* fetcher)
      : base_fetcher_(fetcher),
        latency_ms_(-1) {}
  virtual ~LatencyUrlAsyncFetcher() {}

  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch) {
    if (latency_ms_ != -1) {
      fetch->log_record()->logging_info()->mutable_timing_info()->
          set_header_fetch_ms(latency_ms_);
    }
    base_fetcher_->Fetch(url, message_handler, fetch);
  }

  void set_latency(int64 latency) { latency_ms_ = latency; }

 private:
  UrlAsyncFetcher* base_fetcher_;
  int64 latency_ms_;
  DISALLOW_COPY_AND_ASSIGN(LatencyUrlAsyncFetcher);
};

}  // namespace

class FlushEarlyFlowTest : public ProxyInterfaceTestBase {
 protected:
  static const int kHtmlCacheTimeSec = 5000;

  FlushEarlyFlowTest()
      : fake_critical_images_finder_(
          new FakeCriticalImagesFinder(statistics())),
        start_time_ms_(0),
        max_age_300_("max-age=300"),
        request_start_time_ms_(-1) {
    ConvertTimeToString(MockTimer::kApr_5_2010_ms, &start_time_string_);
    ConvertTimeToString(MockTimer::kApr_5_2010_ms + 5 * Timer::kMinuteMs,
                        &start_time_plus_300s_string_);
    ConvertTimeToString(MockTimer::kApr_5_2010_ms - 2 * Timer::kDayMs,
                        &old_time_string_);
  }
  virtual ~FlushEarlyFlowTest() {}

  virtual void SetUp() {
    RewriteOptions* options = server_context()->global_options();
    server_context_->set_enable_property_cache(true);
    SetupCohort(page_property_cache(), RewriteDriver::kDomCohort);
    options->ClearSignatureForTesting();
    options->set_max_html_cache_time_ms(kHtmlCacheTimeSec * Timer::kSecondMs);
    options->set_ajax_rewriting_enabled(true);
    server_context()->ComputeSignature(options);
    ProxyInterfaceTestBase::SetUp();
    // The original url_async_fetcher() is still owned by RewriteDriverFactory.
    background_fetch_fetcher_.reset(new BackgroundFetchCheckingUrlAsyncFetcher(
        factory()->ComputeUrlAsyncFetcher()));
    latency_fetcher_.reset(
        new LatencyUrlAsyncFetcher(background_fetch_fetcher_.get()));
    server_context()->set_default_system_fetcher(latency_fetcher_.get());
    server_context()->set_critical_images_finder(fake_critical_images_finder_);

    start_time_ms_ = timer()->NowMs();
  }

  void SetupForFlushEarlyFlow(bool enable_experimental) {
    // Setup
    ResponseHeaders headers;
    headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
    headers.Add(HttpAttributes::kSetCookie, "CG=US:CA:Mountain+View");
    headers.Add(HttpAttributes::kSetCookie, "UA=chrome");
    headers.Add(HttpAttributes::kSetCookie, "path=/");

    headers.SetStatusAndReason(HttpStatus::kOK);
    mock_url_fetcher_.SetResponse(kTestDomain, headers, kFlushEarlyHtml);

    // Enable FlushSubresourcesFilter filter.
    RewriteOptions* rewrite_options = server_context()->global_options();
    rewrite_options->ClearSignatureForTesting();
    rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);
    rewrite_options->EnableFilter(RewriteOptions::kCombineCss);
    rewrite_options->EnableFilter(RewriteOptions::kCombineJavascript);
    rewrite_options->set_enable_flush_subresources_experimental(
        enable_experimental);
    rewrite_options->EnableExtendCacheFilters();
    // Disabling the inline filters so that the resources get flushed early
    // else our dummy resources are too small and always get inlined.
    rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
    rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
    rewrite_options->ComputeSignature(hasher());

    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.css"), kContentTypeCss,
                                  kCssContent, kHtmlCacheTimeSec * 2);
    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "2.css"), kContentTypeCss,
                                  kCssContent, kHtmlCacheTimeSec * 2);
    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "3.css"), kContentTypeCss,
                                  kCssContent, kHtmlCacheTimeSec * 2);
    const char kContent[] = "function f() {alert('foo');}";
    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.js"),
                                  kContentTypeJavascript, kContent,
                                  kHtmlCacheTimeSec * 2);
    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "2.js"),
                                  kContentTypeJavascript, kContent,
                                  kHtmlCacheTimeSec * 2);
    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.jpg"),
                                  kContentTypeJpeg, "image",
                                  kHtmlCacheTimeSec * 2);
    ResponseHeaders private_headers;
    DefaultResponseHeaders(kContentTypeJavascript, kHtmlCacheTimeSec,
                           &private_headers);
    private_headers.SetDateAndCaching(http_cache()->timer()->NowMs(),
                                      300 * Timer::kSecondMs, ", private");
    private_headers.ComputeCaching();
    SetFetchResponse(AbsolutifyUrl("private.js"), private_headers, "a");
  }

  void VerifyCharset(ResponseHeaders* headers) {
    EXPECT_TRUE(StringCaseEqual(headers->Lookup1(HttpAttributes::kContentType),
                                "text/html; charset=utf-8"));
  }

  GoogleString FlushEarlyRewrittenHtml(
      UserAgentMatcher::PrefetchMechanism value,
      bool defer_js_enabled, bool insert_dns_prefetch) {
    return FlushEarlyRewrittenHtml(value, defer_js_enabled,
                                   insert_dns_prefetch, false, true, false);
  }

  GoogleString GetDeferJsCode() {
    return StrCat("<script type=\"text/javascript\" src=\"",
                  server_context()->static_javascript_manager()->GetJsUrl(
                      StaticJavascriptManager::kDeferJs, options_),
                  "\"></script>");
  }

  GoogleString FlushEarlyRewrittenHtml(
      UserAgentMatcher::PrefetchMechanism value,
      bool defer_js_enabled, bool insert_dns_prefetch,
      bool split_html_enabled, bool lazyload_enabled, bool redirect_psa_off) {
    GoogleString rewritten_css_url_1 = Encode(kTestDomain,
                                              "cf", "0", "1.css", "css");
    GoogleString rewritten_css_url_2 = Encode(kTestDomain,
                                              "cf", "0", "2.css", "css");
    GoogleString rewritten_css_url_3 = Encode(kTestDomain,
                                              "cf", "0", "3.css", "css");
    GoogleString rewritten_js_url_1 = Encode(kTestDomain,
                                             "jm", "0", "1.js", "js");
    GoogleString rewritten_js_url_2 = Encode(kTestDomain,
                                             "jm", "0", "2.js", "js");
    GoogleString combined_js_url = Encode(
        kTestDomain, "jc", "0",
        "1.js.pagespeed.jm.0.jsX2.js.pagespeed.jm.0.js", "js");
    GoogleString rewritten_img_url_1 = Encode(kTestDomain,
                                              "ce", "0", "1.jpg", "jpg");
    GoogleString redirect_url = StrCat(kTestDomain, "?ModPagespeed=noscript");
    GoogleString cookie_script =
        "<script type=\"text/javascript\" pagespeed_no_defer=\"\">"
        "(function(){"
          "var data = [\"CG=US:CA:Mountain+View\",\"UA=chrome\",\"path=/\"];"
          "for (var i = 0; i < data.length; i++) {"
          "document.cookie = data[i];"
         "}})()"
        "</script>";
    combined_js_url[combined_js_url.find('X')] = '+';
    if (value == UserAgentMatcher::kPrefetchLinkScriptTag && defer_js_enabled) {
      return StringPrintf(
          kRewrittenHtmlLazyloadDeferJsScriptFlushedEarly,
          rewritten_css_url_1.data(), rewritten_css_url_2.data(),
          rewritten_css_url_3.data(),
          LazyloadImagesFilter::GetLazyloadJsSnippet(
              options_,
              server_context()->static_javascript_manager()).c_str(),
          cookie_script.data(),
          rewritten_css_url_1.data(), rewritten_css_url_2.data(),
          rewritten_js_url_1.data(), rewritten_js_url_2.data(),
          rewritten_img_url_1.data(),
          StrCat("<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
                 JsDisableFilter::GetJsDisableScriptSnippet(options_),
                 "</script>").c_str(),
          StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                       redirect_url.c_str()).c_str(),
          rewritten_css_url_3.data(),
          GetDeferJsCode().c_str());
    } else if (value == UserAgentMatcher::kPrefetchLinkScriptTag &&
        split_html_enabled) {
      return StringPrintf(
          kRewrittenSplitHtmlWithLazyloadScriptFlushedEarly,
          rewritten_css_url_1.data(), rewritten_css_url_2.data(),
          rewritten_css_url_3.data(),
          (lazyload_enabled ?
              StrCat("<script type=\"text/javascript\">",
                     LazyloadImagesFilter::GetLazyloadJsSnippet(
                         options_,
                         server_context()->static_javascript_manager()),
                     "</script>").c_str() : ""),
          cookie_script.data(),
          rewritten_css_url_1.data(), rewritten_css_url_2.data(),
          rewritten_js_url_1.data(), rewritten_js_url_2.data(),
          lazyload_enabled ?
              StringPrintf(
                  kRewrittenPageSpeedLazyImg,
                  rewritten_img_url_1.data()).c_str() :
              StringPrintf(kRewrittenPageSpeedImg,
                           rewritten_img_url_1.data()).c_str(),
          StrCat("<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
                 JsDisableFilter::GetJsDisableScriptSnippet(options_),
                 "</script>",
                 lazyload_enabled ? "" : SplitHtmlFilter::kPagespeedFunc,
                 SplitHtmlFilter::kSplitInit).c_str(),
          StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                                 redirect_url.c_str()).c_str(),
          rewritten_css_url_3.data());
    } else if (value == UserAgentMatcher::kPrefetchNotSupported) {
      return StringPrintf(kRewrittenHtml, rewritten_css_url_1.data(),
          rewritten_css_url_2.data(), rewritten_js_url_1.data(),
          rewritten_js_url_2.data(), rewritten_img_url_1.data(),
          rewritten_css_url_3.data());
    } else if (defer_js_enabled) {
      return StringPrintf(
          value == UserAgentMatcher::kPrefetchLinkRelSubresource ?
          kFlushEarlyRewrittenHtmlLinkRelSubresourceWithDeferJs :
          kFlushEarlyRewrittenHtmlImageTagWithDeferJs,
          rewritten_css_url_1.data(), rewritten_css_url_2.data(),
          rewritten_css_url_3.data(), cookie_script.data(),
          rewritten_css_url_1.data(), rewritten_css_url_2.data(),
          rewritten_js_url_1.data(), rewritten_js_url_2.data(),
          rewritten_img_url_1.data(),
          StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                       redirect_url.c_str()).c_str(),
          rewritten_css_url_3.data());
    } else if (redirect_psa_off) {
      return StringPrintf(
          kFlushEarlyRewrittenHtmlWithScriptPsaOff,
          rewritten_css_url_1.data(), rewritten_css_url_2.data(),
          rewritten_js_url_1.data(), rewritten_js_url_2.data(),
          rewritten_css_url_3.data());
    } else {
      GoogleString output_format;
      if (insert_dns_prefetch) {
        output_format = kFlushEarlyRewrittenHtmlImageTagInsertDnsPrefetch;
      } else if (value == UserAgentMatcher::kPrefetchLinkRelSubresource) {
        output_format = kFlushEarlyRewrittenHtmlLinkRelSubresource;
      } else if (value == UserAgentMatcher::kPrefetchLinkScriptTag) {
        output_format = kFlushEarlyRewrittenHtmlLinkScript;
      } else {
        output_format = kFlushEarlyRewrittenHtmlImageTag;
      }
      return StringPrintf(
          output_format.c_str(),
          rewritten_css_url_1.data(), rewritten_css_url_2.data(),
          rewritten_js_url_1.data(), rewritten_js_url_2.data(),
          rewritten_css_url_3.data(), cookie_script.data(),
          rewritten_css_url_1.data(), rewritten_css_url_2.data(),
          rewritten_js_url_1.data(), rewritten_js_url_2.data(),
          rewritten_img_url_1.data(),
          StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                                 redirect_url.c_str()).c_str(),
          rewritten_css_url_3.data());
    }
  }

  void ExperimentalFlushEarlyFlowTestHelper(
      const GoogleString& user_agent,
      UserAgentMatcher::PrefetchMechanism mechanism, bool inject_error) {
    ExperimentalFlushEarlyFlowTestHelperWithPropertyCache(
        user_agent, mechanism, false, false, inject_error);
    ExperimentalFlushEarlyFlowTestHelperWithPropertyCache(
        user_agent, mechanism, false, true, inject_error);
    ExperimentalFlushEarlyFlowTestHelperWithPropertyCache(
        user_agent, mechanism, true, true, inject_error);
    ExperimentalFlushEarlyFlowTestHelperWithPropertyCache(
        user_agent, mechanism, true, false, inject_error);
  }

  void ExperimentalFlushEarlyFlowTestHelperWithPropertyCache(
      const GoogleString& user_agent,
      UserAgentMatcher::PrefetchMechanism mechanism,
      bool delay_pcache, bool thread_pcache, bool inject_error) {
    lru_cache()->Clear();
    SetupForFlushEarlyFlow(true);
    GoogleString text;
    RequestHeaders request_headers;
    request_headers.Replace(HttpAttributes::kUserAgent, user_agent);
    ResponseHeaders headers;
    TestPropertyCacheWithHeadersAndOutput(
        kTestDomain, delay_pcache, thread_pcache, true, false, false,
        false, request_headers, &headers, &text);

    if (inject_error) {
      ResponseHeaders error_headers;
      error_headers.SetStatusAndReason(HttpStatus::kOK);
      mock_url_fetcher_.SetResponse(
          kTestDomain, error_headers, "");
    }

    // Fetch the url again. This time FlushEarlyFlow should not be triggered.
    // None
    TestPropertyCacheWithHeadersAndOutput(
        kTestDomain, delay_pcache, thread_pcache, true, false, false,
        inject_error, request_headers, &headers, &text);
    GoogleString expected_output = FlushEarlyRewrittenHtml(mechanism,
                                                           false, false);
    if (!inject_error) {
      EXPECT_EQ(expected_output, text);
      VerifyCharset(&headers);
    }
  }

  scoped_ptr<BackgroundFetchCheckingUrlAsyncFetcher> background_fetch_fetcher_;
  scoped_ptr<LatencyUrlAsyncFetcher> latency_fetcher_;
  FakeCriticalImagesFinder* fake_critical_images_finder_;
  int64 start_time_ms_;
  GoogleString start_time_string_;
  GoogleString start_time_plus_300s_string_;
  GoogleString old_time_string_;
  const GoogleString max_age_300_;
  int64 request_start_time_ms_;
};

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTest) {
  SetupForFlushEarlyFlow(false);
  GoogleString text;
  RequestHeaders request_headers;
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  // Check total number of cache inserts.
  // 7 for 1.css, 2.css, 3.css, 1.js, 2.js, 1.jpg and private.js.
  // 6 for A.1.css.pagespeed.cf.0.css, A.2.css.pagespeed.cf.0.css,
  //       A.3.css.pagespeed.cf.0.css, 1.js.pagespeed.jm.0.js and
  //       2.js.pagespeed.jm.0.js and 1.jpg.pagespeed.ce.0.jpg.
  // 19 metadata cache enties - three for cf and jm, seven for ce and
  //       six for fs.
  // 1 for DomCohort write in property cache.
  EXPECT_EQ(33, lru_cache()->num_inserts());

  // Fetch the url again. This time FlushEarlyFlow should not be triggered.
  // None
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchNotSupported, false, false), text);
  VerifyCharset(&headers);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestPrefetch) {
  SetupForFlushEarlyFlow(false);
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_rel_subresource");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  // Check total number of cache inserts.
  // 7 for 1.css, 2.css, 3.css, 1.js, 2.js, 1.jpg and private.js.
  // 6 for A.1.css.pagespeed.cf.0.css, A.2.css.pagespeed.cf.0.css,
  //       A.3.css.pagespeed.cf.0.css, 1.js.pagespeed.jm.0.js and
  //       2.js.pagespeed.jm.0.js and 1.jpg.pagespeed.ce.0.jpg.
  // 19 metadata cache enties - three for cf and jm, seven for ce and
  //       six for fs.
  // 1 for DomCohort write in property cache.
  EXPECT_EQ(33, lru_cache()->num_inserts());

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkRelSubresource, false, false),
      text);
  EXPECT_STREQ("cf,ei,jm", logging_info()->applied_rewriters());
  VerifyCharset(&headers);
}

// TODO(rahulbansal): Remove the flakiness and uncomment this.
/*
TEST_F(FlushEarlyFlowTest, FlushEarlyFlowStatusCodeUnstable) {
  // Test that the flush early flow is not triggered when the status code is
  // unstable.
  SetupForFlushEarlyFlow(true);
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_rel_subresource");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkRelSubresource, false, false),
      text);
  EXPECT_EQ(0, statistics()->FindVariable(
      FlushEarlyFlow::kNumFlushEarlyRequestsRedirected)->Get());

  SetFetchResponse404(kTestDomain);
  // Fetch again so that 404 is populated in response headers.
  // It should redirect to ModPagespeed=noscript in this case.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkRelSubresource, false, false, false,
      false, true), text);
  EXPECT_EQ(1, statistics()->FindVariable(
      FlushEarlyFlow::kNumFlushEarlyRequestsRedirected)->Get());

  // Fetch the url again. This time FlushEarlyFlow should not be triggered as
  // the status code is not stable.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(HttpStatus::kNotFound, headers.status_code());

  // Delete the 404 form cache and again set up for 200 response.
  lru_cache()->Delete(kTestDomain);
  SetupForFlushEarlyFlow(true);

  // Flush early flow is again not triggered as the status code is not
  // stable for property_cache_http_status_stability_threshold number of
  // requests.
  for (int i = 0, n = server_context()->global_options()->
       property_cache_http_status_stability_threshold(); i < n; ++i) {
    FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
    EXPECT_TRUE(text.find("link rel=\"subresource\"") == GoogleString::npos);
  }
  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkRelSubresource, false, false),
      text);

  // Fetch again so that 404 is populated in response headers.
  // It should redirect to ModPagespeed=noscript in this case.
  // This case simulates the scenario when fetch finishes before the flush
  // early flow is done.
  SetFetchResponse404(kTestDomain);
  TestPropertyCacheWithHeadersAndOutput(
       kTestDomain, true, true, true, false, false, false,
       request_headers, &headers, &text);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkRelSubresource, false, false, false,
      false, true), text);
  EXPECT_EQ(2, statistics()->FindVariable(
      FlushEarlyFlow::kNumFlushEarlyRequestsRedirected)->Get());
}
*/

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestImageTag) {
  SetupForFlushEarlyFlow(false);
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent, "prefetch_image_tag");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  // Check total number of cache inserts.
  // 7 for 1.css, 2.css, 3.css, 1.js, 2.js, 1.jpg and private.js.
  // 6 for I.1.css.pagespeed.cf.0.css, I.2.css.pagespeed.cf.0.css,
  //       I.3.css.pagespeed.cf.0.css, 1.js.pagespeed.jm.0.js and
  //       2.js.pagespeed.jm.0.js and 1.jpg.pagespeed.ce.0.jpg.
  // 19 metadata cache enties - three for cf and jm, seven for ce and
  //       six for fs.
  // 1 for DomCohort write in property cache.
  EXPECT_EQ(33, lru_cache()->num_inserts());

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchImageTag, false, false), text);
  VerifyCharset(&headers);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestLinkScript) {
  SetupForFlushEarlyFlow(false);
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_script_tag");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  // Check total number of cache inserts.
  // 7 for 1.css, 2.css, 3.css, 1.js, 2.js, 1.jpg and private.js.
  // 6 for I.1.css.pagespeed.cf.0.css, I.2.css.pagespeed.cf.0.css,
  //       I.3.css.pagespeed.cf.0.css, 1.js.pagespeed.jm.0.js and
  //       2.js.pagespeed.jm.0.js and 1.jpg.pagespeed.ce.0.jpg.
  // 19 metadata cache enties - three for cf and jm, seven for ce and
  //       six for fs.
  // 1 for DomCohort write in property cache.
  EXPECT_EQ(33, lru_cache()->num_inserts());

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkScriptTag, false, false), text);
  VerifyCharset(&headers);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestWithDeferJsImageTag) {
  SetupForFlushEarlyFlow(false);
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent, "prefetch_image_tag");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchImageTag, true, false), text);
  VerifyCharset(&headers);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestWithDeferJsPrefetch) {
  SetupForFlushEarlyFlow(false);
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_rel_subresource");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkRelSubresource, true, false), text);
  VerifyCharset(&headers);
}

TEST_F(FlushEarlyFlowTest, ExperimentalFlushEarlyFlowTest) {
  ExperimentalFlushEarlyFlowTestHelper(
      "", UserAgentMatcher::kPrefetchNotSupported, false);
}

TEST_F(FlushEarlyFlowTest, ExperimentalFlushEarlyFlowTestError) {
  ExperimentalFlushEarlyFlowTestHelper(
      "", UserAgentMatcher::kPrefetchNotSupported, true);
}

TEST_F(FlushEarlyFlowTest, ExperimentalFlushEarlyFlowTestPrefetch) {
  ExperimentalFlushEarlyFlowTestHelper(
      "prefetch_link_rel_subresource",
      UserAgentMatcher::kPrefetchLinkRelSubresource, false);
}

TEST_F(FlushEarlyFlowTest, ExperimentalFlushEarlyFlowTestPrefetchError) {
  ExperimentalFlushEarlyFlowTestHelper(
       "prefetch_link_rel_subresource",
      UserAgentMatcher::kPrefetchLinkRelSubresource, true);
}

TEST_F(FlushEarlyFlowTest, ExperimentalFlushEarlyFlowTestImageTag) {
  ExperimentalFlushEarlyFlowTestHelper(
      "prefetch_image_tag", UserAgentMatcher::kPrefetchImageTag, false);
}

TEST_F(FlushEarlyFlowTest, ExperimentalFlushEarlyFlowTestImageTagError) {
  ExperimentalFlushEarlyFlowTestHelper(
      "prefetch_image_tag", UserAgentMatcher::kPrefetchImageTag, true);
}

TEST_F(FlushEarlyFlowTest, ExperimentalFlushEarlyFlowTestLinkScript) {
  ExperimentalFlushEarlyFlowTestHelper(
      "prefetch_link_script_tag", UserAgentMatcher::kPrefetchLinkScriptTag,
      false);
}

TEST_F(FlushEarlyFlowTest, ExperimentalFlushEarlyFlowTestLinkScriptError) {
  ExperimentalFlushEarlyFlowTestHelper(
      "prefetch_link_script_tag", UserAgentMatcher::kPrefetchLinkScriptTag,
      true);
}

TEST_F(FlushEarlyFlowTest, ExperimentalFlushEarlyFlowTestWithDeferJsImageTag) {
  SetupForFlushEarlyFlow(true);
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent, "prefetch_image_tag");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchImageTag, true, false), text);
  VerifyCharset(&headers);
}

TEST_F(FlushEarlyFlowTest, ExperimentalFlushEarlyFlowTestWithDeferJsPrefetch) {
  SetupForFlushEarlyFlow(true);
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_rel_subresource");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkRelSubresource, true, false), text);
  VerifyCharset(&headers);
}

TEST_F(FlushEarlyFlowTest,
       ExperimentalFlushEarlyFlowTestWithInsertDnsPrefetch) {
  SetupForFlushEarlyFlow(true);
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kInsertDnsPrefetch);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent, "prefetch_image_tag");
  ResponseHeaders headers;

  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered but not
  // insert dns prefetch filter as domains are not yet stable.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time InsertDnsPrefetch filter should applied.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchImageTag, false, true), text);
}

TEST_F(FlushEarlyFlowTest, LazyloadAndDeferJsScriptFlushedEarly) {
  latency_fetcher_->set_latency(600);
  SetupForFlushEarlyFlow(true);
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  custom_options->EnableFilter(RewriteOptions::kLazyloadImages);
  custom_options->set_flush_more_resources_early_if_time_permits(true);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);
  GoogleString text;
  RequestHeaders request_headers;
  // Useragent is set to Firefox/ 9.0 because all flush early flow, defer
  // javascript and lazyload filter are enabled for this user agent.
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "Firefox/ 9.0");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkScriptTag, true, false), text);
}

TEST_F(FlushEarlyFlowTest, SplitHtmlWithLazyloadScriptFlushedEarly) {
  SetupForFlushEarlyFlow(true);
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  custom_options->EnableFilter(RewriteOptions::kSplitHtml);
  custom_options->EnableFilter(RewriteOptions::kLazyloadImages);
  custom_options->set_critical_line_config(
       "div[@id = \"container\"]/div[4],"
       "img[3]:h1[@id = \"footer\"]");
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);
  GoogleString text;
  RequestHeaders request_headers;
  // Useragent is set to Firefox/ 9.0 because all flush early flow, defer
  // javascript and lazyload filter are enabled for this user agent.
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "Firefox/ 9.0");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkScriptTag, false, false, true, true,
      false), text);
}

TEST_F(FlushEarlyFlowTest, SplitHtmlWithLazyloadScriptNotFlushedEarly) {
  SetupForFlushEarlyFlow(true);
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  custom_options->EnableFilter(RewriteOptions::kSplitHtml);
  custom_options->set_critical_line_config(
       "div[@id = \"container\"]/div[4],"
       "img[3]:h1[@id = \"footer\"]");
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);
  GoogleString text;
  RequestHeaders request_headers;
  // Useragent is set to Firefox/ 9.0 because all flush early flow, defer
  // javascript and lazyload filter are enabled for this user agent.
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "Firefox/ 9.0");
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkScriptTag, false, false, true, false,
      false), text);
}

TEST_F(FlushEarlyFlowTest, NoLazyloadScriptFlushedOutIfNoImagePresent) {
  const char kInputHtml[] =
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
      "<meta charset=\"UTF-8\"/>"
      "<title>Flush Subresources Early example</title>"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"1.css\">"
      "</head>"
      "<body>"
      "Hello, mod_pagespeed!"
      "</body>"
      "</html>";

  GoogleString redirect_url = StrCat(kTestDomain, "?ModPagespeed=noscript");
  GoogleString kOutputHtml = StrCat(
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<link rel=\"stylesheet\""
      " href=\"http://test.com/A.1.css.pagespeed.cf.0.css\""
      " media=\"print\" disabled=\"true\"/>\n"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_prefetch_start = Number(new Date());"
      "window.mod_pagespeed_num_resources_prefetched = 1"
      "</script>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
      "<meta charset=\"UTF-8\"/>"
      "<title>Flush Subresources Early example</title>"
      "<link rel=\"stylesheet\" type=\"text/css\""
      " href=\"http://test.com/A.1.css.pagespeed.cf.0.css\"></head>"
      "<body>",
      StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                   redirect_url.c_str()),
      "Hello, mod_pagespeed!</body></html>");

  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(kTestDomain, headers, kInputHtml);

  // Enable FlushSubresourcesFilter filter.
  RewriteOptions* rewrite_options = server_context()->global_options();
  rewrite_options->ClearSignatureForTesting();
  rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);
  rewrite_options->set_enable_flush_subresources_experimental(true);
  rewrite_options->EnableExtendCacheFilters();
  // Disabling the inline filters so that the resources get flushed early
  // else our dummy resources are too small and always get inlined.
  rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
  rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
  rewrite_options->ComputeSignature(hasher());

  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.css"), kContentTypeCss,
                                kCssContent, kHtmlCacheTimeSec * 2);

  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kLazyloadImages);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  GoogleString text;
  RequestHeaders request_headers;
  // Useragent is set to Firefox/ 9.0 because all flush early flow, defer
  // javascript and lazyload filter is enabled for this user agent.
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "Firefox/ 9.0");

  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(kOutputHtml, text);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyMoreResourcesIfTimePermits) {
  latency_fetcher_->set_latency(600);
  StringSet* css_critical_images = new StringSet;
  css_critical_images->insert(StrCat(kTestDomain, "1.jpg"));
  fake_critical_images_finder_->set_css_critical_images(css_critical_images);
  GoogleString redirect_url = StrCat(kTestDomain, "?ModPagespeed=noscript");

  GoogleString kOutputHtml = StrCat(
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"http://test.com/A.1.css.pagespeed.cf.0.css\";"
      "new Image().src=\"http://test.com/1.jpg.pagespeed.ce.0.jpg\";"
      "new Image().src=\"http://test.com/1.js.pagespeed.ce.0.js\";})()</script>"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_prefetch_start = Number(new Date());"
      "window.mod_pagespeed_num_resources_prefetched = 3"
      "</script>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
      "<meta charset=\"UTF-8\"/>"
      "<title>Flush Subresources Early example</title>"
      "<link rel=\"stylesheet\" type=\"text/css\""
      " href=\"http://test.com/A.1.css.pagespeed.cf.0.css\"></head>"
      "<body>",
      StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                   redirect_url.c_str()),
      "<script src=\"http://test.com/1.js.pagespeed.ce.0.js\"></script>"
      "Hello, mod_pagespeed!</body></html>");

  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(kTestDomain, headers,
                                kFlushEarlyMoreResourcesInputHtml);

  // Enable FlushSubresourcesFilter filter.
  RewriteOptions* rewrite_options = server_context()->global_options();
  rewrite_options->ClearSignatureForTesting();
  rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);
  rewrite_options->set_enable_flush_subresources_experimental(true);

  rewrite_options->set_flush_more_resources_early_if_time_permits(true);
  rewrite_options->EnableExtendCacheFilters();
  // Disabling the inline filters so that the resources get flushed early
  // else our dummy resources are too small and always get inlined.
  rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
  rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
  rewrite_options->DisableFilter(RewriteOptions::kInlineImages);
  rewrite_options->ComputeSignature(hasher());

  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.jpg"), kContentTypeJpeg,
                                "image", kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.css"), kContentTypeCss,
                                kCssContent, kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.js"),
                                kContentTypeJavascript, "javascript",
                                kHtmlCacheTimeSec * 2);
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent, "prefetch_image_tag");

  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered but
  // all resources may not be flushed.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time all resources based on time will be flushed.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  EXPECT_EQ(kOutputHtml, text);
  fake_critical_images_finder_->set_css_critical_images(NULL);
}

TEST_F(FlushEarlyFlowTest, InsertLazyloadJsOnlyIfResourceHtmlNotEmpty) {
  const char kInputHtml[] =
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<title>Flush Subresources Early example</title>"
      "</head>"
      "<body>"
      "<img src=1.jpg />"
      "Hello, mod_pagespeed!"
      "</body>"
      "</html>";

  GoogleString redirect_url = StrCat(kTestDomain, "?ModPagespeed=noscript");
  GoogleString kOutputHtml = StrCat(
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<title>Flush Subresources Early example</title>"
      "</head>"
      "<body>",
      StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                   redirect_url.c_str()),
      "<script type=\"text/javascript\">",
      LazyloadImagesFilter::GetLazyloadJsSnippet(
          options_, server_context()->static_javascript_manager()),
      "</script>"
      "<img pagespeed_lazy_src=http://test.com/1.jpg.pagespeed.ce.0.jpg"
      " src=\"data:image/gif;"
      "base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==\""
      " onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\"/>"
      "Hello, mod_pagespeed!"
      "<script type=\"text/javascript\" pagespeed_no_defer=\"\">"
      "pagespeed.lazyLoadImages.overrideAttributeFunctions();</script>"
      "</body></html>");

  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(kTestDomain, headers, kInputHtml);

  // Enable FlushSubresourcesFilter filter.
  RewriteOptions* rewrite_options = server_context()->global_options();
  rewrite_options->ClearSignatureForTesting();
  rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);
  rewrite_options->set_enable_flush_subresources_experimental(true);
  rewrite_options->EnableExtendCacheFilters();
  // Disabling the inline filters so that the resources get flushed early
  // else our dummy resources are too small and always get inlined.
  rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
  rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
  rewrite_options->ComputeSignature(hasher());

  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.jpg"), kContentTypeJpeg,
                                "image", kHtmlCacheTimeSec * 2);

  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kLazyloadImages);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  GoogleString text;
  RequestHeaders request_headers;
  // Useragent is set to Firefox/ 9.0 because all flush early flow, defer
  // javascript and lazyload filter is enabled for this user agent.
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "Firefox/ 9.0");

  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered but no
  // lazyload js will be flushed early as no resource is present in the html.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(kOutputHtml, text);
}

TEST_F(FlushEarlyFlowTest, PreconnectTest) {
  latency_fetcher_->set_latency(200);
  const char kInputHtml[] =
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<title>Flush Subresources Early example</title>"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"1.css\">"
      "</head>"
      "<body>"
      "<img src=1.jpg />"
      "<img src=2.jpg />"
      "<img src=3.jpg />"
      "Hello, mod_pagespeed!"
      "</body>"
      "</html>";

  GoogleString redirect_url = StrCat(kTestDomain, "?ModPagespeed=noscript");
  const char pre_connect_tag[] =
      "<link rel=\"stylesheet\" href=\"http://cdn.com/pre_connect?id=%s\"/>";
  const char image_tag[] =
      "<img src=http://cdn.com/http/test.com/http/test.com/%s />";

  GoogleString pre_connect_url = "http://cdn.com/pre_connect";
  GoogleString kOutputHtml = StrCat(
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<script type=\"text/javascript\">"
      "(function(){new Image().src=\"http://cdn.com/http/test.com/http/"
      "test.com/A.1.css.pagespeed.cf.0.css\";})()</script>"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_prefetch_start = Number("
      "new Date());window.mod_pagespeed_num_resources_prefetched = 1</script>",
      StringPrintf(pre_connect_tag, "0"),
      StringPrintf(pre_connect_tag, "1"),
      "<title>Flush Subresources Early example</title>"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"http://cdn.com/http/"
          "test.com/http/test.com/A.1.css.pagespeed.cf.0.css\">"
      "</head>"
      "<body>", StrCat(
      StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                   redirect_url.c_str()),
      StringPrintf(image_tag, "1.jpg.pagespeed.ce.0.jpg"),
      StringPrintf(image_tag, "2.jpg.pagespeed.ce.0.jpg"),
      StringPrintf(image_tag, "3.jpg.pagespeed.ce.0.jpg"),
      "Hello, mod_pagespeed!"
      "</body>"
      "</html>"));

  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(kTestDomain, headers, kInputHtml);

  // Enable FlushSubresourcesFilter filter.
  RewriteOptions* rewrite_options = server_context()->global_options();
  rewrite_options->ClearSignatureForTesting();
  rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);
  rewrite_options->set_enable_flush_subresources_experimental(true);
  rewrite_options->EnableExtendCacheFilters();
  // Disabling the inline filters so that the resources get flushed early
  // else our dummy resources are too small and always get inlined.
  rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
  rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
  rewrite_options->set_pre_connect_url(pre_connect_url);
  rewrite_options->ComputeSignature(hasher());

  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.css"), kContentTypeCss,
                                kCssContent, kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.jpg"), kContentTypeJpeg,
                                "image", kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "2.jpg"), kContentTypeJpeg,
                                "image", kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "3.jpg"), kContentTypeJpeg,
                                "image", kHtmlCacheTimeSec * 2);
  TestUrlNamer url_namer;
  server_context()->set_url_namer(&url_namer);
  url_namer.SetProxyMode(true);

  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent, "prefetch_image_tag");

  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow and pre connect should be
  // triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_EQ(kOutputHtml, text);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestWithLocalStorageDoesNotCrash) {
  SetupForFlushEarlyFlow(true);
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_rel_subresource");

  RewriteOptions* rewrite_options = server_context()->global_options();
  rewrite_options->ClearSignatureForTesting();
  rewrite_options->EnableFilter(RewriteOptions::kLocalStorageCache);
  rewrite_options->ForceEnableFilter(RewriteOptions::kInlineImages);
  rewrite_options->ForceEnableFilter(RewriteOptions::kInlineCss);
  rewrite_options->ComputeSignature(hasher());

  // This sequence of requests used to cause a crash earlier. Here, we just test
  // that this server doesn't crash and don't check the output.
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
}

}  // namespace net_instaweb
