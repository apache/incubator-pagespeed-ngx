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

// Author: rahulbansal@google.com (Rahul Bansal)
//
// This class manages the flow of a blink request. In order to flush the
// critical html early before we start getting bytes back from the
// fetcher, we lookup property cache for BlinkCriticalLineData.
// If found, we flush critical html out and then trigger the normal
// ProxyFetch flow with customized options which extracts cookies and
// non cacheable panels from the page and sends it out.
// If BlinkCriticalLineData is not found in cache, we pass this request through
// normal ProxyFetch flow buffering the html. In the background we
// create a driver to parse it, run it through other filters, compute
// BlinkCriticalLineData and store it into the property cache.

#include "net/instaweb/automatic/public/blink_flow_critical_line.h"

namespace net_instaweb {

void BlinkFlowCriticalLine::Start(const GoogleString& url,
                                  AsyncFetch* base_fetch,
                                  RewriteOptions* options,
                                  ProxyFetchFactory* factory,
                                  ResourceManager* manager) {
}

BlinkFlowCriticalLine::BlinkFlowCriticalLine(const GoogleString& url,
                                             AsyncFetch* base_fetch,
                                             RewriteOptions* options,
                                             ProxyFetchFactory* factory,
                                             ResourceManager* manager) {
}

BlinkFlowCriticalLine::~BlinkFlowCriticalLine() {}

}  // namespace net_instaweb
