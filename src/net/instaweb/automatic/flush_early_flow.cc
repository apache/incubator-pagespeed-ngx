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

#include "base/logging.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/http/http.pb.h"  // for HttpResponseHeaders
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/meta_data.h"  // for Code::kOK
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/js/public/js_minify.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/flush_early_content_writer_filter.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/proto_util.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"  // for Timer

namespace {

const char kPreloadScript[] = "function preload(x){"
    "var obj=document.createElement('object');"
    "obj.data=x;"
    "obj.width=0;"
    "obj.height=0;}";
const char kScriptBlock[] =
    "<script type=\"text/javascript\">(function(){%s})()</script>";

const char kFlushSubresourcesFilter[] = "FlushSubresourcesFilter";

const char kPrefetchObjectTagHtml[] = "preload(%s);";


}  // namespace

namespace net_instaweb {

const char FlushEarlyFlow::kNumRequestsFlushedEarly[] =
    "num_requests_flushed_early";
const char FlushEarlyFlow::kNumResourcesFlushedEarly[] =
    "num_resources_flushed_early";
const char FlushEarlyFlow::kFlushEarlyRewriteLatencyMs[] =
    "flush_early_rewrite_latency_ms";

// TODO(mmohabey): Do Cookie handling when flushed early. If the cookie is
// HttpOnly then do not enter FlushEarlyFlow.
// TODO(mmohabey): Do not flush early if the html is cacheable.
// If this is called then the content type must be html.
// TODO(mmohabey): Enable it for browsers other than Chrome. Temporarily enabled
// for just one browser since same resource might have different url depending
// on the browser. So if subresources are collected in browser A and flushed
// early in browser B then it causes performance degradation.
bool FlushEarlyFlow::CanFlushEarly(const GoogleString& url,
                                   const AsyncFetch* async_fetch,
                                   const RewriteDriver* driver) {
  const RewriteOptions* options = driver->options();
  return (options != NULL && options->enabled() &&
          options->Enabled(RewriteOptions::kFlushSubresources) &&
          async_fetch->request_headers()->method() == RequestHeaders::kGet &&
          driver->UserAgentSupportsFlushEarly() &&
          options->IsAllowed(url));
}

// AsyncFetch that doesn't call HeadersComplete() on the base fetch. Note that
// this class only links the request headers from the base fetch and does not
// link the response headers.
class FlushEarlyAsyncFetch : public AsyncFetchUsingWriter {
 public:
  explicit FlushEarlyAsyncFetch(AsyncFetch* fetch)
      : AsyncFetchUsingWriter(fetch),
        base_fetch_(fetch) {
    set_request_headers(fetch->request_headers());
    set_logging_info(fetch->logging_info());
  }

 private:
  // base_fetch::HeadersComplete() was already called by
  // FlushEarlyFlow::GenerateResponseHeaders on so do not call it again.
  virtual void HandleHeadersComplete() {}

  virtual void HandleDone(bool success) {
    base_fetch_->Done(success);
    delete this;
  }

  AsyncFetch* base_fetch_;

