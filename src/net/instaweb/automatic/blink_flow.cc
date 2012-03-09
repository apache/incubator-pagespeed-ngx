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

// Author: nikhilmadan@google.com (Nikhil Madan)
//
// This class manages the flow of a blink request. In order to flush the layout
// and cacheable panels early before we start getting bytes back from the
// fetcher, we trigger a cache lookup for the json.
// If the json is found, we flush json out and then trigger the normal
// ProxyFetch flow with customized options which extracts cookies and
// non cacheable panels from the page and sends it out.
// If the json is not found in cache, we pass this request through the normal
// ProxyFetch flow and trigger an asynchronous fetch for the page,
// create a driver to parse it and store the extracted json in the cache.

#include "net/instaweb/automatic/public/blink_flow.h"

#include <cstddef>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/panel_config.pb.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/static_javascript_manager.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/timer.h"
#include "third_party/jsoncpp/include/json/json.h"

namespace net_instaweb {

const int kJsonCachePrefixLength = strlen(BlinkUtil::kJsonCachePrefix);

namespace {
const char kTimeToBlinkFlowStart[] = "TIME_TO_BLINK_FLOW_START";
const char kTimeToJsonLookupDone[] = "TIME_TO_JSON_LOOKUP_DONE";
const char kTimeToSplitCritical[] = "TIME_TO_SPLIT_CRITICAL";
const char kLayoutLoaded[] = "LAYOUT_LOADED";

// AsyncFetch that doesn't call HeadersComplete() on the base fetch. Note that
// this class only links the request headers from the base fetch and does not
// link the response headers.
// This is used as a wrapper around the base fetch when the json is found in
// cache. This is done because the response headers and the json have been
// already been flushed out in the base fetch and we don't want to call
// HeadersComplete() twice on the base fetch.
// This class deletes itself when HandleDone() is called.
class AsyncFetchWithHeadersInhibited : public AsyncFetchUsingWriter {
 public:
  explicit AsyncFetchWithHeadersInhibited(AsyncFetch* fetch)
      : AsyncFetchUsingWriter(fetch),
        base_fetch_(fetch) {
    set_request_headers(fetch->request_headers());
  }

 private:
  virtual ~AsyncFetchWithHeadersInhibited() {}
  virtual void HandleHeadersComplete() {}
  virtual void HandleDone(bool success) {
    base_fetch_->Done(success);
    delete this;
  }

  AsyncFetch* base_fetch_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFetchWithHeadersInhibited);
};

// SharedAsyncFetch that fetches the page and passes events through to the base
// fetch. It also determines if the page is html, and whether to trigger an
// async computation of the json.
// TODO(rahulbansal): Buffer the html chunked rather than in one string.
class SharedJsonFetch : public SharedAsyncFetch {
 public:
  SharedJsonFetch(AsyncFetch* base_fetch,
                  const GoogleString& key,
                  ResourceManager* resource_manager,
                  RewriteOptions* options)
      : SharedAsyncFetch(base_fetch),
        key_(key),
        resource_manager_(resource_manager),
        options_(options),
        compute_json_(false) {
    Statistics* stats = resource_manager_->statistics();
    num_shared_json_fetches_complete_ = stats->GetTimedVariable(
        BlinkFlow::kNumSharedJsonFetchesComplete);
  }

  virtual ~SharedJsonFetch() {}

  virtual void HandleHeadersComplete() {
    base_fetch()->HeadersComplete();
    if (response_headers()->status_code() == HttpStatus::kOK) {
      const ContentType* type = response_headers()->DetermineContentType();
      if (type != NULL && type->IsHtmlLike()) {
        compute_json_ = true;
      } else {
        VLOG(1) << "Non html page, not rewritable: " << key_;
      }
    } else {
      VLOG(1) << "Non 200 response code for: " << key_;
    }
  }

  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    bool ret = base_fetch()->Write(content, handler);
    if (compute_json_) {
      content.AppendToString(&json_buffer_);
    }
    return ret;
  }

  virtual void HandleDone(bool success) {
    num_shared_json_fetches_complete_->IncBy(1);
    compute_json_ &= success;
    if (!compute_json_) {
      base_fetch()->Done(success);
      delete this;
    } else {
      // Store base_fetch() in a temp since it might get deleted before calling
      // Done.
      AsyncFetch* fetch = base_fetch();
      json_headers_.CopyFrom(*response_headers());

      json_computation_driver_ = resource_manager_->NewCustomRewriteDriver(
          options_.release());
      // TODO(rahulbansal): Put an increased deadline on this driver.
      json_computation_driver_->SetWriter(&value_);
      json_computation_driver_->set_response_headers_ptr(&json_headers_);
      json_computation_driver_->AddRewriteTask(
          MakeFunction(this, &SharedJsonFetch::Parse));
      // We call Done after scheduling the rewrite on the driver since we expect
      // this to be very low cost. Calling Done on base_fetch() before
      // scheduling the rewrite causes problems with testing.
      fetch->Done(success);
    }
  }

