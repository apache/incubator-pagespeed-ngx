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

#include "net/instaweb/automatic/public/flush_early_flow.h"

#include "net/instaweb/automatic/public/proxy_interface_test_base.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/http/public/user_agent_matcher_test_base.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/critical_css_filter.h"
#include "net/instaweb/rewriter/public/flush_early_content_writer_filter.h"
#include "net/instaweb/rewriter/public/js_disable_filter.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/mock_critical_css_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/split_html_filter.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/test_url_namer.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/util/wildcard.h"

namespace net_instaweb {

namespace {

const char kMockHashValue[] = "MDAwMD";

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

const char kCookieScript[] =
    "<script type=\"text/javascript\" pagespeed_no_defer=\"\">"
    "(function(){"
    "var data = [\"CG=US:CA:Mountain+View\",\"UA=chrome\",\"path=/\"];"
    "for (var i = 0; i < data.length; i++) {"
    "document.cookie = data[i];"
    "}})()"
    "</script>";

const char kPreHeadHtml[] =
    "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
    "<html>"
    "<head>";

const char kRewrittenHtml[] =
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

const char kRewrittenHtmlWithDeferJs[] =
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
    " type=\"text/psajs\" orig_index=\"3\"></script>"
    "%s</head>"
    "<body>%s"
    "Hello, mod_pagespeed!"
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
    "<script pagespeed_orig_src=\"http://www.domain2.com/private.js\""
    " type=\"text/psajs\" orig_index=\"4\"></script>"
    "<link rel=\"stylesheet\" type=\"text/css\""
    " href=\"http://www.domain3.com/3.css\">"
    "%s"
    "</body>"
    "</html>%s";

const char kRewrittenHtmlForRedirect[] =
    "<script type=\"text/javascript\">"
    "window.location.replace(\"%s\")"
    "</script>"
    "</head><body></body></html>";

const char kFlushEarlyRewrittenHtmlImageTag[] =
    "<script type=\"text/javascript\">(function(){"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";})()</script>"
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "%s"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 5</script>";

const char kFlushEarlyRewrittenHtmlImageTagInsertDnsPrefetch[] =
    "<script type=\"text/javascript\">(function(){"
    "new Image().src=\"%s\";"
    "new Image().src=\"%s\";})()</script>"
    "<link rel=\"dns-prefetch\" href=\"//www.domain1.com\">"
    "<link rel=\"dns-prefetch\" href=\"//www.domain2.com\">"
    "<link rel=\"dns-prefetch\" href=\"//www.domain3.com\">"
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "%s"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 5</script>";

const char kFlushEarlyRewrittenHtmlLinkScript[] =
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "<script type=\"psa_prefetch\" src=\"%s\"></script>\n"
    "<script type=\"psa_prefetch\" src=\"%s\"></script>\n"
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "%s"
    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = 5</script>";
const char kFlushEarlyRewrittenHtmlWithLazyloadDeferJsScript[] =
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "<link rel=\"stylesheet\" href=\"%s\"/>\n"
    "%s"


    "<script type='text/javascript'>"
    "window.mod_pagespeed_prefetch_start = Number(new Date());"
    "window.mod_pagespeed_num_resources_prefetched = %d</script>";

}  // namespace

class FlushEarlyFlowTest : public ProxyInterfaceTestBase {
 protected:
  static const int kHtmlCacheTimeSec = 5000;

  FlushEarlyFlowTest()
      : start_time_ms_(0),
        request_url_(kTestDomain),
        noscript_redirect_url_(StrCat(kTestDomain, "?ModPagespeed=noscript")),
        max_age_300_("max-age=300"),
        request_start_time_ms_(-1),
        set_httponly_cookie_(false) {
    ConvertTimeToString(MockTimer::kApr_5_2010_ms, &start_time_string_);
    ConvertTimeToString(MockTimer::kApr_5_2010_ms + 5 * Timer::kMinuteMs,
                        &start_time_plus_300s_string_);
    ConvertTimeToString(MockTimer::kApr_5_2010_ms - 2 * Timer::kDayMs,
                        &old_time_string_);
  }
  virtual ~FlushEarlyFlowTest() {}

  virtual void SetUp() {
    SetMockHashValue("00000");  // Base64 encodes to kMockHashValue.
    RewriteOptions* options = server_context()->global_options();
    server_context_->set_enable_property_cache(true);
    const PropertyCache::Cohort* dom_cohort =
        SetupCohort(server_context_->page_property_cache(),
                    RewriteDriver::kDomCohort);
    server_context_->set_dom_cohort(dom_cohort);
    options->ClearSignatureForTesting();
    options->set_max_html_cache_time_ms(kHtmlCacheTimeSec * Timer::kSecondMs);
    options->set_in_place_rewriting_enabled(true);
    server_context()->ComputeSignature(options);
    ProxyInterfaceTestBase::SetUp();
    // The original url_async_fetcher() is still owned by RewriteDriverFactory.
    background_fetch_fetcher_.reset(new BackgroundFetchCheckingUrlAsyncFetcher(
        factory()->ComputeUrlAsyncFetcher()));
    server_context()->set_default_system_fetcher(
        background_fetch_fetcher_.get());

    start_time_ms_ = timer()->NowMs();
    rewritten_css_url_1_ = Encode(
        kTestDomain, "cf", kMockHashValue, "1.css", "css");
    rewritten_css_url_2_ = Encode(
        kTestDomain, "cf", kMockHashValue, "2.css", "css");
    rewritten_css_url_3_ = Encode(
        kTestDomain, "cf", kMockHashValue, "3.css", "css");
    rewritten_js_url_1_ = Encode(
        kTestDomain, "jm", kMockHashValue, "1.js", "js");
    rewritten_js_url_2_ = Encode(
        kTestDomain, "jm", kMockHashValue, "2.js", "js");
    rewritten_js_url_3_ = Encode(
        kTestDomain, "ce", kMockHashValue, "1.js", "js");
    rewritten_img_url_1_ = Encode(
        kTestDomain, "ce", kMockHashValue, "1.jpg", "jpg");
  }

