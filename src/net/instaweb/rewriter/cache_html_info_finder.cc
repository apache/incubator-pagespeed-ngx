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

#include "net/instaweb/rewriter/public/cache_html_info_finder.h"

namespace net_instaweb {

void CacheHtmlInfoFinder::PropagateCacheDeletes(
    const GoogleString& url, int experiment_id,
    UserAgentMatcher::DeviceType device_type) {
  // Default interface is empty and derived classes can override.
}

bool CacheHtmlInfoFinder::UpdateDiffInfo(
    bool is_diff, int64 now_ms, AbstractLogRecord* cache_html_log_record,
    RewriteDriver* rewrite_driver, RewriteDriverFactory* factory) {
  // Default interface is empty and derived classes can override.
  return false;
}

}  // namespace net_instaweb