  void Parse() {
    GoogleString url = key_.substr(kJsonCachePrefixLength);
    if (json_computation_driver_->StartParse(url)) {
      json_computation_driver_->ParseText(json_buffer_);
      json_computation_driver_->FinishParseAsync(MakeFunction(
          this, &SharedJsonFetch::CompleteFinishParse));
    } else {
      LOG(ERROR) << "StartParse failed for url: " << url;
      json_computation_driver_->Cleanup();
      delete this;
    }
  }

  void CompleteFinishParse() {
    delete this;
  }

 private:
  GoogleString key_;
  ResourceManager* resource_manager_;
  scoped_ptr<RewriteOptions> options_;
  bool compute_json_;
  ResponseHeaders json_headers_;
  GoogleString json_buffer_;
  HTTPValue value_;
  TimedVariable* num_shared_json_fetches_complete_;

  RewriteDriver* json_computation_driver_;

  DISALLOW_COPY_AND_ASSIGN(SharedJsonFetch);
};

}  // namespace

class BlinkFlow::JsonFindCallback : public OptionsAwareHTTPCacheCallback {
 public:
  explicit JsonFindCallback(BlinkFlow* blink_fetch)
      : OptionsAwareHTTPCacheCallback(blink_fetch->options_),
        blink_fetch_(blink_fetch) {}

  virtual ~JsonFindCallback() {}

  virtual void Done(HTTPCache::FindResult find_result) {
    blink_fetch_->time_to_json_lookup_done_ms_ =
        blink_fetch_->GetTimeElapsedFromStartRequest();

    if (find_result != HTTPCache::kFound) {
      blink_fetch_->JsonCacheMiss();
    } else {
      StringPiece contents;
      http_value()->ExtractContents(&contents);
      blink_fetch_->JsonCacheHit(contents, *response_headers());
    }
    delete this;
  }

 private:
  BlinkFlow* blink_fetch_;
  DISALLOW_COPY_AND_ASSIGN(JsonFindCallback);
};

const char BlinkFlow::kNumSharedJsonFetchesStarted[] =
    "num_shared_json_fetches_started";
const char BlinkFlow::kNumSharedJsonFetchesComplete[] =
    "num_shared_json_fetches_complete";
const char BlinkFlow::kAboveTheFold[] = "Above the fold";

BlinkFlow::BlinkFlow(const GoogleString& url,
                     AsyncFetch* base_fetch,
                     const Layout* layout,
                     RewriteOptions* options,
                     ProxyFetchFactory* factory,
                     ResourceManager* manager)
    : url_(url),
      base_fetch_(base_fetch),
      layout_(layout),
      options_(options),
      factory_(factory),
      manager_(manager),
      request_start_time_ms_(-1),
      time_to_start_blink_flow_ms_(-1),
      time_to_json_lookup_done_ms_(-1),
      time_to_split_critical_ms_(-1) {
  Statistics* stats = manager_->statistics();
  num_shared_json_fetches_started_ = stats->GetTimedVariable(
      kNumSharedJsonFetchesStarted);
}

void BlinkFlow::Start(const GoogleString& url,
                      AsyncFetch* base_fetch,
                      const Layout* layout,
                      RewriteOptions* options,
                      ProxyFetchFactory* factory,
                      ResourceManager* manager) {
  BlinkFlow* flow = new BlinkFlow(url, base_fetch, layout, options, factory,
                                  manager);
  flow->InitiateJsonLookup();
}

void BlinkFlow::Initialize(Statistics* stats) {
  stats->AddTimedVariable(kNumSharedJsonFetchesStarted,
                          ResourceManager::kStatisticsGroup);
  stats->AddTimedVariable(kNumSharedJsonFetchesComplete,
                          ResourceManager::kStatisticsGroup);
}

void BlinkFlow::InitiateJsonLookup() {
  // TODO(rahulbansal): Add this field to timing info proto and remove this
  // header.
  const char* request_start_time_ms_str =
      base_fetch_->request_headers()->Lookup1(kRequestStartTimeHeader);
  if (request_start_time_ms_str != NULL) {
    if (!StringToInt64(request_start_time_ms_str, &request_start_time_ms_)) {
      request_start_time_ms_ = 0;
    }
  }

  time_to_start_blink_flow_ms_ = GetTimeElapsedFromStartRequest();

  GoogleUrl gurl(url_);
  json_url_ = StrCat(BlinkUtil::kJsonCachePrefix, gurl.Spec());
  JsonFindCallback* callback = new JsonFindCallback(this);
  manager_->http_cache()->Find(json_url_,
                               manager_->message_handler(),
                               callback);
}

