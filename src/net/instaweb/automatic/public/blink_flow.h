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

#ifndef NET_INSTAWEB_AUTOMATIC_PUBLIC_BLINK_FLOW_H_
#define NET_INSTAWEB_AUTOMATIC_PUBLIC_BLINK_FLOW_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AsyncFetch;
class Layout;
class ProxyFetchFactory;
class ResourceManager;
class ResponseHeaders;
class RewriteOptions;

// This class manages the blink flow for looking up the layout in cache,
// modifying the options for passthru and triggering asynchronous lookups to
// compute the layout and insert it into cache.
class BlinkFlow {
 public:
  static void Start(const GoogleString& url,
                    AsyncFetch* base_fetch,
                    const Layout* layout,
                    RewriteOptions* options,
                    ProxyFetchFactory* factory,
                    ResourceManager* manager);

  virtual ~BlinkFlow();

 private:
  class LayoutFindCallback;
  friend class LayoutFindCallback;

  BlinkFlow(const GoogleString& url,
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
        manager_(manager) {}

  void InitiateLayoutLookup();

  void LayoutCacheHit(const StringPiece& content,
                      const ResponseHeaders& headers);

  void LayoutCacheMiss();

  void TriggerProxyFetch(bool layout_found);

  void ComputeLayoutInBackground();

  GoogleString url_;
  AsyncFetch* base_fetch_;
  const Layout* layout_;
  RewriteOptions* options_;
  ProxyFetchFactory* factory_;
  ResourceManager* manager_;
  GoogleString layout_url_;

  DISALLOW_COPY_AND_ASSIGN(BlinkFlow);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_AUTOMATIC_PUBLIC_BLINK_FLOW_H_
