/*
 * Copyright 2013 Google Inc.
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

// Author: richardho@google.com (Richard Ho)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CACHE_HTML_INFO_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CACHE_HTML_INFO_FINDER_H_

#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Manages the cache lifetimes of CacheHtmlInfo.
class CacheHtmlInfoFinder {
 public:
  CacheHtmlInfoFinder() { }
  virtual ~CacheHtmlInfoFinder() { }

  virtual void PropagateCacheDeletes(const GoogleString& url, int furious_id,
                                     UserAgentMatcher::DeviceType device_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheHtmlInfoFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CACHE_HTML_INFO_FINDER_H_