  DISALLOW_COPY_AND_ASSIGN(FlushEarlyAsyncFetch);
};

void FlushEarlyFlow::Start(
    const GoogleString& url,
    AsyncFetch* base_fetch,
    RewriteDriver* driver,
    ProxyFetchFactory* factory,
    ProxyFetchPropertyCallbackCollector* property_cache_callback) {
  FlushEarlyFlow* flow = new FlushEarlyFlow(
      url, base_fetch, driver, factory, property_cache_callback);
  Function* func = MakeFunction(flow, &FlushEarlyFlow::FlushEarly);
  property_cache_callback->AddPostLookupTask(func);
}

void FlushEarlyFlow::Initialize(Statistics* stats) {
  stats->AddTimedVariable(kNumRequestsFlushedEarly,
                          ResourceManager::kStatisticsGroup);
  stats->AddTimedVariable(
      FlushEarlyContentWriterFilter::kNumResourcesFlushedEarly,
      ResourceManager::kStatisticsGroup);
  stats->AddHistogram(kFlushEarlyRewriteLatencyMs);
}

FlushEarlyFlow::FlushEarlyFlow(
    const GoogleString& url,
    AsyncFetch* base_fetch,
    RewriteDriver* driver,
    ProxyFetchFactory* factory,
    ProxyFetchPropertyCallbackCollector* property_cache_callback)
    : url_(url),
      dummy_head_writer_(&dummy_head_),
      num_resources_flushed_(0),
      base_fetch_(base_fetch),
      flush_early_fetch_(NULL),
      driver_(driver),
      factory_(factory),
      manager_(driver->resource_manager()),
      property_cache_callback_(property_cache_callback),
      handler_(driver_->resource_manager()->message_handler()) {
  Statistics* stats = manager_->statistics();
  num_requests_flushed_early_ = stats->GetTimedVariable(
      kNumRequestsFlushedEarly);
  num_resources_flushed_early_ = stats->GetTimedVariable(
      FlushEarlyContentWriterFilter::kNumResourcesFlushedEarly);
  flush_early_rewrite_latency_ms = stats->AddHistogram(
      kFlushEarlyRewriteLatencyMs);
  flush_early_rewrite_latency_ms->EnableNegativeBuckets();
}

FlushEarlyFlow::~FlushEarlyFlow() {}

void FlushEarlyFlow::FlushEarly() {
  const PropertyCache::Cohort* cohort = manager_->page_property_cache()->
      GetCohort(RewriteDriver::kDomCohort);
  PropertyPage* page =
      property_cache_callback_->GetPropertyPageWithoutOwnership(
          ProxyFetchPropertyCallback::kPagePropertyCache);
  if (page != NULL && cohort != NULL) {
    PropertyValue* property_value = page->GetProperty(
        cohort, RewriteDriver::kSubresourcesPropertyName);

    if (property_value != NULL && property_value->has_value()) {
      FlushEarlyInfo flush_early_info;
      ArrayInputStream value(property_value->value().data(),
                             property_value->value().size());
      flush_early_info.ParseFromZeroCopyStream(&value);
      if (flush_early_info.has_resource_html() &&
          !flush_early_info.resource_html().empty()) {
        // If the flush early info has non-empty resource html, we flush early.
        DCHECK(driver_->options()->enable_flush_subresources_experimental());

        int64 now_ms = manager_->timer()->NowMs();
        // Clone the RewriteDriver which is used rewrite the HTML that we are
        // trying to flush early.
        RewriteDriver* new_driver = driver_->Clone();
        new_driver->set_response_headers_ptr(base_fetch_->response_headers());
        new_driver->set_flushing_early(true);
        new_driver->SetWriter(base_fetch_);
        new_driver->set_user_agent(driver_->user_agent());
        new_driver->StartParse(url_);

        // Copy over the response headers from flush_early_info.
        GenerateResponseHeaders(flush_early_info);

        // Write the pre-head content out to the user. Note that we also pass
        // the pre-head content to new_driver but it is not written out by it.
        // This is so that we can flush other content such as the javascript
        // needed by filters from here. Also, we may need the pre-head to detect
        // the encoding of the page.
        base_fetch_->Write(flush_early_info.pre_head(), handler_);
        base_fetch_->Write("<head>", handler_);
        base_fetch_->Write(flush_early_info.content_type_meta_tag(), handler_);
        base_fetch_->Flush(handler_);

        // Parse and rewrite the flush early HTML.
        new_driver->ParseText(flush_early_info.pre_head());
        new_driver->ParseText("<head>");
        new_driver->ParseText(flush_early_info.content_type_meta_tag());
        new_driver->ParseText(flush_early_info.resource_html());
        driver_->set_flushed_early(true);
        num_requests_flushed_early_->IncBy(1);
        // This deletes the driver once done.
        new_driver->FinishParseAsync(
            MakeFunction(this, &FlushEarlyFlow::FlushEarlyRewriteDone, now_ms));
        return;
      } else {
        GenerateDummyHeadAndCountResources(flush_early_info);
        if (flush_early_info.response_headers().status_code() ==
            HttpStatus::kOK && num_resources_flushed_ > 0) {
          handler_->Message(kInfo, "Flushed %d Subresources Early for %s.",
                            num_resources_flushed_, url_.c_str());
          num_requests_flushed_early_->IncBy(1);
          num_resources_flushed_early_->IncBy(num_resources_flushed_);
          GenerateResponseHeaders(flush_early_info);
          base_fetch_->Write(dummy_head_, handler_);
          base_fetch_->Flush(handler_);
          driver_->set_flushed_early(true);
        }
      }
    }
  }
  TriggerProxyFetch();
}

void FlushEarlyFlow::FlushEarlyRewriteDone(int64 start_time_ms) {
  base_fetch_->Write("</head>", handler_);
  base_fetch_->Flush(handler_);
  flush_early_rewrite_latency_ms->Add(
      manager_->timer()->NowMs() - start_time_ms);
  TriggerProxyFetch();
}

void FlushEarlyFlow::TriggerProxyFetch() {
  AsyncFetch* fetch = driver_->flushed_early() ?
      new FlushEarlyAsyncFetch(base_fetch_) : base_fetch_;
  factory_->StartNewProxyFetch(
      url_, fetch, driver_, property_cache_callback_, NULL);
  delete this;
}

void FlushEarlyFlow::GenerateResponseHeaders(
    const FlushEarlyInfo& flush_early_info) {
  ResponseHeaders* response_headers = base_fetch_->response_headers();
  response_headers->UpdateFromProto(flush_early_info.response_headers());
  // TODO(mmohabey): Add this header only when debug filter is on.
  response_headers->Add(kPsaRewriterHeader, kFlushSubresourcesFilter);
  response_headers->SetDateAndCaching(manager_->timer()->NowMs(), 0,
                                      ", private, no-cache");
  response_headers->ComputeCaching();
  base_fetch_->HeadersComplete();
}

void FlushEarlyFlow::GenerateDummyHeadAndCountResources(
    const FlushEarlyInfo& flush_early_info) {
  Write(flush_early_info.pre_head());
  Write("<head>");
  Write(flush_early_info.content_type_meta_tag());
  GoogleString head_string, script, minified_script;
  bool has_script = false;
  switch (manager_->user_agent_matcher().GetPrefetchMechanism(
      driver_->user_agent().data())) {
    case UserAgentMatcher::kPrefetchNotSupported:
      LOG(DFATAL) << "Entered Flush Early Flow for a unsupported user agent";
      break;
    case UserAgentMatcher::kPrefetchLinkRelSubresource:
      head_string = GetHeadString(
          flush_early_info,
          FlushEarlyContentWriterFilter::kPrefetchLinkRelSubresourceHtml);
      break;
    case UserAgentMatcher::kPrefetchImageTag:
      has_script = true;
      script = GetHeadString(
          flush_early_info,
          FlushEarlyContentWriterFilter::kPrefetchImageTagHtml);
      break;
    case UserAgentMatcher::kPrefetchObjectTag:
      has_script = true;
      StrAppend(&script, kPreloadScript, GetHeadString(flush_early_info,
                kPrefetchObjectTagHtml));
      break;
  }
  if (has_script) {
    if (!driver_->options()->Enabled(RewriteOptions::kDebug)) {
      pagespeed::js::MinifyJs(script, &minified_script);
      Write(StringPrintf(kScriptBlock, minified_script.c_str()));
    } else {
      Write(StringPrintf(kScriptBlock, script.c_str()));
    }
  } else {
    Write(head_string);
  }
  Write(StringPrintf(FlushEarlyContentWriterFilter::kPrefetchStartTimeScript,
                     num_resources_flushed_));
  Write("</head>");
}

GoogleString FlushEarlyFlow::GetHeadString(
    const FlushEarlyInfo& flush_early_info, const char* format) {
  GoogleString head_string;
  for (int i = 0; i < flush_early_info.subresource_size(); ++i) {
    if (driver_->options()->Enabled(RewriteOptions::kDeferJavascript)) {
      if (flush_early_info.subresource(i).content_type() == JAVASCRIPT) {
        continue;
      }
    }
    StrAppend(&head_string, StringPrintf(
        format, flush_early_info.subresource(i).rewritten_url().c_str()));
    ++num_resources_flushed_;
  }
  return head_string;
}

void FlushEarlyFlow::Write(const StringPiece& val) {
  dummy_head_writer_.Write(val, handler_);
}

}  // namespace net_instaweb
