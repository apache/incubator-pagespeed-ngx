/*
 * Copyright 2012 Google Inc.
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

#include "net/instaweb/automatic/public/flush_early_flow.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/meta_data.h"  // for Code::kOK
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/cache_html_info_finder.h"
#include "net/instaweb/rewriter/public/critical_css_finder.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/flush_early_content_writer_filter.h"
#include "net/instaweb/rewriter/public/flush_early_info_finder.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/rewritten_content_scanning_filter.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/enums.pb.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/escaping.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"  // for Timer
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/http/http.pb.h"

namespace {

const char kJavascriptInline[] = "<script type=\"text/javascript\">%s</script>";

const int kMaxParallelConnections = 6;

// Minimum fetch latency for sending pre-connect requests.
const int kMinLatencyForPreconnectMs = 100;

}  // namespace

namespace net_instaweb {

class StaticAssetManager;

namespace {

void InitFlushEarlyDriverWithPropertyCacheValues(
    RewriteDriver* flush_early_driver, FallbackPropertyPage* page) {
  // Reading Flush early flow info from Property Page. After reading,
  // property page in new_driver is set to NULL, so that no one writes to
  // property cache while flushing early. Also property page isn't guaranteed to
  // exist in flush_early_driver lifetime.
  // TODO(pulkitg): Change the functions GetHtmlCriticalImages and
  // UpdateFlushEarlyInfoInDriver to take AbstractPropertyPage as a parameter so
  // that set_unowned_fallback_property_page function call can be removed.
  flush_early_driver->set_unowned_fallback_property_page(page);
  // Populates all fields which are needed from property_page as property_page
  // will be set to NULL afterwards.
  flush_early_driver->flush_early_info();
  ServerContext* server_context = flush_early_driver->server_context();
  FlushEarlyInfoFinder* finder = server_context->flush_early_info_finder();
  if (finder != NULL && finder->IsMeaningful(flush_early_driver)) {
    finder->UpdateFlushEarlyInfoInDriver(flush_early_driver);
  }

  // Because we are resetting the property page at the end of this function, we
  // need to make sure the CriticalImageFinder state is updated here. We don't
  // have a public interface for updating the state in the driver, so perform a
  // throwaway critical image query here, which will in turn cause the state
  // that CriticalImageFinder keeps in RewriteDriver to be updated.
  // TODO(jud): Remove this when the CriticalImageFinder is held in the
  // RewriteDriver, instead of ServerContext.
  server_context->critical_images_finder()->
      GetHtmlCriticalImages(flush_early_driver);

  CriticalSelectorFinder* selector_finder =
      server_context->critical_selector_finder();
  if (selector_finder != NULL) {
    selector_finder->GetCriticalSelectors(flush_early_driver);
  }

  CriticalCssFinder* css_finder = server_context->critical_css_finder();
  if (css_finder != NULL) {
    css_finder->UpdateCriticalCssInfoInDriver(flush_early_driver);
  }

  CacheHtmlInfoFinder* cache_html_finder =
      flush_early_driver->server_context()->cache_html_info_finder();
  if (cache_html_finder != NULL) {
    cache_html_finder->UpdateSplitInfoInDriver(flush_early_driver);
  }

  flush_early_driver->set_unowned_fallback_property_page(NULL);
}

}  // namespace

const char FlushEarlyFlow::kNumRequestsFlushedEarly[] =
    "num_requests_flushed_early";
const char FlushEarlyFlow::kNumResourcesFlushedEarly[] =
    "num_resources_flushed_early";
const char FlushEarlyFlow::kFlushEarlyRewriteLatencyMs[] =
    "flush_early_rewrite_latency_ms";
const char FlushEarlyFlow::kNumFlushEarlyHttpStatusCodeDeemedUnstable[] =
    "num_flush_early_http_status_code_deemed_unstable";
const char FlushEarlyFlow::kNumFlushEarlyRequestsRedirected[] =
    "num_flush_early_requests_redirected";
const char FlushEarlyFlow::kRedirectPageJs[] =
    "<script type=\"text/javascript\">window.location.replace(\"%s\")"
    "</script>";

// TODO(mmohabey): Do not flush early if the html is cacheable.
// If this is called then the content type must be html.
// TODO(nikhilmadan): Disable flush early if the response code isn't
// consistently a 200.

// AsyncFetch that manages the parallelization of FlushEarlyFlow with the
// ProxyFetch flow. Note that this fetch is passed to ProxyFetch as the
// base_fetch.
// While the FlushEarlyFlow is running, it buffers up bytes from the ProxyFetch
// flow, while streaming out bytes from the FlushEarlyFlow flow.
// Once the FlushEarlyFlow is completed, it writes out all the buffered bytes
// from ProxyFetch, after which it starts streaming bytes from ProxyFetch.
class FlushEarlyFlow::FlushEarlyAsyncFetch : public AsyncFetch {
 public:
  FlushEarlyAsyncFetch(AsyncFetch* fetch, AbstractMutex* mutex,
                       MessageHandler* message_handler,
                       const GoogleString& url,
                       ServerContext* server_context)
      : AsyncFetch(fetch->request_context()),
        base_fetch_(fetch),
        mutex_(mutex),
        message_handler_(message_handler),
        server_context_(server_context),
        url_(url),
        flush_early_flow_done_(false),
        flushed_early_(false),
        headers_complete_called_(false),
        flush_called_(false),
        done_called_(false),
        done_value_(false),
        non_ok_response_(false) {
    set_request_headers(fetch->request_headers());
    Statistics* stats = server_context_->statistics();
    num_flush_early_requests_redirected_ = stats->GetTimedVariable(
        kNumFlushEarlyRequestsRedirected);
  }

  // Indicates that the flush early flow is complete.
  void set_flush_early_flow_done(bool flushed_early) {
    bool should_delete = false;
    {
      ScopedMutex lock(mutex_.get());
      flush_early_flow_done_ = true;
      flushed_early_ = flushed_early;
      if (!flushed_early && headers_complete_called_) {
        base_fetch_->response_headers()->CopyFrom(*response_headers());
      }
      if (flushed_early && non_ok_response_) {
        SendRedirectToPsaOff();
      } else {
        // Write out all the buffered content and call Flush and Done if it were
        // called earlier.
        if (!buffered_content_.empty()) {
          base_fetch_->Write(buffered_content_, message_handler_);
          buffered_content_.clear();
        }
        if (flush_called_) {
          base_fetch_->Flush(message_handler_);
        }
      }
      if (done_called_) {
        base_fetch_->Done(done_value_);
        should_delete = true;
      }
    }
    if (should_delete) {
      delete this;
    }
  }

 private:
  // If the flush early flow isn't done yet, do nothing here since
  // set_flush_early_flow_done will do the needful.
  // If we flushed early, then the FlushEarlyFlow would have already set
  // the headers. Hence, do nothing.
  // If we didn't flush early, copy the response headers into the base fetch.
  virtual void HandleHeadersComplete() {
    {
      ScopedMutex lock(mutex_.get());
      non_ok_response_ =
          ((response_headers()->status_code() != HttpStatus::kOK) ||
           !response_headers()->IsHtmlLike());
      if (flushed_early_ && non_ok_response_) {
        SendRedirectToPsaOff();
        return;
      }
      if (!flush_early_flow_done_ || flushed_early_) {
        headers_complete_called_ = true;
        return;
      }
    }
    base_fetch_->response_headers()->CopyFrom(*response_headers());
  }

  // If the flush early flow is still in progress, buffer the bytes. Otherwise,
  // write them out to base_fetch.
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler) {
    {
      ScopedMutex lock(mutex_.get());
      if (flushed_early_ && non_ok_response_) {
        return true;
      }
      if (!flush_early_flow_done_) {
        buffered_content_.append(sp.data(), sp.size());
        return true;
      }
    }
    return base_fetch_->Write(sp, handler);
  }

  // If the flush early flow is still in progress, store the fact that flush
  // was called. Otherwise, pass the call to base_fetch.
  virtual bool HandleFlush(MessageHandler* handler) {
    {
      ScopedMutex lock(mutex_.get());
      if (flushed_early_ && non_ok_response_) {
        return true;
      }
      if (!flush_early_flow_done_) {
        flush_called_ = true;
        return true;
      }
    }
    return base_fetch_->Flush(handler);
  }

  // If the flush early flow is still in progress, store the fact that done was
  // called. Otherwise, pass the call to base_fetch.
  virtual void HandleDone(bool success) {
    {
      ScopedMutex lock(mutex_.get());
      if (!flush_early_flow_done_) {
        done_called_ = true;
        done_value_ = success;
        return;
      }
    }
    base_fetch_->Done(success);
    delete this;
  }

  void SendRedirectToPsaOff() {
    num_flush_early_requests_redirected_->IncBy(1);
    GoogleUrl gurl(url_);
    scoped_ptr<GoogleUrl> url_with_psa_off(gurl.CopyAndAddEscapedQueryParam(
        RewriteQuery::kPageSpeed, RewriteQuery::kNoscriptValue));
    GoogleString escaped_url;
    EscapeToJsStringLiteral(url_with_psa_off->Spec(), false,
                            &escaped_url);
    base_fetch_->Write(StringPrintf(kRedirectPageJs, escaped_url.c_str()),
                       message_handler_);
    base_fetch_->Write("</head><body></body></html>", message_handler_);
    base_fetch_->Flush(message_handler_);
  }

  AsyncFetch* base_fetch_;
  scoped_ptr<AbstractMutex> mutex_;
  MessageHandler* message_handler_;
  ServerContext* server_context_;
  GoogleString url_;
  GoogleString buffered_content_;
  bool flush_early_flow_done_;
  bool flushed_early_;
  bool headers_complete_called_;
  bool flush_called_;
  bool done_called_;
  bool done_value_;
  bool non_ok_response_;

  TimedVariable* num_flush_early_requests_redirected_;

  DISALLOW_COPY_AND_ASSIGN(FlushEarlyAsyncFetch);
};

void FlushEarlyFlow::TryStart(
    const GoogleString& url,
    AsyncFetch** base_fetch,
    RewriteDriver* driver,
    ProxyFetchFactory* factory,
    ProxyFetchPropertyCallbackCollector* property_cache_callback) {
  if (!driver->options()->Enabled(RewriteOptions::kFlushSubresources)) {
    return;
  }
  const RequestHeaders* request_headers = driver->request_headers();
  if (request_headers == NULL ||
      request_headers->method() != RequestHeaders::kGet ||
      request_headers->Has(HttpAttributes::kIfModifiedSince) ||
      request_headers->Has(HttpAttributes::kIfNoneMatch) ||
      driver->request_context()->split_request_type() ==
      RequestContext::SPLIT_BELOW_THE_FOLD) {
    driver->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kFlushSubresources),
        RewriterHtmlApplication::DISABLED);
    return;
  }
  if (!driver->request_properties()->CanPreloadResources()) {
    driver->log_record()->LogRewriterHtmlStatus(
        RewriteOptions::FilterId(RewriteOptions::kFlushSubresources),
        RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED);
    return;
  }

  FlushEarlyAsyncFetch* flush_early_fetch = new FlushEarlyAsyncFetch(
      *base_fetch, driver->server_context()->thread_system()->NewMutex(),
      driver->server_context()->message_handler(), url,
      driver->server_context());
  FlushEarlyFlow* flow = new FlushEarlyFlow(
      url, *base_fetch, flush_early_fetch, driver, factory,
      property_cache_callback);

  // Change the base_fetch in ProxyFetch to flush_early_fetch.
  *base_fetch = flush_early_fetch;
  Function* func = MakeFunction(flow, &FlushEarlyFlow::FlushEarly,
                                &FlushEarlyFlow::Cancel);
  property_cache_callback->AddPostLookupTask(func);
}

void FlushEarlyFlow::InitStats(Statistics* stats) {
  stats->AddTimedVariable(kNumRequestsFlushedEarly,
                          ServerContext::kStatisticsGroup);
  stats->AddTimedVariable(
      FlushEarlyContentWriterFilter::kNumResourcesFlushedEarly,
      ServerContext::kStatisticsGroup);
  stats->AddTimedVariable(kNumFlushEarlyHttpStatusCodeDeemedUnstable,
                          ServerContext::kStatisticsGroup);
  stats->AddTimedVariable(kNumFlushEarlyRequestsRedirected,
                          ServerContext::kStatisticsGroup);
  stats->AddHistogram(kFlushEarlyRewriteLatencyMs)->EnableNegativeBuckets();
}

FlushEarlyFlow::FlushEarlyFlow(
    const GoogleString& url,
    AsyncFetch* base_fetch,
    FlushEarlyAsyncFetch* flush_early_fetch,
    RewriteDriver* driver,
    ProxyFetchFactory* factory,
    ProxyFetchPropertyCallbackCollector* property_cache_callback)
    : url_(url),
      dummy_head_writer_(&dummy_head_),
      num_resources_flushed_(0),
      num_rewritten_resources_(0),
      average_fetch_time_(0),
      base_fetch_(base_fetch),
      flush_early_fetch_(flush_early_fetch),
      driver_(driver),
      factory_(factory),
      server_context_(driver->server_context()),
      property_cache_callback_(property_cache_callback),
      should_flush_early_lazyload_script_(false),
      handler_(driver_->server_context()->message_handler()) {
  Statistics* stats = server_context_->statistics();
  num_requests_flushed_early_ = stats->GetTimedVariable(
      kNumRequestsFlushedEarly);
  num_resources_flushed_early_ = stats->GetTimedVariable(
      FlushEarlyContentWriterFilter::kNumResourcesFlushedEarly);
  num_flush_early_http_status_code_deemed_unstable_ = stats->GetTimedVariable(
      kNumFlushEarlyHttpStatusCodeDeemedUnstable);
  flush_early_rewrite_latency_ms_ = stats->GetHistogram(
      kFlushEarlyRewriteLatencyMs);
  driver_->increment_async_events_count();
  // If mobile, do not flush preconnects as it can potentially block useful
  // connections to resources. This is also used to determine whether to
  // flush early the lazy load js snippet.
  is_mobile_user_agent_ = driver_->request_properties()->IsMobile();
}

FlushEarlyFlow::~FlushEarlyFlow() {
  driver_->decrement_async_events_count();
}

void FlushEarlyFlow::FlushEarly() {
  const RewriteOptions* options = driver_->options();
  const PropertyCache::Cohort* cohort = server_context_->dom_cohort();
  PropertyPage* page = property_cache_callback_->property_page();
  AbstractPropertyPage* fallback_page =
      property_cache_callback_->fallback_property_page();
  DCHECK(page != NULL);
  bool property_cache_miss = true;
  if (page != NULL && cohort != NULL) {
    PropertyValue* num_rewritten_resources_property_value = page->GetProperty(
        cohort,
        RewrittenContentScanningFilter::kNumProxiedRewrittenResourcesProperty);

    if (num_rewritten_resources_property_value->has_value()) {
      StringToInt(num_rewritten_resources_property_value->value().data(),
                  &num_rewritten_resources_);
    }
    PropertyValue* status_code_property_value = fallback_page->GetProperty(
        cohort, RewriteDriver::kStatusCodePropertyName);

    // We do not trigger flush early flow if the status code of the response is
    // not constant for property_cache_http_status_stability_threshold previous
    // requests.
    bool status_code_property_value_recently_constant =
        !status_code_property_value->has_value() ||
        status_code_property_value->IsRecentlyConstant(
            options->property_cache_http_status_stability_threshold());
    if (!status_code_property_value_recently_constant) {
      num_flush_early_http_status_code_deemed_unstable_->IncBy(1);
    }

    PropertyValue* property_value = fallback_page->GetProperty(
        cohort, RewriteDriver::kSubresourcesPropertyName);
    if (property_value != NULL && property_value->has_value()) {
      property_cache_miss = false;
      FlushEarlyInfo flush_early_info;
      ArrayInputStream value(property_value->value().data(),
                             property_value->value().size());
      flush_early_info.ParseFromZeroCopyStream(&value);
      if (!flush_early_info.http_only_cookie_present() &&
          flush_early_info.has_resource_html() &&
          !flush_early_info.resource_html().empty() &&
          flush_early_info.response_headers().status_code() ==
          HttpStatus::kOK && status_code_property_value_recently_constant) {
        // If the flush early info has non-empty resource html, we flush early.

        // Check whether to flush lazyload script snippets early.
        PropertyValue* lazyload_property_value = fallback_page->GetProperty(
            cohort,
            LazyloadImagesFilter::kIsLazyloadScriptInsertedPropertyName);
        if (lazyload_property_value->has_value() &&
            StringCaseEqual(lazyload_property_value->value(), "1") &&
            options->Enabled(RewriteOptions::kLazyloadImages) &&
            (LazyloadImagesFilter::ShouldApply(driver_) ==
             RewriterHtmlApplication::ACTIVE) &&
            !is_mobile_user_agent_) {
          driver_->set_is_lazyload_script_flushed(true);
          should_flush_early_lazyload_script_ = true;
        }

        int64 now_ms = server_context_->timer()->NowMs();
        // Clone the RewriteDriver which is used rewrite the HTML that we are
        // trying to flush early.
        RewriteDriver* new_driver = driver_->Clone();
        new_driver->increment_async_events_count();
        new_driver->set_response_headers_ptr(base_fetch_->response_headers());
        new_driver->SetRequestHeaders(*driver_->request_headers());
        new_driver->set_flushing_early(true);
        new_driver->SetWriter(base_fetch_);
        new_driver->SetUserAgent(driver_->user_agent());
        new_driver->StartParse(url_);

        new_driver->log_record()->SetRewriterLoggingStatus(
            RewriteOptions::FilterId(RewriteOptions::kFlushSubresources),
            RewriterApplication::APPLIED_OK);

        // Allow the usage of fallback properties for critical images.
        InitFlushEarlyDriverWithPropertyCacheValues(
            new_driver, property_cache_callback_->fallback_property_page());
        if (flush_early_info.has_average_fetch_latency_ms()) {
          average_fetch_time_ = flush_early_info.average_fetch_latency_ms();
        }
        // Copy over the response headers from flush_early_info.
        GenerateResponseHeaders(flush_early_info);

        // Write the pre-head content out to the user. Note that we also pass
        // the pre-head content to new_driver but it is not written out by it.
        // This is so that we can flush other content such as the javascript
        // needed by filters from here. Also, we may need the pre-head to detect
        // the encoding of the page.
        base_fetch_->Write(flush_early_info.pre_head(), handler_);

        // Parse and rewrite the flush early HTML.
        new_driver->ParseText(flush_early_info.pre_head());
        new_driver->ParseText(flush_early_info.resource_html());

        if (new_driver->options()->
            flush_more_resources_early_if_time_permits()) {
          const StringSet& css_critical_images = new_driver->server_context()->
              critical_images_finder()->GetCssCriticalImages(new_driver);
          // Critical images inside css.
          StringSet::iterator it = css_critical_images.begin();
          for (; it != css_critical_images.end(); ++it) {
            new_driver->ParseText("<img src='");
            GoogleString escaped_url;
            HtmlKeywords::Escape(*it, &escaped_url);
            new_driver->ParseText(escaped_url);
            new_driver->ParseText("' />");
          }
        }
        driver_->set_flushed_early(true);
        driver_->log_record()->LogRewriterHtmlStatus(
            RewriteOptions::FilterId(RewriteOptions::kFlushSubresources),
            RewriterHtmlApplication::ACTIVE);
        // Keep driver alive till the FlushEarlyFlow is completed.
        num_requests_flushed_early_->IncBy(1);

        // This deletes the driver once done.
        new_driver->FinishParseAsync(
            MakeFunction(this, &FlushEarlyFlow::FlushEarlyRewriteDone, now_ms,
                         new_driver));
        return;
      }
    }
  }
  driver_->log_record()->LogRewriterHtmlStatus(
      RewriteOptions::FilterId(RewriteOptions::kFlushSubresources),
      (property_cache_miss ?
       RewriterHtmlApplication::PROPERTY_CACHE_MISS :
       RewriterHtmlApplication::DISABLED));
  flush_early_fetch_->set_flush_early_flow_done(driver_->flushed_early());
  delete this;
}

void FlushEarlyFlow::Cancel() {
  flush_early_fetch_->set_flush_early_flow_done(false);
  delete this;
}

void FlushEarlyFlow::FlushEarlyRewriteDone(int64 start_time_ms,
                                           RewriteDriver* flush_early_driver) {
  int max_preconnect_attempts = std::min(
      kMaxParallelConnections, num_rewritten_resources_ -
      flush_early_driver->num_flushed_early_pagespeed_resources()) -
      flush_early_driver->num_flushed_early_pagespeed_resources();

  if (should_flush_early_lazyload_script_) {
    // Flush Lazyload filter script content.
    StaticAssetManager* static_asset_manager =
          server_context_->static_asset_manager();
    GoogleString script_content = LazyloadImagesFilter::GetLazyloadJsSnippet(
        driver_->options(), static_asset_manager);
    base_fetch_->Write(StringPrintf(kJavascriptInline,
        script_content.c_str()), handler_);
    if (!driver_->options()->lazyload_images_blank_url().empty()) {
      --max_preconnect_attempts;
    }
  }

  if (!is_mobile_user_agent_ && max_preconnect_attempts > 0 &&
      !flush_early_driver->options()->pre_connect_url().empty() &&
      average_fetch_time_ > kMinLatencyForPreconnectMs) {
    for (int index = 0; index < max_preconnect_attempts; ++index) {
      GoogleString url =
          StrCat(flush_early_driver->options()->pre_connect_url(),
                 "?id=", IntegerToString(index));
      base_fetch_->Write(StringPrintf("<link rel=\"stylesheet\" href=\"%s\"/>",
                                      url.c_str()), handler_);
    }
  }
  flush_early_driver->decrement_async_events_count();
  base_fetch_->Flush(handler_);
  flush_early_rewrite_latency_ms_->Add(
      server_context_->timer()->NowMs() - start_time_ms);
  // We delete FlushEarlyFlow first to prevent the race conditon where
  // tests finish before the destructor of FlushEarlyFlow gets called
  // and hence decrement async events on the driver doesn't happen.
  FlushEarlyAsyncFetch* flush_early_fetch = flush_early_fetch_;
  delete this;
  flush_early_fetch->set_flush_early_flow_done(true);
}

void FlushEarlyFlow::GenerateResponseHeaders(
    const FlushEarlyInfo& flush_early_info) {
  ResponseHeaders* response_headers = base_fetch_->response_headers();
  response_headers->UpdateFromProto(flush_early_info.response_headers());
  // TODO(mmohabey): Add this header only when debug filter is on.
  {
    ScopedMutex lock(driver_->log_record()->mutex());
    response_headers->Add(
        kPsaRewriterHeader, driver_->log_record()->AppliedRewritersString());
  }
  response_headers->SetDateAndCaching(server_context_->timer()->NowMs(), 0,
                                      ", private, no-cache");

  if ((driver_->options()->Enabled(RewriteOptions::kDeferJavascript) ||
       driver_->options()->Enabled(RewriteOptions::kSplitHtml)) &&
      driver_->user_agent_matcher()->IsIe(driver_->user_agent()) &&
      !response_headers->Has(HttpAttributes::kXUACompatible)) {
    response_headers->Add(HttpAttributes::kXUACompatible, "IE=edge");
  }

  response_headers->ComputeCaching();
  base_fetch_->HeadersComplete();
}

void FlushEarlyFlow::Write(const StringPiece& val) {
  dummy_head_writer_.Write(val, handler_);
}

}  // namespace net_instaweb
