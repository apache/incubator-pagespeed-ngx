/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/function.h"
#include "base/logging.h"

namespace net_instaweb {

Function::Function()
    : quit_requested_(NULL),
      delete_after_callback_(true) {
  Reset();
}

Function::~Function() {
  DCHECK((run_called_ != cancel_called_) || !delete_after_callback_)
      << "Either run or cancel should be called";
}

void Function::Reset() {
  run_called_ = false;
  cancel_called_ = false;
}

void Function::CallRun() {
  // Note: we need to capture should_delete in a local, since in cases
  // where it's false, an another thread may be responsible for deleting
  // us, so it may be unsafe to access our fields after the callback runs
  // (for example, a different-thread function with a stack-allocated
  //  SchedulerBlockingFunction may exit in between our call to Run() and
  //  where we check for whether to delete or not).
  bool should_delete = delete_after_callback_;
  DCHECK(!cancel_called_);
  DCHECK(!run_called_);
  run_called_ = true;
  Run();
  if (should_delete) {
    delete this;
  }
}

void Function::CallCancel() {
  bool should_delete = delete_after_callback_;
  DCHECK(!cancel_called_);
  DCHECK(!run_called_);
  cancel_called_ = true;
  Cancel();
  if (should_delete) {
    delete this;
  }
}

}  // namespace net_instaweb
