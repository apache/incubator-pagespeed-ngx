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

}  // namespace

namespace net_instaweb {

const char FlushEarlyFlow::kNumRequestsFlushedEarly[] =
    "num_requests_flushed_early";
const char FlushEarlyFlow::kNumResourcesFlushedEarly[] =
    "num_resources_flushed_early";

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
  stats->AddTimedVariable(kNumResourcesFlushedEarly,
                          ResourceManager::kStatisticsGroup);
}

FlushEarlyFlow::FlushEarlyFlow(
    const GoogleString& url,
    AsyncFetch* base_fetch,
    RewriteDriver* driver,
    ProxyFetchFactory* factory,
    ProxyFetchPropertyCallbackCollector* property_cache_callback)
    : url_(url),
      base_fetch_(base_fetch),
      driver_(driver),
      factory_(factory),
      manager_(driver->resource_manager()),
      property_cache_callback_(property_cache_callback),
      handler_(driver_->resource_manager()->message_handler()) {
  Statistics* stats = manager_->statistics();
  num_requests_flushed_early_ = stats->GetTimedVariable(
      kNumRequestsFlushedEarly);
  num_resources_flushed_early_ = stats->GetTimedVariable(
      kNumResourcesFlushedEarly);
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
      if (flush_early_info.response_headers().status_code() == HttpStatus::kOK
          && flush_early_info.resources_size() > 0) {
        handler_->Message(kInfo, "Flushed %d Subresources Early for %s.",
                          flush_early_info.resources_size(), url_.c_str());
        num_requests_flushed_early_->IncBy(1);
        num_resources_flushed_early_->IncBy(flush_early_info.resources_size());
        GenerateResponseHeaders(flush_early_info);
        GenerateDummyHeadAndFlush(flush_early_info);
        driver_->set_flushed_early(true);
      }
    }
  }
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

void FlushEarlyFlow::GenerateDummyHeadAndFlush(
    const FlushEarlyInfo& flush_early_info) {
  Write(flush_early_info.pre_head());
  Write("<head>");
  GoogleString head_string, script, minified_script;
  bool has_script = false;
  switch (manager_->user_agent_matcher().GetPrefetchMechanism(
      driver_->user_agent().data())) {
    case UserAgentMatcher::kPrefetchNotSupported:
      LOG(DFATAL) << "Entered Flush Early Flow for a unsupported user agent";
      break;
    case UserAgentMatcher::kPrefetchLinkRelSubresource:
      for (int i = 0; i < flush_early_info.resources_size(); ++i) {
        StrAppend(&head_string, "<link rel=\"subresource\" href=\"",
                  flush_early_info.resources(i), "\"/>");
      }
      break;
    case UserAgentMatcher::kPrefetchImageTag:
      has_script = true;
      for (int i = 0; i < flush_early_info.resources_size(); ++i) {
        StrAppend(&script, "new Image().src=\"",
                  flush_early_info.resources(i), "\";");
      }
      break;
    case UserAgentMatcher::kPrefetchObjectTag:
      has_script = true;
      StrAppend(&script, kPreloadScript);
      for (int i = 0; i < flush_early_info.resources_size(); ++i) {
        StrAppend(&script, "preload(", flush_early_info.resources(i), ");");
      }
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
  Write("</head>");
  base_fetch_->Flush(handler_);
}

void FlushEarlyFlow::Write(const StringPiece& val) {
  base_fetch_->Write(val, handler_);
}

}  // namespace net_instaweb
