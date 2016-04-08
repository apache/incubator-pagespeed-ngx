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

#include "pagespeed/controller/compatible_central_controller.h"

#include "pagespeed/controller/named_lock_schedule_rewrite_controller.h"
#include "pagespeed/controller/work_bound_expensive_operation_controller.h"

namespace net_instaweb {

CompatibleCentralController::CompatibleCentralController(
    int max_expensive_operations, Statistics* statistics,
    ThreadSystem* thread_system, NamedLockManager* lock_manager)
    : InProcessCentralController(
          new WorkBoundExpensiveOperationController(
// Treat 0 as -1 (unlimited) for backward compatibility.
// See longer comment in GoogleRewriteDriverFactory::CreateCentralController().
              max_expensive_operations > 0 ? max_expensive_operations : -1,
              statistics),
          new NamedLockScheduleRewriteController(lock_manager, thread_system,
                                                 statistics)) {}

CompatibleCentralController::~CompatibleCentralController() {
}

}  // namespace net_instaweb
