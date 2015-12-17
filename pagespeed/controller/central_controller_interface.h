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

#ifndef PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_INTERFACE_H_
#define PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_INTERFACE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/string.h"

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

  // Runs callback at an indeterminate time in the future when the rewrite
  // denoted by key should be performed. Only one concurrent rewrite for 'key'
  // will be scheduled at once; will Cancel the callback immediately if another
  // rewrite is active for 'key' (or maybe at some point in the future if it
  // is determined that the rewrite should be skipped).
  virtual void ScheduleRewrite(const GoogleString& key, Function* callback) = 0;

  // Invoke *one* of these after you are done with your rewrite to indicate
  // success or failure. Either will relinquish the lock on the key, however
  // "Complete" will mark the key done, whereas "Failed" allows for a retry.
  // Only call "Failed" in the case where a retry might help. For example,
  // do not call it if the object in question is corrupt and cannot be parsed.
  // You should only call these if ScheduleRewrite called Run on the callback
  // above. Do not call this if the callback's Cancel method was invoked.
  virtual void NotifyRewriteComplete(const GoogleString& key) = 0;
  virtual void NotifyRewriteFailed(const GoogleString& key) = 0;

 protected:
  CentralControllerInterface() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(CentralControllerInterface);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_INTERFACE_H_
