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

// Author: mmohabey@google.com (Megha Mohabey)

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_EARLY_INFO_FINDER_TEST_BASE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_EARLY_INFO_FINDER_TEST_BASE_H_

#include "net/instaweb/rewriter/public/flush_early_info_finder.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

// By default, FlushEarlyInfoFinder does not return meaningful results. This
// class can be used by tests which manually manage FlushEarlyRenderInfo.
class MeaningfulFlushEarlyInfoFinder : public FlushEarlyInfoFinder {
 public:
  MeaningfulFlushEarlyInfoFinder() {}
  virtual ~MeaningfulFlushEarlyInfoFinder() {}
  virtual bool IsMeaningful() const {
    return true;
  }
  virtual const char* GetCohort() const {
    return "NullCohort";
  }
  virtual int64 cache_expiration_time_ms() const {
    return Timer::kHourMs;
  }
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FLUSH_EARLY_INFO_FINDER_TEST_BASE_H_
