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

// Author: pulkitg@google.com (Pulkit Goyal)

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_BLINK_FLOW_CRITICAL_LINE_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_BLINK_FLOW_CRITICAL_LINE_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AsyncFetch;
class BlinkCriticalLineData;
class BlinkCriticalLineDataFinder;
class BlinkInfo;
class PropertyPage;
class ProxyFetchPropertyCallbackCollector;
class ProxyFetchFactory;
class ResourceManager;
class RewriteOptions;
class Statistics;
class TimedVariable;

// This class manages the blink flow for looking up BlinkCriticalLineData in
// cache, modifying the options for passthru and triggering asynchronous
// lookups to compute the critical line and insert it into cache.
class BlinkFlowCriticalLine {
 public:
  // These strings identify sync-points for reproducing races between foreground
  // serving request and background blink computation requests in tests.
  static const char kBackgroundComputationDone[];

  static void Start(const GoogleString& url,
                    AsyncFetch* base_fetch,
                    RewriteOptions* options,
                    ProxyFetchFactory* factory,
                    ResourceManager* manager,
                    ProxyFetchPropertyCallbackCollector* property_callback);

  virtual ~BlinkFlowCriticalLine();

  static void Initialize(Statistics* statistics);

  static const char kAboveTheFold[];
  static const char kNumBlinkHtmlCacheHits[];
  static const char kNumBlinkHtmlCacheMisses[];
  static const char kNumBlinkSharedFetchesStarted[];
  static const char kNumBlinkSharedFetchesCompleted[];
  static const char kNumComputeBlinkCriticalLineDataCalls[];

 private:
  BlinkFlowCriticalLine(const GoogleString& url,
                        AsyncFetch* base_fetch,
                        RewriteOptions* options,
                        ProxyFetchFactory* factory,
                        ResourceManager* manager,
                        ProxyFetchPropertyCallbackCollector* property_callback);

  // Function called by the callback collector whenever property cache lookup
  // is done. Based on the result, it will call either
  // BlinkCriticalLineDataHit() or BlinkCriticalLineDataMiss().
  void BlinkCriticalLineDataLookupDone(
      ProxyFetchPropertyCallbackCollector* collector);

  // Serves the critical html content to the client and triggers the proxy fetch
  // for non cacheable content.
  void BlinkCriticalLineDataHit();

  // Serves the request in passthru mode and triggers a background request to
  // compute BlinkCriticalLineData.
  void BlinkCriticalLineDataMiss();

  void TriggerProxyFetch(bool critical_line_data_found);

  // Serves all the panel contents including critical html, critical images json
  // and non critical json. This is the case when there are no cacheable panels
  // in the page.
  void ServeAllPanelContents();

  // Serves critical panel contents including critical html and
  // critical images json. This is the case when there are cacheable panels
  // in the page.
  void ServeCriticalPanelContents();

  // Sends critical html to the client.
  void SendCriticalHtml(const GoogleString& critical_json_str);

  // Sends inline images json to the client.
  void SendInlineImagesJson(const GoogleString& pushed_images_str);

  // Sends non critical json to the client.
  void SendNonCriticalJson(GoogleString* non_critical_json_str);

  void WriteString(const StringPiece& str);

  void Flush();

  // Modify the rewrite options to be used in the background and user-facing
  // request when BlinkCriticalLineData is found in the cache.
  void SetFilterOptions(RewriteOptions* options) const;

  // Returns true if property cache has last response code as non 200.
  bool IsLastResponseCodeInvalid(PropertyPage* page);

  GoogleString url_;
  GoogleUrl google_url_;
  GoogleString critical_html_;
  AsyncFetch* base_fetch_;
  BlinkInfo* blink_info_;
  RewriteOptions* options_;
  ProxyFetchFactory* factory_;
  ResourceManager* manager_;
  ProxyFetchPropertyCallbackCollector* property_callback_;
  scoped_ptr<BlinkCriticalLineData> blink_critical_line_data_;
  BlinkCriticalLineDataFinder* finder_;

  TimedVariable* num_blink_html_cache_hits_;
  TimedVariable* num_blink_shared_fetches_started_;

  DISALLOW_COPY_AND_ASSIGN(BlinkFlowCriticalLine);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_BLINK_FLOW_CRITICAL_LINE_H_
