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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_INTERFACE_ADAPTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_INTERFACE_ADAPTER_H_

#include "net/instaweb/rewriter/public/central_controller_interface.h"
#include "net/instaweb/rewriter/public/central_controller_callback.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"

namespace net_instaweb {

// Adapt CentralControllerInterface onto a more programmer-friendly API.

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

class CentralControllerInterfaceAdapter {
 public:
  // Takes ownership of interface.
  explicit CentralControllerInterfaceAdapter(
      CentralControllerInterface* interface);
  virtual ~CentralControllerInterfaceAdapter();

  // Runs callback at an indeterminate time in the future when it is safe
  // to perform a CPU intensive operation. Or may Cancel the callback at some
  // point if it is determined that the work cannot be performed.
  virtual void ScheduleExpensiveOperation(ExpensiveOperationCallback* callback);

 private:
  scoped_ptr<CentralControllerInterface> central_controller_;

  DISALLOW_COPY_AND_ASSIGN(CentralControllerInterfaceAdapter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_INTERFACE_ADAPTER_H_
