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

#include "pagespeed/controller/central_controller_interface.h"
#include "pagespeed/controller/expensive_operation_controller.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

// Concrete implementation of CentralControllerInterface, suitable for calling
// directly by workers that run in the same process as the controller.
// Implements CentrolControllerInterface by delegating to pluggable
// implementations of the component tasks.

class CentralController : public CentralControllerInterface {
 public:
  CentralController(
      ExpensiveOperationController* expensive_operation_controller);

  virtual ~CentralController();

  // CentralControllerInterface for expensive operations.
  // All just delegated to expensive_operation_controller_.
  virtual void ScheduleExpensiveOperation(Function* callback);
  virtual void NotifyExpensiveOperationComplete();

  // TODO(cheesy): There will be a different delegate for the popularity
  // contest in here.

 private:
  scoped_ptr<ExpensiveOperationController> expensive_operation_controller_;

  DISALLOW_COPY_AND_ASSIGN(CentralController);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_H_
