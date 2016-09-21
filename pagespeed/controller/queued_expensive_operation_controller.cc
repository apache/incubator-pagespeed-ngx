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

#include "pagespeed/controller/queued_expensive_operation_controller.h"

#include "base/logging.h"

namespace net_instaweb {

const char QueuedExpensiveOperationController::kActiveExpensiveOperations[] =
    "active-expensive-operations";
const char QueuedExpensiveOperationController::kQueuedExpensiveOperations[] =
    "queued-expensive-operations";
const char QueuedExpensiveOperationController::kPermittedExpensiveOperations[] =
    "permitted-expensive-operations";

QueuedExpensiveOperationController::QueuedExpensiveOperationController(
    int max_expensive_operations, ThreadSystem* thread_system,
    Statistics* stats)
    : max_in_progress_(max_expensive_operations),
      num_in_progress_(0),
      mutex_(thread_system->NewMutex()),
      active_operations_counter_(
          stats->GetUpDownCounter(kActiveExpensiveOperations)),
      queued_operations_counter_(
          stats->GetUpDownCounter(kQueuedExpensiveOperations)),
      permitted_operations_counter_(
          stats->GetTimedVariable(kPermittedExpensiveOperations)) {
}

QueuedExpensiveOperationController::~QueuedExpensiveOperationController() {
  // queue_ is *supposed* to be empty at this point. In case it's not, we
  // should make sure it gets cleaned up to avoid a leak. Given that we expect
  // the controller will be deleted after everything else has shutdown, running
  // Cancel is a bit scary. Instead we just delete the contents of the queue.
  // Note that Function DCHECKS that it was run upon deletion, so the DCHECK
  // below would have fired in the loop, regardless.
  DCHECK(queue_.empty());

  while (!queue_.empty()) {
    delete queue_.front();
    queue_.pop();
  }
}

void QueuedExpensiveOperationController::InitStats(Statistics* statistics) {
  statistics->AddGlobalUpDownCounter(kActiveExpensiveOperations);
  statistics->AddGlobalUpDownCounter(kQueuedExpensiveOperations);
  statistics->AddTimedVariable(kPermittedExpensiveOperations,
                               Statistics::kDefaultGroup);
}

void QueuedExpensiveOperationController::ScheduleExpensiveOperation(
    Function* callback) {
  ScopedMutex lock(mutex_.get());
  CHECK(callback != NULL);

  // If we are configured to disallow all expensive operations, immediately deny
  // the request and don't queue it.
  if (max_in_progress_ == 0) {
    lock.Release();
    callback->CallCancel();
    return;
  }

  // If we have a spare slot, run the callback immediately.
  if (max_in_progress_ < 0 || num_in_progress_ < max_in_progress_) {
    IncrementInProgress();
    lock.Release();
    callback->CallRun();
    return;
  } else {
    // No slot, so enqueue the callback for later.
    Enqueue(callback);
  }
}

void QueuedExpensiveOperationController::NotifyExpensiveOperationComplete() {
  ScopedMutex lock(mutex_.get());
  DecrementInProgress();

  // We should now have a slot available. If there's something on the queue,
  // run it.
  if (max_in_progress_ > 0) {
    CHECK_LT(num_in_progress_, max_in_progress_);
  }
  Function* callback = Dequeue();
  if (callback != NULL) {
    IncrementInProgress();
    lock.Release();
    callback->CallRun();
    return;
  }
}

void QueuedExpensiveOperationController::Enqueue(Function* callback) {
  queue_.push(callback);
  queued_operations_counter_->Set(queue_.size());
}

Function* QueuedExpensiveOperationController::Dequeue() {
  Function* result = NULL;
  if (!queue_.empty()) {
    result = queue_.front();
    queue_.pop();
    queued_operations_counter_->Set(queue_.size());
  }
  return result;
}

void QueuedExpensiveOperationController::IncrementInProgress() {
  ++num_in_progress_;
  active_operations_counter_->Set(num_in_progress_);
  permitted_operations_counter_->IncBy(1);
}

void QueuedExpensiveOperationController::DecrementInProgress() {
  DCHECK_GT(num_in_progress_, 0);
  if (num_in_progress_ > 0) {
    --num_in_progress_;
    active_operations_counter_->Set(num_in_progress_);
  }
}

}  // namespace net_instaweb
