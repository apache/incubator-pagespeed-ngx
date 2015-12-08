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

#ifndef PAGESPEED_CONTROLLER_EXPENSIVE_OPERATION_CONTROLLER_H_
#define PAGESPEED_CONTROLLER_EXPENSIVE_OPERATION_CONTROLLER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"

namespace net_instaweb {

// Abstract interface class that supports PSOL operations for rate-limiting
// CPU intensive operations. For use in CentralController.

class ExpensiveOperationController {
 public:
  virtual ~ExpensiveOperationController() { }

  // Run callback at an indeterminate time in the future when it is safe
  // to perform a CPU intensive operation. May Cancel the callback at some
  // point if it is determined that the work cannot be performed.
  virtual void ScheduleExpensiveOperation(Function* callback) = 0;

  // Inform controller that the operation has been completed.
  // Should only be called if Run() was invoked on callback above.
  virtual void NotifyExpensiveOperationComplete() = 0;

 protected:
  ExpensiveOperationController() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExpensiveOperationController);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_EXPENSIVE_OPERATION_CONTROLLER_H_
