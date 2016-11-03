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

#include "pagespeed/controller/in_process_central_controller.h"

#include "pagespeed/controller/expensive_operation_callback.h"
#include "pagespeed/controller/named_lock_schedule_rewrite_controller.h"
#include "pagespeed/controller/popularity_contest_schedule_rewrite_controller.h"
#include "pagespeed/controller/queued_expensive_operation_controller.h"
#include "pagespeed/controller/schedule_rewrite_callback.h"
#include "pagespeed/controller/work_bound_expensive_operation_controller.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

namespace {

class ExpensiveOperationContextImpl : public ExpensiveOperationContext {
 public:
  ExpensiveOperationContextImpl(ExpensiveOperationController* controller,
                                ExpensiveOperationCallback* callback)
      : controller_(controller), callback_(callback) {
    // SetTransactionContext steals ownership, which means we will never outlive
    // the callback.
    callback_->SetTransactionContext(this);
    controller_->ScheduleExpensiveOperation(
        MakeFunction(this, &ExpensiveOperationContextImpl::CallRun,
                     &ExpensiveOperationContextImpl::CallCancel));
  }

  ~ExpensiveOperationContextImpl() {
    Done();
  }

  void Done() override {
    if (controller_ != nullptr) {
      controller_->NotifyExpensiveOperationComplete();
      controller_ = nullptr;
    }
  }

 private:
  void CallRun() {
    callback_->CallRun();
  }

  void CallCancel() {
    controller_ = nullptr;  // Controller denied us, so don't try to release.
    callback_->CallCancel();
  }

  ExpensiveOperationController* controller_;
  ExpensiveOperationCallback* callback_;
};

class ScheduleRewriteContextImpl : public ScheduleRewriteContext {
 public:
  ScheduleRewriteContextImpl(ScheduleRewriteController* controller,
                             ScheduleRewriteCallback* callback)
      : controller_(controller), callback_(callback), key_(callback_->key()) {
    // SetTransactionContext steals ownership, which means we will never outlive
    // the callback.
    callback_->SetTransactionContext(this);
    controller_->ScheduleRewrite(
        key_, MakeFunction(this, &ScheduleRewriteContextImpl::CallRun,
                           &ScheduleRewriteContextImpl::CallCancel));
  }

  ~ScheduleRewriteContextImpl() {
    MarkSucceeded();
  }

  void MarkSucceeded() override {
    if (controller_ != nullptr) {
      controller_->NotifyRewriteComplete(key_);
      controller_ = nullptr;
    }
  }

  void MarkFailed() override {
    if (controller_ != nullptr) {
      controller_->NotifyRewriteFailed(key_);
      controller_ = nullptr;
    }
  }

 private:
  void CallRun() {
    callback_->CallRun();
  }

  void CallCancel() {
    controller_ = nullptr;  // Controller denied us, so don't try to release.
    callback_->CallCancel();
  }

  ScheduleRewriteController* controller_;
  ScheduleRewriteCallback* callback_;
  const GoogleString key_;
};

}  // namespace

InProcessCentralController::InProcessCentralController(
    ExpensiveOperationController* expensive_operation_controller,
    ScheduleRewriteController* schedule_rewrite_controller)
    : expensive_operation_controller_(expensive_operation_controller),
      schedule_rewrite_controller_(schedule_rewrite_controller) {
}

InProcessCentralController::~InProcessCentralController() {
}

void InProcessCentralController::InitStats(Statistics* statistics) {
  NamedLockScheduleRewriteController::InitStats(statistics);
  PopularityContestScheduleRewriteController::InitStats(statistics);
  QueuedExpensiveOperationController::InitStats(statistics);
  WorkBoundExpensiveOperationController::InitStats(statistics);
}

void InProcessCentralController::ShutDown() {
  schedule_rewrite_controller_->ShutDown();
}

void InProcessCentralController::ScheduleExpensiveOperation(
    ExpensiveOperationCallback* callback) {
  // Starts the transaction and deletes itself when done.
  new ExpensiveOperationContextImpl(expensive_operation_controller_.get(),
                                    callback);
}

void InProcessCentralController::ScheduleRewrite(
    ScheduleRewriteCallback* callback) {
  // Starts the transaction and deletes itself when done.
  new ScheduleRewriteContextImpl(schedule_rewrite_controller_.get(), callback);
}

}  // namespace net_instaweb
