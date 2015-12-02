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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_INTERFACE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_INTERFACE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"

namespace net_instaweb {

// Abstract interface class that supports various PSOL operations which should
// be performed in a centralized fashion, instead of once per worker process.

class CentralControllerInterface {
 public:
  virtual ~CentralControllerInterface() { }

  // Runs callback at an indeterminate time in the future when it is safe
  // to perform a CPU intensive operation. Or may Cancel the callback at some
  // point if it is determined that the work cannot be performed.
  virtual void ScheduleExpensiveOperation(Function* callback) = 0;

  // Invoke after performing your expensive operation to relinquish the
  // resource. You should only call this if ScheduleExpensiveOperation
  // called Run on the callback above. Do not call this if the callback's
  // Cancel method was invoked.
  virtual void NotifyExpensiveOperationComplete() = 0;

 protected:
  CentralControllerInterface() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(CentralControllerInterface);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_INTERFACE_H_
