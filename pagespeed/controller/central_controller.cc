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

#include "pagespeed/controller/central_controller.h"
#include "pagespeed/controller/named_lock_schedule_rewrite_controller.h"
#include "pagespeed/controller/popularity_contest_schedule_rewrite_controller.h"
#include "pagespeed/controller/queued_expensive_operation_controller.h"
#include "pagespeed/controller/work_bound_expensive_operation_controller.h"

namespace net_instaweb {

CentralController::CentralController(
    ExpensiveOperationController* expensive_operation_controller,
    ScheduleRewriteController* schedule_rewrite_controller)
    : expensive_operation_controller_(expensive_operation_controller),
      schedule_rewrite_controller_(schedule_rewrite_controller) {
}

CentralController::~CentralController() {
}

void CentralController::InitStats(Statistics* statistics) {
  NamedLockScheduleRewriteController::InitStats(statistics);
  PopularityContestScheduleRewriteController::InitStats(statistics);
  QueuedExpensiveOperationController::InitStats(statistics);
  WorkBoundExpensiveOperationController::InitStats(statistics);
}

void CentralController::ScheduleExpensiveOperation(Function* callback) {
  expensive_operation_controller_->ScheduleExpensiveOperation(callback);
}

void CentralController::NotifyExpensiveOperationComplete() {
  expensive_operation_controller_->NotifyExpensiveOperationComplete();
}

void CentralController::ScheduleRewrite(const GoogleString& key,
                                        Function* callback) {
  schedule_rewrite_controller_->ScheduleRewrite(key, callback);
}

void CentralController::NotifyRewriteComplete(const GoogleString& key) {
  schedule_rewrite_controller_->NotifyRewriteComplete(key);
}

void CentralController::NotifyRewriteFailed(const GoogleString& key) {
  schedule_rewrite_controller_->NotifyRewriteFailed(key);
}

}  // namespace net_instaweb