  void SetupForFlushEarlyFlow() {
    // Setup
    ResponseHeaders headers;
    headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
    headers.Add(HttpAttributes::kSetCookie, "CG=US:CA:Mountain+View");
    headers.Add(HttpAttributes::kSetCookie, "UA=chrome");
    headers.Add(HttpAttributes::kSetCookie, "path=/");
    if (set_httponly_cookie_) {
      headers.Add(HttpAttributes::kSetCookie, "a=1; HttpOnly");
    }

    headers.SetStatusAndReason(HttpStatus::kOK);
    mock_url_fetcher_.SetResponse(request_url_, headers, kFlushEarlyHtml);

    // Enable FlushSubresourcesFilter filter.
    RewriteOptions* rewrite_options = server_context()->global_options();
    rewrite_options->ClearSignatureForTesting();
    rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);
    rewrite_options->EnableFilter(RewriteOptions::kCombineCss);
    rewrite_options->EnableFilter(RewriteOptions::kCombineJavascript);
    rewrite_options->EnableExtendCacheFilters();
    // Disabling the inline filters so that the resources get flushed early
    // else our dummy resources are too small and always get inlined.
    rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
    rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
    rewrite_options->ComputeSignature();

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

  GoogleString GetDeferJsCode() {
    return StrCat("<script type=\"text/javascript\" src=\"",
                  server_context()->static_asset_manager()->GetAssetUrl(
                      StaticAssetManager::kDeferJs, options_),
                  "\"></script>");
  }

  GoogleString GetSplitHtmlSuffixCode() {
    return StringPrintf(SplitHtmlFilter::kSplitSuffixJsFormatString,
                        0, "/psajs/blink.0.js", "{}", "false");
  }

  GoogleString NoScriptRedirectHtml() {
    return StringPrintf(kNoScriptRedirectFormatter,
                        noscript_redirect_url_.c_str(),
                        noscript_redirect_url_.c_str());
  }

  GoogleString RewrittenHtmlWithDeferJs(bool split_html_enabled,
                                        const GoogleString& image_tag,
                                        bool is_ie) {
    GoogleString defer_js_injected_html1 = StrCat(
        "<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
        JsDisableFilter::GetJsDisableScriptSnippet(options_),
        "</script>", split_html_enabled ? SplitHtmlFilter::kSplitInit : "");
    GoogleString defer_js_injected_html2, defer_js_injected_html3;
    if (split_html_enabled) {
      defer_js_injected_html3 = GetSplitHtmlSuffixCode();
    } else {
      defer_js_injected_html2 = GetDeferJsCode();
    }
    const char kCompatibleMetaTag[] =
        "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">";

    return StringPrintf(
        kRewrittenHtmlWithDeferJs, (is_ie ? kCompatibleMetaTag : ""),
        rewritten_css_url_1_.data(), rewritten_css_url_2_.data(),
        rewritten_js_url_1_.data(), rewritten_js_url_2_.data(),
        image_tag.data(), defer_js_injected_html1.c_str(),
        NoScriptRedirectHtml().c_str(), rewritten_css_url_3_.data(),
        defer_js_injected_html2.c_str(),
        defer_js_injected_html3.c_str());
  }

  GoogleString RewrittenHtml(const GoogleString& noscript_redirect) {
    return StringPrintf(
        kRewrittenHtml, rewritten_css_url_1_.data(),
        rewritten_css_url_2_.data(), rewritten_js_url_1_.data(),
        rewritten_js_url_2_.data(), rewritten_img_url_1_.data(),
        noscript_redirect.c_str(),
        rewritten_css_url_3_.data());
  }

  GoogleString RewrittenHtmlForRedirect() {
    return StringPrintf(kRewrittenHtmlForRedirect,
                        redirect_url_.c_str());
  }

  GoogleString FlushEarlyRewrittenHtml(
      UserAgentMatcher::PrefetchMechanism value,
      bool defer_js_enabled, bool insert_dns_prefetch,
      bool ua_only_for_flush_early_html) {
    return FlushEarlyRewrittenHtml(value, defer_js_enabled,
                                   insert_dns_prefetch, false, false, false,
                                   ua_only_for_flush_early_html, false);
  }