BlinkFlow::~BlinkFlow() {}

void BlinkFlow::JsonCacheHit(const StringPiece& content,
                             const ResponseHeaders& headers) {
  Json::Reader json_reader;
  Json::Value json;
  std::string json_str = std::string(content.data(), content.size());
  if (!json_reader.parse(json_str, json)) {
    LOG(ERROR) << "Couldn't parse Json from cache for url " << url_;
    VLOG(1) << "Unparseable json is " << json_str;
    JsonCacheMiss();
    return;
  }

  StringPiece layout = (json)[0][BlinkUtil::kInstanceHtml].asCString();
  // NOTE: Since we compute layout in background and only get it in serialized
  // form, we have to strip everything after the layout marker.
  size_t pos = layout.find(BlinkUtil::kLayoutMarker);
  if (pos == StringPiece::npos) {
    LOG(ERROR) << "Layout marker not found for url " << url_;
    VLOG(1) << "Layout without marker is " << layout;
    JsonCacheMiss();
    return;
  }

  ResponseHeaders* response_headers = base_fetch_->response_headers();
  response_headers->CopyFrom(headers);
  response_headers->Add(kPsaRewriterHeader, BlinkFlow::kAboveTheFold);
  // Remove any Etag headers from the json response. Note that an Etag is
  // added by the HTTPCache for all responses that don't already have one.
  response_headers->RemoveAll(HttpAttributes::kEtag);

  const PanelSet* panel_set_ = &(layout_->panel_set());
  PanelIdToSpecMap panel_id_to_spec_;
  bool non_cacheable_present = BlinkUtil::ComputePanels(panel_set_,
                                                        &panel_id_to_spec_);

  // TODO(rahulbansal): Do this only if there are uncacheable panels.
  response_headers->ComputeCaching();
  response_headers->SetDateAndCaching(response_headers->date_ms(), 0,
                                      ", private, no-cache");

  base_fetch_->HeadersComplete();
  SendLayout(layout.substr(0, pos));

  if (!non_cacheable_present) {
    ServeAllPanelContents(json, panel_id_to_spec_);
  } else {
    ServeCriticalPanelContents(json, panel_id_to_spec_);
    options_->set_serve_blink_non_critical(true);
  }

  // Trigger a fetch for non cacheable panels and cookies.
  SetFilterOptions(options_);
  TriggerProxyFetch(true);
}

void BlinkFlow::ServeCriticalPanelContents(
    const Json::Value& json,
    const PanelIdToSpecMap& panel_id_to_spec) {
  GoogleString critical_json_str, non_critical_json_str, pushed_images_str;
  BlinkUtil::SplitCritical(json, panel_id_to_spec, &critical_json_str,
                           &non_critical_json_str, &pushed_images_str);
  time_to_split_critical_ms_ = GetTimeElapsedFromStartRequest();
  // TODO(rahulbansal): Add an option for storing sent_critical_data.
  SendCriticalJson(&critical_json_str);
  SendInlineImagesJson(&pushed_images_str);
}

void BlinkFlow::ServeAllPanelContents(
    const Json::Value& json,
    const PanelIdToSpecMap& panel_id_to_spec) {
  GoogleString critical_json_str, non_critical_json_str, pushed_images_str;
  BlinkUtil::SplitCritical(json, panel_id_to_spec, &critical_json_str,
                           &non_critical_json_str, &pushed_images_str);
  time_to_split_critical_ms_ = GetTimeElapsedFromStartRequest();
  SendCriticalJson(&critical_json_str);
  SendInlineImagesJson(&pushed_images_str);
  SendNonCriticalJson(&non_critical_json_str);
}

void BlinkFlow::SendLayout(const StringPiece& layout) {
  WriteString(layout);
  // TODO(rahulbansal): Not serving off a sharded domain will cause an extra
  // dns lookup.
  StaticJavascriptManager* js_manager = manager_->static_javascript_manager();
  WriteString(StrCat("<script src=\"",
                     js_manager->GetBlinkJsUrl(options_),
                     "\"></script>"));
  WriteString("<script>pagespeed.panelLoaderInit();</script>");
  WriteString(GetAddTimingScriptString(kTimeToBlinkFlowStart,
                                       time_to_start_blink_flow_ms_));
  WriteString(GetAddTimingScriptString(kTimeToJsonLookupDone,
                                       time_to_json_lookup_done_ms_));
  WriteString(StrCat("<script>pagespeed.panelLoader.addCsiTiming(\"",
                     kLayoutLoaded,
                     "\", new Date() - pagespeed.panelLoader.timeStart, ",
                     IntegerToString(layout.size()),
                     ")</script>"));
  Flush();
}

