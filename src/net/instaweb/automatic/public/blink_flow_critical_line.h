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
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AsyncFetch;
class BlinkCriticalLineData;
class BlinkCriticalLineDataFinder;
class ProxyFetchPropertyCallbackCollector;
class ProxyFetchFactory;
class ResourceManager;
class RewriteOptions;

// This class manages the blink flow for looking up BlinkCriticalLineData in
// cache, modifying the options for passthru and triggering asynchronous
// lookups to compute the critical line and insert it into cache.
class BlinkFlowCriticalLine {
 public:
  static void Start(const GoogleString& url,
                    AsyncFetch* base_fetch,
                    RewriteOptions* options,
                    ProxyFetchFactory* factory,
                    ResourceManager* manager,
                    ProxyFetchPropertyCallbackCollector* property_callback);

  virtual ~BlinkFlowCriticalLine();

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

  void TriggerProxyFetch(bool critical_line_data_found);

  // Modify the rewrite options before serving to client if
  // BlinkCriticalLineData is not available in cache. Options being modified
  // will be used in the background and not in the user-facing request.
  void SetFilterOptions(RewriteOptions* options) const;

  GoogleString url_;
  AsyncFetch* base_fetch_;
  RewriteOptions* options_;
  ProxyFetchFactory* factory_;
  ResourceManager* manager_;
  ProxyFetchPropertyCallbackCollector* property_callback_;
  scoped_ptr<BlinkCriticalLineData> blink_critical_line_data_;
  BlinkCriticalLineDataFinder* finder_;

  DISALLOW_COPY_AND_ASSIGN(BlinkFlowCriticalLine);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_BLINK_FLOW_CRITICAL_LINE_H_