  GoogleString FlushEarlyRewrittenHtml(
      UserAgentMatcher::PrefetchMechanism value,
      bool defer_js_enabled, bool insert_dns_prefetch,
      bool lazyload_enabled, bool redirect_psa_off, bool split_html_enabled,
      bool ua_only_for_flush_early_html, bool is_ie) {
    GoogleString flush_early_html;
    GoogleString cookie_script = kCookieScript;
    GoogleString rewritten_html;

    GoogleString expected_deferjs_url = split_html_enabled ?
        "/psajs/blink.0.js" : "/psajs/js_defer.0.js";


    // Get rewritten html.
    if (defer_js_enabled & !ua_only_for_flush_early_html) {
      if (lazyload_enabled) {
        rewritten_html = RewrittenHtmlWithDeferJs(
            split_html_enabled, StrCat(
                "<img pagespeed_lazy_src=\"", rewritten_img_url_1_.data(),
                "\" src=\"/psajs/1.0.gif\"",
                " onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\"/>",
                "<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
                "pagespeed.lazyLoadImages.overrideAttributeFunctions();",
                "</script>"), is_ie);
      } else {
        rewritten_html = RewrittenHtmlWithDeferJs(
            split_html_enabled,
            StrCat("<img src=\"", rewritten_img_url_1_.data(), "\"/>"),
            is_ie);
      }

    } else {
      if (redirect_psa_off) {
        rewritten_html = RewrittenHtmlForRedirect();
        cookie_script = "";
      } else if (value == UserAgentMatcher::kPrefetchNotSupported) {
        rewritten_html = RewrittenHtml("");
      } else {
        rewritten_html = RewrittenHtml(NoScriptRedirectHtml());
      }
    }

    // Get FlushEarly html.
    switch (value) {
      case UserAgentMatcher::kPrefetchLinkScriptTag:
        if (defer_js_enabled) {
          flush_early_html = StringPrintf(
              kFlushEarlyRewrittenHtmlWithLazyloadDeferJsScript,
              rewritten_css_url_1_.data(), rewritten_css_url_2_.data(),
              rewritten_css_url_3_.data(),
              StrCat("<script type=\"psa_prefetch\" src=\"",
                     expected_deferjs_url, "\"></script>\n",
                     FlushEarlyContentWriterFilter::kDisableLinkTag).c_str(),
              4);
          if (lazyload_enabled) {
            StrAppend(&flush_early_html, StrCat(
                "<script type=\"text/javascript\">",
                LazyloadImagesFilter::GetLazyloadJsSnippet(
                    options_,
                    server_context()->static_asset_manager()),
                "</script>").c_str());
          }
        } else {
          flush_early_html = StringPrintf(
              kFlushEarlyRewrittenHtmlLinkScript,
              rewritten_css_url_1_.data(), rewritten_css_url_2_.data(),
              rewritten_js_url_1_.data(), rewritten_js_url_2_.data(),
              rewritten_css_url_3_.data(),
              FlushEarlyContentWriterFilter::kDisableLinkTag);
        }
        break;
      case UserAgentMatcher::kPrefetchImageTag:
        if (defer_js_enabled) {
          flush_early_html = StringPrintf(
              kFlushEarlyRewrittenHtmlWithLazyloadDeferJsScript,
              rewritten_css_url_1_.data(),
              rewritten_css_url_2_.data(), rewritten_css_url_3_.data(),
              FlushEarlyContentWriterFilter::kDisableLinkTag, 3);
        } else {
          GoogleString output_format;
          if (insert_dns_prefetch) {
            output_format = kFlushEarlyRewrittenHtmlImageTagInsertDnsPrefetch;
          } else {
            output_format = kFlushEarlyRewrittenHtmlImageTag;
          }
          flush_early_html = StringPrintf(
              output_format.c_str(),
              rewritten_js_url_1_.data(), rewritten_js_url_2_.data(),
              rewritten_css_url_1_.data(), rewritten_css_url_2_.data(),
              rewritten_css_url_3_.data(),
              FlushEarlyContentWriterFilter::kDisableLinkTag);
        }
        break;
      case UserAgentMatcher::kPrefetchNotSupported:
      default:
        flush_early_html = "";
        cookie_script = "";
        break;
    }

    // Return combined html.
    return StrCat(kPreHeadHtml, flush_early_html, cookie_script,
                  rewritten_html);
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
    SetupForFlushEarlyFlow();
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
    GoogleString expected_output = FlushEarlyRewrittenHtml(
        mechanism, false, false, true);
    if (!inject_error) {
      EXPECT_STREQ(expected_output, text);
      VerifyCharset(&headers);
    }
  }
  void TestFlushEarlyFlow(const StringPiece& user_agent,
                          UserAgentMatcher::PrefetchMechanism mechanism,
                          bool ua_only_for_flush_early_html) {
    SetupForFlushEarlyFlow();
    GoogleString text;
    RequestHeaders request_headers;
    ResponseHeaders headers;
    request_headers.Replace(HttpAttributes::kUserAgent, user_agent.as_string());
    FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
    // Check total number of cache inserts.
    // 7 for 1.css, 2.css, 3.css, 1.js, 2.js, 1.jpg and private.js.
    // 19 metadata cache enties - three for cf and jm, seven for ce and
    //       six for fs.
    // 1 for DomCohort write in property cache.
    EXPECT_EQ(27, lru_cache()->num_inserts());

    // Fetch the url again. This time FlushEarlyFlow should be triggered with
    // the  appropriate prefetch mechanism.
    FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
    EXPECT_EQ(FlushEarlyRewrittenHtml(mechanism, false, false,
                                      ua_only_for_flush_early_html), text);
    VerifyCharset(&headers);
    if (mechanism != UserAgentMatcher::kPrefetchNotSupported) {
      EXPECT_STREQ("cf,ei,fs,jm", AppliedRewriterStringFromLog());
      EXPECT_STREQ("cf,ei,fs,jm", headers.Lookup1(kPsaRewriterHeader));
    }
  }

  void TestFlushLazyLoadJsEarly(bool is_mobile) {
    const char kInputHtml[] =
        "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
        "<html>"
        "<head>"
        "<title>Flush Subresources Early example</title>"
        "</head>"
        "<body>"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"1.css\">"
        "<img src=1.jpg />"
        "Hello, mod_pagespeed!"
        "</body>"
        "</html>";

    GoogleString redirect_url = StrCat(kTestDomain, "?ModPagespeed=noscript");
    GoogleString kNotMobileOutputHtml = StrCat(
        "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
        "<html>"
        "<head>",
        StringPrintf(
            "<link rel=\"stylesheet\" href=\"%s\"/>\n"
            "%s"
            "<script type='text/javascript'>"
            "window.mod_pagespeed_prefetch_start = Number(new Date());"
            "window.mod_pagespeed_num_resources_prefetched = 1"
            "</script>", rewritten_css_url_1_.c_str(),
            FlushEarlyContentWriterFilter::kDisableLinkTag),
        "<script type=\"text/javascript\">",
        LazyloadImagesFilter::GetLazyloadJsSnippet(
            options_, server_context()->static_asset_manager()),
        "</script>"
        "<title>Flush Subresources Early example</title>"
        "</head>"
        "<body>",
        StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                     redirect_url.c_str()),
        StringPrintf(
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
        "<img pagespeed_lazy_src=http://test.com/1.jpg.pagespeed.ce.%s.jpg"
        " src=\"/psajs/1.0.gif\""
        " onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\"/>"
        "Hello, mod_pagespeed!"
        "<script type=\"text/javascript\" pagespeed_no_defer=\"\">"
        "pagespeed.lazyLoadImages.overrideAttributeFunctions();</script>"
        "</body></html>", rewritten_css_url_1_.c_str(), kMockHashValue));

