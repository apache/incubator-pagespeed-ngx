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

#ifndef PAGESPEED_CONTROLLER_EXPENSIVE_OPERATION_CALLBACK_H_
#define PAGESPEED_CONTROLLER_EXPENSIVE_OPERATION_CALLBACK_H_

#include "pagespeed/controller/central_controller_interface.h"
#include "pagespeed/controller/central_controller_callback.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"

// Callback classes to support ExpensiveOperation features in
// CentralControllerInterfaceAdapter.

namespace net_instaweb {

// Passed to RunImpl for implementations of ExpensiveOperationCallback.
class ExpensiveOperationContext {
 public:
  explicit ExpensiveOperationContext(CentralControllerInterface* interface);
  ~ExpensiveOperationContext();

  // Mark the expensive operation as complete. Automatically invoked at
  // destruction if not explicitly called.
  void Done();

 private:
  CentralControllerInterface* central_controller_;
};

// Implementor interface to ExpensiveOperation features in
// CentralControllerInterfaceAdapter.
class ExpensiveOperationCallback
    : public CentralControllerCallback<ExpensiveOperationContext> {
 public:
  explicit ExpensiveOperationCallback(QueuedWorkerPool::Sequence* sequence);
  virtual ~ExpensiveOperationCallback();

 private:
  virtual ExpensiveOperationContext* CreateTransactionContext(
      CentralControllerInterface* interface);

  // CentralControllerCallback interface.
  virtual void RunImpl(scoped_ptr<ExpensiveOperationContext>* context) = 0;
  virtual void CancelImpl() = 0;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_EXPENSIVE_OPERATION_CALLBACK_H_
