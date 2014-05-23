/*
 * Copyright 2010 Google Inc.
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

// Author: jmaessen@google.com (Jan Maessen)

#include "pagespeed/kernel/util/statistics_work_bound.h"

#include <cstddef>
#include "pagespeed/kernel/base/statistics.h"

namespace net_instaweb {

StatisticsWorkBound::StatisticsWorkBound(UpDownCounter* counter, int bound)
    : counter_((bound == 0) ? NULL : counter), bound_(bound) { }
StatisticsWorkBound::~StatisticsWorkBound() { }

bool StatisticsWorkBound::TryToWork() {
  bool ok = true;
  if (counter_ != NULL) {
    // We conservatively increment, then test, and decrement on failure.  This
    // guarantees that two incrementors don't both get through when we're within
    // 1 of the bound, at the cost of occasionally rejecting them both.
    counter_->Add(1);
    ok = (counter_->Get() <= bound_);
    if (!ok) {
      counter_->Add(-1);
    }
  }
  return ok;
}

void StatisticsWorkBound::WorkComplete() {
  if (counter_ != NULL) {
    counter_->Add(-1);
  }
}


}  // namespace net_instaweb