    GoogleString kMobileOutputHtml = StrCat(
        "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
        "<html>"
        "<head>",
        StringPrintf(
            "<link rel=\"stylesheet\" href=\"%s\"/>\n"
            "%s"
            "<script type='text/javascript'>"
            "window.mod_pagespeed_prefetch_start = Number(new Date());"
            "window.mod_pagespeed_num_resources_prefetched = 1"
            "</script>", rewritten_css_url_1_.c_str(),
            FlushEarlyContentWriterFilter::kDisableLinkTag),
        "<title>Flush Subresources Early example</title>"
        "</head>"
        "<body>",
        StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                     redirect_url.c_str()),
        StringPrintf(
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">"
        "<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
        rewritten_css_url_1_.c_str()),
        LazyloadImagesFilter::GetLazyloadJsSnippet(
            options_, server_context()->static_asset_manager()),
        StringPrintf(
        "</script>"
        "<img pagespeed_lazy_src=http://test.com/1.jpg.pagespeed.ce.%s.jpg"
        " src=\"/psajs/1.0.gif\""
        " onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\"/>"
        "Hello, mod_pagespeed!"
        "<script type=\"text/javascript\" pagespeed_no_defer=\"\">"
        "pagespeed.lazyLoadImages.overrideAttributeFunctions();</script>"
        "</body></html>", kMockHashValue));

    ResponseHeaders headers;
    headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
    headers.SetStatusAndReason(HttpStatus::kOK);
    mock_url_fetcher_.SetResponse(kTestDomain, headers, kInputHtml);

    // Enable FlushSubresourcesFilter filter.
    RewriteOptions* rewrite_options = server_context()->global_options();
    rewrite_options->ClearSignatureForTesting();
    rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);
    rewrite_options->EnableExtendCacheFilters();
    // Disabling the inline filters so that the resources get flushed early
    // else our dummy resources are too small and always get inlined.
    rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
    rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
    rewrite_options->ComputeSignature();

    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.jpg"),
                                  kContentTypeJpeg, "image",
                                  kHtmlCacheTimeSec * 2);
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
    if (is_mobile) {
      request_headers.Replace(
          HttpAttributes::kUserAgent,
          UserAgentMatcherTestBase::kAndroidChrome21UserAgent);
    } else {
      request_headers.Replace(HttpAttributes::kUserAgent, "Chrome/ 9.0");
    }

    FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

    // Fetch the url again. This time FlushEarlyFlow should be triggered but no
    // lazyload js will be flushed early as no resource is present in the html.
    FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
    if (is_mobile) {
      EXPECT_STREQ(kMobileOutputHtml, text);
    } else {
      EXPECT_STREQ(kNotMobileOutputHtml, text);
    }
  }

  void TestFlushPreconnects(bool is_mobile) {
    SetHeaderLatencyMs(200);
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
    GoogleString kOutputHtml = StringPrintf(
        "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
        "<html>"
        "<head>"
        "<link rel=\"stylesheet\" href=\"http://cdn.com/http/"
        "test.com/http/test.com/A.1.css.pagespeed.cf.%s.css\"/>\n"
        "%s"
        "<script type='text/javascript'>"
        "window.mod_pagespeed_prefetch_start = Number("
        "new Date());"
        "window.mod_pagespeed_num_resources_prefetched = 1</script>",
        kMockHashValue, FlushEarlyContentWriterFilter::kDisableLinkTag);
    if (!is_mobile) {
      StrAppend(&kOutputHtml, StringPrintf(pre_connect_tag, "0"),
                StringPrintf(pre_connect_tag, "1"));
    }
    StrAppend(&kOutputHtml,
        StringPrintf(
        "<title>Flush Subresources Early example</title>"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"http://cdn.com/http/"
            "test.com/http/test.com/A.1.css.pagespeed.cf.%s.css\">"
        "</head>"
        "<body>", kMockHashValue), StrCat(
        StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                     redirect_url.c_str()),
        StringPrintf(image_tag, StringPrintf("1.jpg.pagespeed.ce.%s.jpg",
                                             kMockHashValue).c_str()),
        StringPrintf(image_tag, StringPrintf("2.jpg.pagespeed.ce.%s.jpg",
                                             kMockHashValue).c_str()),
        StringPrintf(image_tag, StringPrintf("3.jpg.pagespeed.ce.%s.jpg",
                                             kMockHashValue).c_str()),
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
    rewrite_options->EnableExtendCacheFilters();
    // Disabling the inline filters so that the resources get flushed early
    // else our dummy resources are too small and always get inlined.
    rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
    rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
    rewrite_options->set_pre_connect_url(pre_connect_url);
    rewrite_options->ComputeSignature();

    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.css"), kContentTypeCss,
                                  kCssContent, kHtmlCacheTimeSec * 2);
    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.jpg"),
                                  kContentTypeJpeg, "image",
                                  kHtmlCacheTimeSec * 2);
    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "2.jpg"),
                                  kContentTypeJpeg, "image",
                                  kHtmlCacheTimeSec * 2);
    SetResponseWithDefaultHeaders(StrCat(kTestDomain, "3.jpg"),
                                  kContentTypeJpeg, "image",
                                  kHtmlCacheTimeSec * 2);
    TestUrlNamer url_namer;
    server_context()->set_url_namer(&url_namer);
    url_namer.SetProxyMode(true);

    GoogleString text;
    RequestHeaders request_headers;
    if (is_mobile) {
      request_headers.Replace(
          HttpAttributes::kUserAgent,
          UserAgentMatcherTestBase::kAndroidChrome21UserAgent);
    } else {
      request_headers.Replace(HttpAttributes::kUserAgent, "prefetch_image_tag");
    }

    FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

    // Fetch the url again. This time FlushEarlyFlow and pre connect should be
    // triggered.
    FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
    EXPECT_STREQ(kOutputHtml, text);
  }

  void SetHeaderLatencyMs(int64 latency_ms) {
    RequestContext::TimingInfo* timing_info = mutable_timing_info();
    timing_info->FetchStarted();
    AdvanceTimeMs(latency_ms);
    timing_info->FetchHeaderReceived();
  }

  scoped_ptr<BackgroundFetchCheckingUrlAsyncFetcher> background_fetch_fetcher_;
  int64 start_time_ms_;
  GoogleString start_time_string_;
  GoogleString start_time_plus_300s_string_;
  GoogleString old_time_string_;
  GoogleString rewritten_css_url_1_;
  GoogleString rewritten_css_url_2_;
  GoogleString rewritten_css_url_3_;
  GoogleString rewritten_js_url_1_;
  GoogleString rewritten_js_url_2_;
  GoogleString rewritten_js_url_3_;
  GoogleString rewritten_img_url_1_;

  GoogleString request_url_;
  GoogleString redirect_url_;
  GoogleString noscript_redirect_url_;

  const GoogleString max_age_300_;
  int64 request_start_time_ms_;
  bool set_httponly_cookie_;
};

