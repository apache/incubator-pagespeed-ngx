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

#include "pagespeed/kernel/util/statistics_work_bound.h"

namespace net_instaweb {

const char
    WorkBoundExpensiveOperationController::kCurrentExpensiveOperations[] =
        "current-expensive-operations";

WorkBoundExpensiveOperationController::WorkBoundExpensiveOperationController(
    int max_expensive_operations, Statistics* stats)
    : work_bound_(new StatisticsWorkBound(
          stats->GetUpDownCounter(kCurrentExpensiveOperations),
          max_expensive_operations)) {}

WorkBoundExpensiveOperationController::
    ~WorkBoundExpensiveOperationController() {}

void WorkBoundExpensiveOperationController::InitStats(Statistics* statistics) {
    statistics->AddGlobalUpDownCounter(kCurrentExpensiveOperations);
}

void WorkBoundExpensiveOperationController::ScheduleExpensiveOperation(
    Function* callback) {
  if (work_bound_->TryToWork()) {
    callback->CallRun();
  } else {
    callback->CallCancel();
  }
}

void WorkBoundExpensiveOperationController::NotifyExpensiveOperationComplete() {
  work_bound_->WorkComplete();
}

}  // namespace net_instaweb
