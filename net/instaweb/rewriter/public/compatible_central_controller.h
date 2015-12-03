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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_COMPATIBLE_CENTRAL_CONTROLLER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_COMPATIBLE_CENTRAL_CONTROLLER_H_

#include "net/instaweb/rewriter/public/central_controller_interface.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/util/work_bound.h"

namespace net_instaweb {

// Implements CentralControllerInterface, delegating to implementations that
// match pre-CentralControllerInterface code.
class CompatibleCentralController : public CentralControllerInterface {
 public:
  static const char kCurrentExpensiveOperations[];

  CompatibleCentralController(int max_expensive_operations, Statistics* stats);

  virtual ~CompatibleCentralController();

  static void InitStats(Statistics* stats);

  virtual void ScheduleExpensiveOperation(Function* callback);
  virtual void NotifyExpensiveOperationComplete();

 private:
  scoped_ptr<WorkBound> work_bound_;

  DISALLOW_COPY_AND_ASSIGN(CompatibleCentralController);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COMPATIBLE_CENTRAL_CONTROLLER_H_