// TODO(mpalem): Add tests for max_prefetch_js_elements and defer Js.

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTest) {
  TestFlushEarlyFlow(NULL, UserAgentMatcher::kPrefetchNotSupported, true);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestPrefetch) {
  TestFlushEarlyFlow("prefetch_link_script_tag",
                     UserAgentMatcher::kPrefetchLinkScriptTag, true);
  rewrite_driver_->log_record()->WriteLog();
  EXPECT_EQ(5, logging_info()->rewriter_stats_size());
  EXPECT_STREQ("fs", logging_info()->rewriter_stats(2).id());
  const RewriterStats& stats = logging_info()->rewriter_stats(2);
  EXPECT_EQ(RewriterHtmlApplication::ACTIVE, stats.html_status());
  EXPECT_EQ(2, stats.status_counts_size());
  const RewriteStatusCount& applied = stats.status_counts(0);
  EXPECT_EQ(RewriterApplication::APPLIED_OK, applied.application_status());
  EXPECT_EQ(6, applied.count());
  const RewriteStatusCount& not_applied = stats.status_counts(1);
  EXPECT_EQ(RewriterApplication::NOT_APPLIED, not_applied.application_status());
  EXPECT_EQ(2, not_applied.count());
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestPcacheMiss) {
  SetupForFlushEarlyFlow();
  GoogleString text;
  RequestHeaders request_headers;
  ResponseHeaders headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_script_tag");
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  rewrite_driver_->log_record()->WriteLog();
  EXPECT_EQ(5, logging_info()->rewriter_stats_size());
  EXPECT_STREQ("fs", logging_info()->rewriter_stats(2).id());
  const RewriterStats& stats = logging_info()->rewriter_stats(2);
  EXPECT_EQ(RewriterHtmlApplication::PROPERTY_CACHE_MISS, stats.html_status());
  EXPECT_EQ(0, stats.status_counts_size());
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestFallbackPageUsage) {
  SetupForFlushEarlyFlow();

  // Enable UseFallbackPropertyCacheValues.
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->set_use_fallback_property_cache_values(true);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  // Setting up mock responses for the url and fallback url.
  GoogleString url = StrCat(kTestDomain, "a.html?query=some");
  GoogleString fallback_url =
      StrCat(kTestDomain, "a.html?different_query=some");
  ResponseHeaders response_headers;
  response_headers.Add(
      HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  response_headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(url, response_headers, kFlushEarlyHtml);
  mock_url_fetcher_.SetResponse(
      fallback_url, response_headers, kFlushEarlyHtml);

  GoogleString text;
  RequestHeaders request_headers;
  ResponseHeaders headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_script_tag");

  FetchFromProxy(url, request_headers, true, &text, &headers);

  // Request another url with different query params so that fallback values
  // will be used.
  FetchFromProxy(fallback_url, request_headers, true, &text, &headers);

  rewrite_driver_->log_record()->WriteLog();
  EXPECT_EQ(5, logging_info()->rewriter_stats_size());
  EXPECT_STREQ("fs", logging_info()->rewriter_stats(2).id());
  const RewriterStats& fallback_stats = logging_info()->rewriter_stats(2);
  EXPECT_EQ(RewriterHtmlApplication::ACTIVE, fallback_stats.html_status());
  EXPECT_EQ(2, fallback_stats.status_counts_size());
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestDisabled) {
  // Adding a httponly cookie in the response causes flush early to be disabled
  // for the second request.
  set_httponly_cookie_ = true;
  SetupForFlushEarlyFlow();
  GoogleString text;
  RequestHeaders request_headers;
  ResponseHeaders headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_script_tag");
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_FALSE(headers.Has(kPsaRewriterHeader));
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_FALSE(headers.Has(kPsaRewriterHeader));

  rewrite_driver_->log_record()->WriteLog();
  EXPECT_EQ(5, logging_info()->rewriter_stats_size());
  EXPECT_STREQ("fs", logging_info()->rewriter_stats(2).id());
  const RewriterStats& stats = logging_info()->rewriter_stats(2);
  EXPECT_EQ(RewriterHtmlApplication::DISABLED, stats.html_status());
  EXPECT_EQ(0, stats.status_counts_size());

  // Change the fetcher's response to not set the http only cookie. We still
  // don't flush early.
  set_httponly_cookie_ = false;
  SetupForFlushEarlyFlow();
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_FALSE(headers.Has(kPsaRewriterHeader));
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_FALSE(headers.Has(kPsaRewriterHeader));

  // Clear all the caches. We don't flush early on the first request since we
  // miss the pcache, but flush early on the second request.
  lru_cache()->Clear();
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_FALSE(headers.Has(kPsaRewriterHeader));
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_TRUE(headers.Has(kPsaRewriterHeader));
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestUnsupportedUserAgent) {
  SetupForFlushEarlyFlow();
  GoogleString text;
  RequestHeaders request_headers;
  ResponseHeaders headers;
  request_headers.Replace(HttpAttributes::kUserAgent, "");
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  rewrite_driver_->log_record()->WriteLog();
  EXPECT_EQ(5, logging_info()->rewriter_stats_size());
  EXPECT_STREQ("fs", logging_info()->rewriter_stats(2).id());
  const RewriterStats& stats = logging_info()->rewriter_stats(2);
  EXPECT_EQ(RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED,
            stats.html_status());
  EXPECT_EQ(0, stats.status_counts_size());
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowStatusCodeUnstable) {
  // Test that the flush early flow is not triggered when the status code is
  // unstable.
  request_url_ = "http://test.com/?q=1";
  SetupForFlushEarlyFlow();
  redirect_url_ = StrCat(request_url_, "&ModPagespeed=noscript");
  noscript_redirect_url_ = StrCat(request_url_, "&amp;ModPagespeed=noscript");
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_script_tag");
  ResponseHeaders headers;
  FetchFromProxy(request_url_, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(request_url_, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkScriptTag, false, false, true),
      text);
  EXPECT_EQ(0, statistics()->FindVariable(
      FlushEarlyFlow::kNumFlushEarlyRequestsRedirected)->Get());

  SetFetchResponse404(request_url_);
  // Fetch again so that 404 is populated in response headers.
  // It should redirect to ModPagespeed=noscript in this case.
  FetchFromProxy(request_url_, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkScriptTag, false, false, false,
      true, false, true, false), text);
  EXPECT_EQ(1, statistics()->FindVariable(
      FlushEarlyFlow::kNumFlushEarlyRequestsRedirected)->Get());

  // Fetch the url again. This time FlushEarlyFlow should not be triggered as
  // the status code is not stable.
  FetchFromProxy(request_url_, request_headers, true, &text, &headers);
  EXPECT_EQ(HttpStatus::kNotFound, headers.status_code());

  // Delete the 404 from cache and again set up for 200 response.
  lru_cache()->Delete(request_url_);
  SetupForFlushEarlyFlow();

  // Flush early flow is again not triggered as the status code is not
  // stable for property_cache_http_status_stability_threshold number of
  // requests.
  for (int i = 0, n = server_context()->global_options()->
       property_cache_http_status_stability_threshold(); i < n; ++i) {
    FetchFromProxy(request_url_, request_headers, true, &text, &headers);
    EXPECT_TRUE(text.find("mod_pagespeed_num_resources_prefetched") ==
                GoogleString::npos);
  }
  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(request_url_, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkScriptTag, false, false, true),
      text);

  // Fetch again so that 404 is populated in response headers.
  // It should redirect to ModPagespeed=noscript in this case.
  SetFetchResponse404(request_url_);
  FetchFromProxy(request_url_, request_headers, true, &text, &headers);
  EXPECT_EQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkScriptTag, false, false, false,
      true, false, true, false), text);
  EXPECT_EQ(2, statistics()->FindVariable(
      FlushEarlyFlow::kNumFlushEarlyRequestsRedirected)->Get());
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestMobile) {
  TestFlushEarlyFlow(UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
                     UserAgentMatcher::kPrefetchImageTag, false);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestImageTag) {
  TestFlushEarlyFlow("prefetch_image_tag",
                     UserAgentMatcher::kPrefetchImageTag, true);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestLinkScript) {
  TestFlushEarlyFlow("prefetch_link_script_tag",
                     UserAgentMatcher::kPrefetchLinkScriptTag, true);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestWithDeferJsImageTag) {
  SetupForFlushEarlyFlow();
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
  EXPECT_STREQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchImageTag, true, false, true), text);
  VerifyCharset(&headers);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestWithDeferJsPrefetch) {
  SetupForFlushEarlyFlow();
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  custom_options->set_max_prefetch_js_elements(0);
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent, "Firefox/ 9.0");

  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_STREQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkScriptTag, true, false, false), text);
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


