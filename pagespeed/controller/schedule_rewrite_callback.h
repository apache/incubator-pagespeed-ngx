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

#ifndef PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_CALLBACK_H_
#define PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_CALLBACK_H_

#include "pagespeed/controller/central_controller_interface.h"
#include "pagespeed/controller/central_controller_callback.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"

// Callback classes to support rewrite scheduling in
// CentralControllerInterfaceAdapter.

namespace net_instaweb {

// Passed to RunImpl for implementations of ScheduleRewriteCallback.
class ScheduleRewriteContext {
 public:
  explicit ScheduleRewriteContext(CentralControllerInterface* interface,
                                  const GoogleString& key);
  ~ScheduleRewriteContext();

  // Mark the rewrite operation as complete. MarkSucceeded will be
  // automatically invoked at destruction if neither is explicitly called.
  void MarkSucceeded();
  void MarkFailed();

 private:
  CentralControllerInterface* central_controller_;
  GoogleString key_;
};

// Implementor interface to rewrite scheduling features in
// CentralControllerInterfaceAdapter.
class ScheduleRewriteCallback
    : public CentralControllerCallback<ScheduleRewriteContext> {
 public:
  explicit ScheduleRewriteCallback(const GoogleString& key,
                                   QueuedWorkerPool::Sequence* sequence);
  virtual ~ScheduleRewriteCallback();

  const GoogleString& key() { return key_; }

 private:
  virtual ScheduleRewriteContext* CreateTransactionContext(
      CentralControllerInterface* interface);

  // CentralControllerCallback interface.
  virtual void RunImpl(scoped_ptr<ScheduleRewriteContext>* context) = 0;
  virtual void CancelImpl() = 0;

  GoogleString key_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_CALLBACK_H_
