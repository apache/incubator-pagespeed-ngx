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

#include "net/instaweb/rewriter/public/blink_critical_line_data_finder.h"

namespace net_instaweb {

const char BlinkCriticalLineDataFinder::kBlinkCohort[] = "blink";

BlinkCriticalLineDataFinder::BlinkCriticalLineDataFinder() {
}

BlinkCriticalLineDataFinder::~BlinkCriticalLineDataFinder() {
}

BlinkCriticalLineData*
BlinkCriticalLineDataFinder::ExtractBlinkCriticalLineData(
    int64 cache_time_ms, PropertyPage* page, int64 now_ms, bool diff_enabled) {
  return NULL;
  // Default interface returns NULL and derived classes can override.
}

void BlinkCriticalLineDataFinder::ComputeBlinkCriticalLineData(
    const GoogleString& computed_hash,
    const GoogleString& computed_hash_smart_diff,
    const StringPiece html_content,
    const ResponseHeaders* response_headers,
    RewriteDriver* driver) {
  // Default interface is empty and derived classes can override.
}

}  // namespace net_instaweb