TEST_F(FlushEarlyFlowTest,
       ExperimentalFlushEarlyFlowTestWithInsertDnsPrefetch) {
  SetupForFlushEarlyFlow();
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
  EXPECT_STREQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchImageTag, false, true, true), text);
}

TEST_F(FlushEarlyFlowTest, LazyloadAndDeferJsScriptFlushedEarly) {
  SetHeaderLatencyMs(600);
  SetupForFlushEarlyFlow();
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  custom_options->EnableFilter(RewriteOptions::kLazyloadImages);
  custom_options->set_max_prefetch_js_elements(0);
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
  EXPECT_STREQ(FlushEarlyRewrittenHtml(
      UserAgentMatcher::kPrefetchLinkScriptTag, true, false, true,
      false, false, false, false), text);
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
  GoogleString kOutputHtml = StringPrintf(
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<link rel=\"stylesheet\" href=\"%s\"/>\n"
      "%s"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_prefetch_start = Number(new Date());"
      "window.mod_pagespeed_num_resources_prefetched = 1"
      "</script>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
      "<meta charset=\"UTF-8\"/>"
      "<title>Flush Subresources Early example</title>"
      "<link rel=\"stylesheet\" type=\"text/css\""
      " href=\"%s\"></head>"
      "<body>%sHello, mod_pagespeed!</body></html>",
      rewritten_css_url_1_.c_str(),
      FlushEarlyContentWriterFilter::kDisableLinkTag,
      rewritten_css_url_1_.c_str(),
      StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                   redirect_url.c_str()).c_str());

  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(kTestDomain, headers, kInputHtml);

  // Enable FlushSubresourcesFilter filter.
  RewriteOptions* rewrite_options = server_context()->global_options();
  rewrite_options->ClearSignatureForTesting();
  rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);
  rewrite_options->EnableExtendCacheFilters();
  // Disabling the inline filters so that the resources get flushed early
  // else our dummy resources are too small and always get inlined.
  rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
  rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
  rewrite_options->ComputeSignature();

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
  EXPECT_STREQ(kOutputHtml, text);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyMoreResourcesIfTimePermits) {
  SetHeaderLatencyMs(600);
  StringSet* css_critical_images = new StringSet;
  css_critical_images->insert(StrCat(kTestDomain, "1.jpg"));
  SetCssCriticalImagesInFinder(css_critical_images);
  GoogleString redirect_url = StrCat(kTestDomain, "?ModPagespeed=noscript");

  GoogleString kOutputHtml = StringPrintf(
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<script type=\"text/javascript\">(function(){"
      "new Image().src=\"http://test.com/1.jpg.pagespeed.ce.%s.jpg\";"
      "new Image().src=\"%s\";})()</script>"
      "<link rel=\"stylesheet\" href=\"%s\"/>\n"
      "%s"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_prefetch_start = Number(new Date());"
      "window.mod_pagespeed_num_resources_prefetched = 3"
      "</script>"
      "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"/>"
      "<meta http-equiv=\"last-modified\" content=\"2012-08-09T11:03:27Z\"/>"
      "<meta charset=\"UTF-8\"/>"
      "<title>Flush Subresources Early example</title>"
      "<link rel=\"stylesheet\" type=\"text/css\""
      " href=\"%s\"></head>"
      "<body>%s"
      "<script src=\"%s\"></script>"
      "Hello, mod_pagespeed!</body></html>",
      kMockHashValue, rewritten_js_url_3_.c_str(),
      rewritten_css_url_1_.c_str(),
      FlushEarlyContentWriterFilter::kDisableLinkTag,
      rewritten_css_url_1_.c_str(),
      StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                   redirect_url.c_str()).c_str(), rewritten_js_url_3_.c_str());

  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(kTestDomain, headers,
                                kFlushEarlyMoreResourcesInputHtml);

  // Enable FlushSubresourcesFilter filter.
  RewriteOptions* rewrite_options = server_context()->global_options();
  rewrite_options->ClearSignatureForTesting();
  rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);

  rewrite_options->set_flush_more_resources_early_if_time_permits(true);
  rewrite_options->EnableExtendCacheFilters();
  // Disabling the inline filters so that the resources get flushed early
  // else our dummy resources are too small and always get inlined.
  rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
  rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
  rewrite_options->DisableFilter(RewriteOptions::kInlineImages);
  rewrite_options->ComputeSignature();

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

  EXPECT_STREQ(kOutputHtml, text);
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
      "<script type=\"text/javascript\" pagespeed_no_defer=\"\">",
      LazyloadImagesFilter::GetLazyloadJsSnippet(
          options_, server_context()->static_asset_manager()),
      StringPrintf(
      "</script>"
      "<img pagespeed_lazy_src=http://test.com/1.jpg.pagespeed.ce.%s.jpg"
      " src=\"/psajs/1.0.gif\""
      " onload=\"pagespeed.lazyLoadImages.loadIfVisible(this);\"/>"
      "Hello, mod_pagespeed!"
      "<script type=\"text/javascript\" pagespeed_no_defer=\"\">"
      "pagespeed.lazyLoadImages.overrideAttributeFunctions();</script>"
      "</body></html>", kMockHashValue));

  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(kTestDomain, headers, kInputHtml);

  // Enable FlushSubresourcesFilter filter.
  RewriteOptions* rewrite_options = server_context()->global_options();
  rewrite_options->ClearSignatureForTesting();
  rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);
  rewrite_options->EnableExtendCacheFilters();
  // Disabling the inline filters so that the resources get flushed early
  // else our dummy resources are too small and always get inlined.
  rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
  rewrite_options->DisableFilter(RewriteOptions::kInlineJavascript);
  rewrite_options->ComputeSignature();

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
  EXPECT_STREQ(kOutputHtml, text);
}

