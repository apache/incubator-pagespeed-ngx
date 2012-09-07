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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_BLINK_CRITICAL_LINE_DATA_FINDER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_BLINK_CRITICAL_LINE_DATA_FINDER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class BlinkCriticalLineData;
class PropertyPage;
class ResponseHeaders;
class RewriteDriver;

// Finds BlinkCriticalLineData from the given html content. This information
// will be used by BlinkFlowCriticalLine.
// TODO(pulkitg): Rethink about the naming and structure of this class.
class BlinkCriticalLineDataFinder {
 public:
  static const char kBlinkCohort[];
  BlinkCriticalLineDataFinder();
  virtual ~BlinkCriticalLineDataFinder();

  // Gets BlinkCriticalLineData from the given PropertyPage.
  virtual BlinkCriticalLineData* ExtractBlinkCriticalLineData(
      int64 cache_time_ms, PropertyPage* page, int64 now_ms, bool diff_enabled);

  // Computes BlinkCriticalLineData for the given html content.
  virtual void ComputeBlinkCriticalLineData(
      const GoogleString& computed_hash,
      const GoogleString& computed_hash_smart_diff,
      const StringPiece html_content,
      const ResponseHeaders* response_headers,
      RewriteDriver* driver);

 private:
  DISALLOW_COPY_AND_ASSIGN(BlinkCriticalLineDataFinder);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_BLINK_CRITICAL_LINE_DATA_FINDER_H_
