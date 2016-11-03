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

#ifndef PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_H_
#define PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_H_

#include "pagespeed/controller/expensive_operation_callback.h"
#include "pagespeed/controller/schedule_rewrite_callback.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

// Abstract interface class that supports various PSOL operations which should
// be performed in a centralized fashion, instead of once per worker process.

class CentralController {
 public:
  virtual ~CentralController();

  // Runs callback at an indeterminate time in the future when it is safe
  // to perform a CPU intensive operation. Or may Cancel the callback at some
  // point if it is determined that the work cannot be performed.
  virtual void ScheduleExpensiveOperation(
      ExpensiveOperationCallback* callback) = 0;

  // Runs callback at an indeterminate time in the future when the associated
  // rewrite should be performed. May Cancel the callback immediately or at
  // some point in the future if the rewrite should not be performed by the
  // caller. Only one rewrite per callback.key() will be scheduled at once.
  virtual void ScheduleRewrite(ScheduleRewriteCallback* callback) = 0;

  // Implementations of this method should try to cancel any pending operations
  // ASAP, and immediately reject new incoming ones. This method should behave
  // safely when called more than once.
  virtual void ShutDown() = 0;

 protected:
  CentralController();

 private:
  DISALLOW_COPY_AND_ASSIGN(CentralController);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_H_