TEST_F(FlushEarlyFlowTest, DontInsertLazyloadJsIfMobile) {
  TestFlushLazyLoadJsEarly(true);
}

TEST_F(FlushEarlyFlowTest, InsertLazyloadJsIfNotMobile) {
  TestFlushLazyLoadJsEarly(false);
}

TEST_F(FlushEarlyFlowTest, PreconnectTest) {
  TestFlushPreconnects(false);
}

TEST_F(FlushEarlyFlowTest, NoPreconnectForMobile) {
  TestFlushPreconnects(true);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowTestWithLocalStorageDoesNotCrash) {
  SetupForFlushEarlyFlow();
  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          "prefetch_link_rel_subresource");

  RewriteOptions* rewrite_options = server_context()->global_options();
  rewrite_options->ClearSignatureForTesting();
  rewrite_options->EnableFilter(RewriteOptions::kLocalStorageCache);
  rewrite_options->ForceEnableFilter(RewriteOptions::kInlineImages);
  rewrite_options->ForceEnableFilter(RewriteOptions::kInlineCss);
  rewrite_options->ComputeSignature();

  // This sequence of requests used to cause a crash earlier. Here, we just test
  // that this server doesn't crash and don't check the output.
  ResponseHeaders headers;
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowWithIEAddUACompatibilityHeader) {
  SetupForFlushEarlyFlow();
  RequestHeaders request_headers;
  // Useragent is set to "MSIE 9." because we need to check if appropriate
  // HttpAttributes::kXUACompatible header is added, which happens only with
  // MSIE 9 and above.
  request_headers.Replace(HttpAttributes::kUserAgent, " MSIE 9.");
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  custom_options->set_max_prefetch_js_elements(0);

  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  GoogleString text;
  ResponseHeaders headers;

  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  EXPECT_STREQ(FlushEarlyRewrittenHtml(UserAgentMatcher::kPrefetchLinkScriptTag,
                                    true, false, false, false,
                                    false, false, true), text);
  ConstStringStarVector values;
  EXPECT_TRUE(headers.Lookup(HttpAttributes::kXUACompatible, &values));
  EXPECT_STREQ("IE=edge", *(values[0]));
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowWithDeferJsAndSplitEnabled) {
  SetupForFlushEarlyFlow();
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          " MSIE 9.");
  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  custom_options->EnableFilter(RewriteOptions::kDeferJavascript);
  custom_options->EnableFilter(RewriteOptions::kSplitHtml);
  custom_options->set_max_prefetch_js_elements(0);

  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  GoogleString text;
  ResponseHeaders headers;

  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  EXPECT_STREQ(FlushEarlyRewrittenHtml(UserAgentMatcher::kPrefetchLinkScriptTag,
                                    true, false, false, false,
                                    true, false, true), text);
}

