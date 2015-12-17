// Copyright 2015 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: cheesy@google.com (Steve Hill)

#include "pagespeed/controller/work_bound_expensive_operation_controller.h"

namespace net_instaweb {

const char
    WorkBoundExpensiveOperationController::kCurrentExpensiveOperations[] =
        "current-expensive-operations";

WorkBoundExpensiveOperationController::WorkBoundExpensiveOperationController(
    int max_expensive_operations, Statistics* stats)
    : bound_(max_expensive_operations),
      counter_(bound_ > 0 ? stats->GetUpDownCounter(kCurrentExpensiveOperations)
                          : NULL) {}

WorkBoundExpensiveOperationController::
    ~WorkBoundExpensiveOperationController() {}

void WorkBoundExpensiveOperationController::InitStats(Statistics* statistics) {
    statistics->AddGlobalUpDownCounter(kCurrentExpensiveOperations);
}

bool WorkBoundExpensiveOperationController::TryToWork() {
  bool can_work = true;
  if (counter_ != NULL) {
    // We conservatively increment, then test, and decrement on failure.  This
    // guarantees that two incrementors don't both get through when we're within
    // 1 of the bound, at the cost of occasionally rejecting them both.
    // TODO(cheesy): If Statistics ever improves its atomicity gurantees, we
    // should just use the value returned by Add().
    counter_->Add(1);
    can_work = (counter_->Get() <= bound_);
    if (!can_work) {
      counter_->Add(-1);
    }
  }
  return can_work;
}

void WorkBoundExpensiveOperationController::ScheduleExpensiveOperation(
    Function* callback) {
  if (TryToWork()) {
    callback->CallRun();
  } else {
    callback->CallCancel();
  }
}

void WorkBoundExpensiveOperationController::NotifyExpensiveOperationComplete() {
  if (counter_ != NULL) {
    counter_->Add(-1);
  }
}

}  // namespace net_instaweb
