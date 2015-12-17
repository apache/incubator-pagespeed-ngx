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

#include "pagespeed/controller/schedule_rewrite_callback.h"

namespace net_instaweb {

ScheduleRewriteContext::ScheduleRewriteContext(
    CentralControllerInterface* central_controller, const GoogleString& key)
    : central_controller_(central_controller), key_(key) {
}

ScheduleRewriteContext::~ScheduleRewriteContext() {
  MarkSucceeded();
}

void ScheduleRewriteContext::MarkSucceeded() {
  if (central_controller_ != NULL) {
    central_controller_->NotifyRewriteComplete(key_);
    central_controller_ = NULL;
  }
}

void ScheduleRewriteContext::MarkFailed() {
  if (central_controller_ != NULL) {
    central_controller_->NotifyRewriteFailed(key_);
    central_controller_ = NULL;
  }
}

ScheduleRewriteCallback::ScheduleRewriteCallback(
    const GoogleString& key, QueuedWorkerPool::Sequence* sequence)
    : CentralControllerCallback<ScheduleRewriteContext>(sequence), key_(key) {
}

ScheduleRewriteCallback::~ScheduleRewriteCallback() {
}

ScheduleRewriteContext* ScheduleRewriteCallback::CreateTransactionContext(
    CentralControllerInterface* interface) {
  return new ScheduleRewriteContext(interface, key_);
}

}  // namespace net_instaweb