TEST_F(FlushEarlyFlowTest, FlushEarlyFlowWithCriticalCSSEnabled) {
  GoogleString redirect_url = StrCat(kTestDomain, "?ModPagespeed=noscript");
  GoogleString invoke_flush_style_template =
      StringPrintf(CriticalCssFilter::kInvokeFlushEarlyCssTemplate, "*", "");

  const char kInputHtml[] =
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<title>Flush Subresources Early example</title>"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"1.css\">"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"2.css?a=1&b=2\">"
      "</head>"
      "<body>"
      "Hello, mod_pagespeed!"
      "</body>"
      "</html>";
  GoogleString output_html = StringPrintf(
      "<!doctype html PUBLIC \"HTML 4.0.1 Strict>"
      "<html>"
      "<head>"
      "<script type=\"text/psa_flush_style\" id=\"*\">b{color:#000}</script>"
      "<script type=\"text/psa_flush_style\" id=\"*\">a{float:left}</script>"
      "<script type='text/javascript'>"
      "window.mod_pagespeed_prefetch_start = Number(new Date());"
      "window.mod_pagespeed_num_resources_prefetched = 2"
      "</script>"
      "<title>Flush Subresources Early example</title>"
      "<script id=\"psa_flush_style_early\""
      " pagespeed_no_defer=\"\" type=\"text/javascript\">"
      "%s</script>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">%s</script>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">%s</script>"
      "</head>"
      "<body>%sHello, mod_pagespeed!</body></html>"
      "<noscript id=\"psa_add_styles\">"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"*1.css*\">"
      "<link rel=\"stylesheet\" type=\"text/css\" href=\"*2.css*\"></noscript>"
      "<script pagespeed_no_defer=\"\" type=\"text/javascript\">"
      "%s*"
      "</script>",
      CriticalCssFilter::kApplyFlushEarlyCssTemplate,
      invoke_flush_style_template.c_str(),
      invoke_flush_style_template.c_str(),
      StringPrintf(kNoScriptRedirectFormatter, redirect_url.c_str(),
                   redirect_url.c_str()).c_str(),
      CriticalCssFilter::kAddStylesScript);

  // Setup response to resources.
  ResponseHeaders headers;
  headers.Add(HttpAttributes::kContentType, kContentTypeHtml.mime_type());
  headers.SetStatusAndReason(HttpStatus::kOK);
  mock_url_fetcher_.SetResponse(kTestDomain, headers, kInputHtml);
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "1.css"), kContentTypeCss,
                                kCssContent, kHtmlCacheTimeSec * 2);
  SetResponseWithDefaultHeaders(StrCat(kTestDomain, "2.css?a=1&b=2"),
                                kContentTypeCss, kCssContent,
                                kHtmlCacheTimeSec * 2);

  // Enable FlushSubresourcesFilter filter.
  RewriteOptions* rewrite_options = server_context()->global_options();
  rewrite_options->ClearSignatureForTesting();
  rewrite_options->EnableFilter(RewriteOptions::kFlushSubresources);
  // Disabling the inline filters so that the resources get flushed early
  // else our dummy resources are too small and always get inlined.
  rewrite_options->DisableFilter(RewriteOptions::kInlineCss);
  rewrite_options->DisableFilter(RewriteOptions::kRewriteJavascript);
  // Enable Critical CSS filter.
  rewrite_options->set_enable_flush_early_critical_css(true);
  rewrite_options->EnableFilter(RewriteOptions::kPrioritizeCriticalCss);
  rewrite_options->ComputeSignature();

  scoped_ptr<RewriteOptions> custom_options(
      server_context()->global_options()->Clone());
  ProxyUrlNamer url_namer;
  url_namer.set_options(custom_options.get());
  server_context()->set_url_namer(&url_namer);

  // Add critical css rules.
  MockCriticalCssFinder* critical_css_finder =
      new MockCriticalCssFinder(rewrite_driver(), statistics());
  server_context()->set_critical_css_finder(critical_css_finder);
  critical_css_finder->AddCriticalCss("http://test.com/1.css",
                                      "b {color: black }", 100);
  critical_css_finder->AddCriticalCss("http://test.com/2.css?a=1&b=2",
                                      "a {float: left }", 100);

  GoogleString text;
  RequestHeaders request_headers;
  request_headers.Replace(HttpAttributes::kUserAgent,
                          UserAgentMatcherTestBase::kChrome18UserAgent);

  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);

  // Fetch the url again. This time FlushEarlyFlow should be triggered.
  FetchFromProxy(kTestDomain, request_headers, true, &text, &headers);
  EXPECT_TRUE(Wildcard(output_html).Match(text)) <<
      "Expected:\n" << output_html << "\nGot:\n" << text;
}

}  // namespace net_instaweb
