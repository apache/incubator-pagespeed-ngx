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

#ifndef PAGESPEED_CONTROLLER_QUEUED_EXPENSIVE_OPERATION_CONTROLLER_H_
#define PAGESPEED_CONTROLLER_QUEUED_EXPENSIVE_OPERATION_CONTROLLER_H_

#include <queue>

#include "pagespeed/controller/expensive_operation_controller.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// Implements ExpensiveOperationController using a counter and a queue to
// limit operations in strict order. Note that this implementation does not
// communicate across process boundaries; It assumes that requests from all
// workers will be routed to it either by virtue of running in a single
// process/multi-threaded environment, or through an external RPC system.
// See WorkBoundExpensiveOperationController for an alternate implementation
// that does not have this limitation.
class QueuedExpensiveOperationController
    : public ExpensiveOperationController {
 public:
  static const char kActiveExpensiveOperations[];
  static const char kQueuedExpensiveOperations[];
  static const char kPermittedExpensiveOperations[];

  QueuedExpensiveOperationController(int max_expensive_operations,
                                     ThreadSystem* thread_system,
                                     Statistics* stats);
  virtual ~QueuedExpensiveOperationController();

  // ExpensiveOperationController interface.
  virtual void ScheduleExpensiveOperation(Function* callback);
  virtual void NotifyExpensiveOperationComplete();

  static void InitStats(Statistics* stats);

 private:
  void IncrementInProgress() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void DecrementInProgress() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void Enqueue(Function* function) EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Function* Dequeue() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  const int max_in_progress_;
  std::queue<Function*> queue_ GUARDED_BY(mutex_);
  int num_in_progress_ GUARDED_BY(mutex_);
  scoped_ptr<AbstractMutex> mutex_;
  UpDownCounter* active_operations_counter_;
  UpDownCounter* queued_operations_counter_;
  TimedVariable* permitted_operations_counter_;

  DISALLOW_COPY_AND_ASSIGN(QueuedExpensiveOperationController);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_QUEUED_EXPENSIVE_OPERATION_CONTROLLER_H_
