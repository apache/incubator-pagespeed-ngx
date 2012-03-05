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

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_BLINK_FLOW_CRITICAL_LINE_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_BLINK_FLOW_CRITICAL_LINE_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

class AsyncFetch;
class RewriteOptions;
class ProxyFetchFactory;
class ResourceManager;

// This class manages the blink flow for looking up BlinkCriticalLineData in
// cache, modifying the options for passthru and triggering asynchronous
// lookups to compute the critical line and insert it into cache.
class BlinkFlowCriticalLine {
 public:
  static void Start(const GoogleString& url,
                    AsyncFetch* base_fetch,
                    RewriteOptions* options,
                    ProxyFetchFactory* factory,
                    ResourceManager* manager);

  virtual ~BlinkFlowCriticalLine();

 private:
  BlinkFlowCriticalLine(const GoogleString& url,
                        AsyncFetch* base_fetch,
                        RewriteOptions* options,
                        ProxyFetchFactory* factory,
                        ResourceManager* manager);

  DISALLOW_COPY_AND_ASSIGN(BlinkFlowCriticalLine);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_BLINK_FLOW_CRITICAL_LINE_H_
