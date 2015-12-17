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

#ifndef PAGESPEED_CONTROLLER_WORK_BOUND_EXPENSIVE_OPERATION_CONTROLLER_H_
#define PAGESPEED_CONTROLLER_WORK_BOUND_EXPENSIVE_OPERATION_CONTROLLER_H_

#include "pagespeed/controller/expensive_operation_controller.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/statistics.h"

namespace net_instaweb {

// Implements ExpensiveOperationController by using a statistic to limit
// multiple simultaneous expensive jobs. Named after now removed WorkBound
// class. This uses Statistics to communicate between multiple worker
// processes so does not have have the cross-process constraints of
// QueuedExpensiveOperationController. However, this implementation does not
// queue requests, instead observing the count of in-progress operations and
// canceling the request if that number is too great.
class WorkBoundExpensiveOperationController
    : public ExpensiveOperationController {
 public:
  static const char kCurrentExpensiveOperations[];

  WorkBoundExpensiveOperationController(int max_expensive_operations,
                                        Statistics* stats);
  virtual ~WorkBoundExpensiveOperationController();

  // ExpensiveOperationController interface.
  virtual void ScheduleExpensiveOperation(Function* callback);
  virtual void NotifyExpensiveOperationComplete();

  static void InitStats(Statistics* stats);

 private:
  bool TryToWork();

  const int bound_;
  UpDownCounter* counter_;

  DISALLOW_COPY_AND_ASSIGN(WorkBoundExpensiveOperationController);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_WORK_BOUND_EXPENSIVE_OPERATION_CONTROLLER_H_
