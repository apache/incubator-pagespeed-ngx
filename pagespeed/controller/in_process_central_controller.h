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

#ifndef PAGESPEED_CONTROLLER_IN_PROCESS_CENTRAL_CONTROLLER_H_
#define PAGESPEED_CONTROLLER_IN_PROCESS_CENTRAL_CONTROLLER_H_

#include "pagespeed/controller/central_controller.h"
#include "pagespeed/controller/expensive_operation_callback.h"
#include "pagespeed/controller/expensive_operation_controller.h"
#include "pagespeed/controller/schedule_rewrite_callback.h"
#include "pagespeed/controller/schedule_rewrite_controller.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"

namespace net_instaweb {

// Concrete implementation of CentralController, suitable for calling directly
// by workers that run in the same process as the controller. Implemented by
// delegating to pluggable implementations of the component tasks.

class InProcessCentralController : public CentralController {
 public:
  // Takes ownership of both controllers.
  InProcessCentralController(
      ExpensiveOperationController* expensive_operation_controller,
      ScheduleRewriteController* schedule_rewrite_controller);

  virtual ~InProcessCentralController();

  void ScheduleExpensiveOperation(
      ExpensiveOperationCallback* callback) override;
  void ScheduleRewrite(ScheduleRewriteCallback* callback) override;

  static void InitStats(Statistics* stats);
  void ShutDown() override;

 private:
  scoped_ptr<ExpensiveOperationController> expensive_operation_controller_;
  scoped_ptr<ScheduleRewriteController> schedule_rewrite_controller_;

  DISALLOW_COPY_AND_ASSIGN(InProcessCentralController);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_IN_PROCESS_CENTRAL_CONTROLLER_H_