void BlinkFlow::SendCriticalJson(GoogleString* critical_json_str) {
  const char* user_ip = base_fetch_->request_headers()->Lookup1(
      HttpAttributes::kXForwardedFor);
  if (user_ip != NULL && manager_->factory()->IsDebugClient(user_ip)) {
    WriteString("<script>pagespeed.panelLoader.setRequestFromInternalIp();"
                "</script>");
  }
  WriteString(GetAddTimingScriptString(kTimeToSplitCritical,
                                       time_to_split_critical_ms_));
  WriteString("<script>pagespeed.panelLoader.loadCriticalData(");
  BlinkUtil::EscapeString(critical_json_str);
  WriteString(*critical_json_str);
  WriteString(");</script>");
  Flush();
}

void BlinkFlow::SendInlineImagesJson(GoogleString* pushed_images_str) {
  WriteString("<script>pagespeed.panelLoader.loadImagesData(");
  WriteString(*pushed_images_str);
  WriteString(");</script>");
  Flush();
}

void BlinkFlow::SendNonCriticalJson(GoogleString* non_critical_json_str) {
  WriteString("<script>pagespeed.panelLoader.bufferNonCriticalData(");
  BlinkUtil::EscapeString(non_critical_json_str);
  WriteString(*non_critical_json_str);
  WriteString(");</script>");
  Flush();
}

void BlinkFlow::WriteString(const StringPiece& str) {
  base_fetch_->Write(str, manager_->message_handler());
}

void BlinkFlow::Flush() {
  base_fetch_->Flush(manager_->message_handler());
}

void BlinkFlow::JsonCacheMiss() {
  TriggerProxyFetch(false);
}

void BlinkFlow::TriggerProxyFetch(bool json_found) {
  AsyncFetch* fetch;
  if (json_found) {
    // Remove any headers that can lead to a 304, since blink can't handle 304s.
    base_fetch_->request_headers()->RemoveAll(HttpAttributes::kIfNoneMatch);
    base_fetch_->request_headers()->RemoveAll(HttpAttributes::kIfModifiedSince);
    // Pass a new fetch into proxy fetch that inhibits HeadersComplete() on the
    // base fetch. It also doesn't attach the response headers from the base
    // fetch since headers have already been flushed out.
    fetch = new AsyncFetchWithHeadersInhibited(base_fetch_);
  } else {
    RewriteOptions* options = options_->Clone();
    SetFilterOptions(options);
    fetch = new SharedJsonFetch(base_fetch_, json_url_, manager_, options);
    num_shared_json_fetches_started_->IncBy(1);

    // TODO(nikhilmadan): We are temporarily disabling all rewriters since
    // SharedJsonFetch uses the output of ProxyFetch which may be rewritten. Fix
    // this.
    options_->ClearFilters();
  }

  // NewCustomRewriteDriver takes ownership of custom_options_.
  manager_->ComputeSignature(options_);
  RewriteDriver* driver = manager_->NewCustomRewriteDriver(options_);

  // TODO(jmarantz): pass-through the property-cache callback rather than NULL.
  factory_->StartNewProxyFetch(url_, fetch, driver, NULL);
  delete this;
}

void BlinkFlow::SetFilterOptions(RewriteOptions* options) const {
  options->DisableFilter(RewriteOptions::kHtmlWriterFilter);
  options->DisableFilter(RewriteOptions::kCombineCss);
  options->DisableFilter(RewriteOptions::kCombineJavascript);
  options->DisableFilter(RewriteOptions::kMoveCssToHead);
  options->DisableFilter(RewriteOptions::kLazyloadImages);
  // TODO(rahulbansal): ConvertMetaTags is a special case incompatible filter
  // which actually causes a SIGSEGV.
  options->DisableFilter(RewriteOptions::kConvertMetaTags);
  options->DisableFilter(RewriteOptions::kDeferJavascript);

  options->ForceEnableFilter(RewriteOptions::kComputePanelJson);
  options->ForceEnableFilter(RewriteOptions::kDisableJavascript);

  options->set_min_image_size_low_resolution_bytes(0);
  // Enable inlining for all the images in html.
  options->set_max_inlined_preview_images_index(-1);
}

int64 BlinkFlow::GetTimeElapsedFromStartRequest() {
  return manager_->timer()->NowMs() - request_start_time_ms_;
}

GoogleString BlinkFlow::GetAddTimingScriptString(
    const GoogleString& timing_str, int64 time_ms) {
  return StrCat("<script>pagespeed.panelLoader.addCsiTiming(\"", timing_str,
                "\", ", Integer64ToString(time_ms), ")</script>");
}

}  // namespace net_instaweb
