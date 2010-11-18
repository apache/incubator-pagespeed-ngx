/**
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

#include "net/instaweb/util/public/statistics_work_bound.h"

#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

StatisticsWorkBound::StatisticsWorkBound(Variable* variable, int bound)
    : variable_((bound == 0) ? NULL : variable), bound_(bound) { }
StatisticsWorkBound::~StatisticsWorkBound() { }

bool StatisticsWorkBound::TryToWork() {
  bool ok = true;
  if (variable_ != NULL) {
    // We conservatively increment, then test, and decrement on failure.  This
    // guarantees that two incrementors don't both get through when we're within
    // 1 of the bound, at the cost of occasionally rejecting them both.
    variable_->Add(1);
    ok = (variable_->Get() <= bound_);
    if (!ok) {
      variable_->Add(-1);
    }
  }
  return ok;
}

void StatisticsWorkBound::WorkComplete() {
  if (variable_ != NULL) {
    variable_->Add(-1);
  }
}


}  // namespace net_instaweb
