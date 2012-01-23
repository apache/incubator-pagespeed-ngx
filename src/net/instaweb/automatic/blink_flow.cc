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
// early before we start getting bytes back from the fetcher, we trigger a cache
// lookup for the json.
// If the json is found, we flush json out and then trigger the normal
// ProxyFetch flow with customized options which extracts json from the page and
// sends it out.
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
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/panel_config.pb.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/google_url.h"
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

// AsyncFetch that fetches the sample page, initiates a new RewriteDriver to
// compute the json and stores it in cache.
// TODO(rahulbansal): Buffer the html chunked rather than in one string.
class JsonFetch : public StringAsyncFetch {
 public:
  JsonFetch(const GoogleString& key,
            ResourceManager* resource_manager,
            RewriteOptions* options)
      : key_(key),
        resource_manager_(resource_manager),
        options_(options) {}

  virtual ~JsonFetch() {}

  virtual void HandleDone(bool success) {
    if (!success || response_headers()->status_code() != HttpStatus::kOK) {
      // Do nothing since the fetch failed.
      LOG(INFO) << "Background fetch for layout url " << key_ << " failed.";
      delete this;
      return;
    }

    const ContentType* type = response_headers()->DetermineContentType();
    if (type == NULL || !type->IsHtmlLike()) {
      LOG(INFO) << "Non html page, not rewritable: " << key_;
      delete this;
      return;
    }

    json_computation_driver_ = resource_manager_->NewCustomRewriteDriver(
        options_.release());
    // Set deadline to 10s since we want maximum filters to complete.
    // Note that no client is blocked waiting for this request to complete.
    json_computation_driver_->set_rewrite_deadline_ms(
        10 * Timer::kSecondMs);
    json_computation_driver_->SetWriter(&value_);
    json_computation_driver_->set_response_headers_ptr(response_headers());
    json_computation_driver_->AddLowPriorityRewriteTask(
        MakeFunction(this, &JsonFetch::Parse));
  }

  void Parse() {
    json_computation_driver_->StartParse(
        key_.substr(kJsonCachePrefixLength));
    json_computation_driver_->ParseText(buffer());

    // Clean up.
    json_computation_driver_->FinishParseAsync(MakeFunction(
        this, &JsonFetch::CompleteFinishParse));
  }

  void CompleteFinishParse() {
    delete this;
  }

 private:
  GoogleString key_;
  ResourceManager* resource_manager_;
  scoped_ptr<RewriteOptions> options_;
  HTTPValue value_;

  RewriteDriver* json_computation_driver_;

  DISALLOW_COPY_AND_ASSIGN(JsonFetch);
};

}  // namespace

class BlinkFlow::JsonFindCallback : public HTTPCache::Callback {
 public:
  explicit JsonFindCallback(BlinkFlow* blink_fetch)
      : blink_fetch_(blink_fetch) {}

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

  virtual bool IsCacheValid(const ResponseHeaders& headers) {
    return true;
  }

 private:
  BlinkFlow* blink_fetch_;
  DISALLOW_COPY_AND_ASSIGN(JsonFindCallback);
};

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

void BlinkFlow::InitiateJsonLookup() {
  // TODO(rahulbansal): Remove start_time_ms from rewrite_driver.
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
    LOG(DFATAL) << "Couldn't parse Json From Cache: " << json_str;
    JsonCacheMiss();
    return;
  }

  StringPiece layout = (json)[0][BlinkUtil::kInstanceHtml].asCString();
  // NOTE: Since we compute layout in background and only get it in serialized
  // form, we have to strip everything after the layout marker.
  size_t pos = layout.find(BlinkUtil::kLayoutMarker);
  if (pos == StringPiece::npos) {
    LOG(DFATAL) << "Layout marker not found: " << layout;
    JsonCacheMiss();
    return;
  }

  ResponseHeaders* response_headers = base_fetch_->response_headers();
  response_headers->CopyFrom(headers);
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
  options_->EnableFilter(RewriteOptions::kComputePanelJson);
  options_->DisableFilter(RewriteOptions::kHtmlWriterFilter);
  options_->EnableFilter(RewriteOptions::kDisableJavascript);
  options_->EnableFilter(RewriteOptions::kInlineImages);
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
  // TODO(rahulbansal): We can't send cookies here since the fetch hasn't
  // yet returned. This needs to be fixed so that even for a case when
  // everything is cached, we do send the cookies when the fetch returns.
  SendNonCriticalJson(&non_critical_json_str);
}

void BlinkFlow::SendLayout(const StringPiece& layout) {
  WriteString(layout);
  // TODO(rahulbansal): Not serving off a sharded domain will cause an extra
  // dns lookup.
  WriteString(StrCat("<script src=\"",
                     manager_->url_namer()->get_proxy_domain(),
                     "/webinstant/blink.js\"></script>"));
  WriteString("<script>pagespeed.panelLoaderInit();</script>");
  WriteString(GetAddTimingScriptString(kTimeToBlinkFlowStart,
                                       time_to_start_blink_flow_ms_));
  WriteString(GetAddTimingScriptString(kTimeToJsonLookupDone,
                                       time_to_json_lookup_done_ms_));
  WriteString(StrCat("<script>pagespeed.panelLoader.addCsiTiming(\"", kLayoutLoaded,
                     "\", new Date() - pagespeed.panelLoader.timeStart, ",
                     IntegerToString(layout.size()),
                     ")</script>"));
  Flush();
}

void BlinkFlow::SendCriticalJson(GoogleString* critical_json_str) {
  // TODO(rahulbansal): Remove user_ip from rewrite_driver.
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
  ComputeJsonInBackground();
  TriggerProxyFetch(false);
}

void BlinkFlow::TriggerProxyFetch(bool json_found) {
  AsyncFetch* fetch = base_fetch_;
  if (json_found) {
    // Remove any headers that can lead to a 304, since blink can't handle 304s.
    base_fetch_->request_headers()->RemoveAll(HttpAttributes::kIfNoneMatch);
    base_fetch_->request_headers()->RemoveAll(HttpAttributes::kIfModifiedSince);
    // Pass a new fetch into proxy fetch that inhibits HeadersComplete() on the
    // base fetch. It also doesn't attach the response headers from the base
    // fetch since headers have already been flushed out.
    fetch = new AsyncFetchWithHeadersInhibited(base_fetch_);
  }
  factory_->StartNewProxyFetch(url_, fetch, options_);
  delete this;
}

void BlinkFlow::ComputeJsonInBackground() {
  RewriteOptions* options = options_->Clone();
  options->EnableFilter(RewriteOptions::kComputePanelJson);
  options->EnableFilter(RewriteOptions::kDisableJavascript);
  options->DisableFilter(RewriteOptions::kHtmlWriterFilter);

  JsonFetch* json_fetch = new JsonFetch(json_url_, manager_, options);

  // TODO(rahulbansal): We can use the output of AsyncFetchWithHeadersInhibited
  // instead of triggering a new background fetch.
  manager_->url_async_fetcher()->Fetch(
      json_url_.substr(kJsonCachePrefixLength),
      manager_->message_handler(),
      json_fetch);
}

int64 BlinkFlow::GetTimeElapsedFromStartRequest() {
  return manager_->timer()->NowMs() - request_start_time_ms_;
}

GoogleString BlinkFlow::GetAddTimingScriptString(
    const GoogleString& timing_str, int64 time_ms) {
  return StrCat("<script>pagespeed.panelLoader.addCsiTiming(\"", timing_str, "\", ",
                Integer64ToString(time_ms), ")</script>");
}

}  // namespace net_instaweb
