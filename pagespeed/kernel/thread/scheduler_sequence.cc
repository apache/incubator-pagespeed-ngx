/*
 * Copyright 2015 Google Inc.
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

#include "pagespeed/kernel/thread/scheduler_sequence.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/thread/scheduler.h"

namespace net_instaweb {

Scheduler::Sequence::Sequence(Scheduler* scheduler)
    : scheduler_(scheduler),
      forwarding_sequence_(nullptr) {
}

Scheduler::Sequence::~Sequence() {
  while (!work_queue_.empty()) {
    Function* function = work_queue_.front();
    work_queue_.pop_front();
    function->CallCancel();
  }
}

void Scheduler::Sequence::Add(Function* function) {
  net_instaweb::Sequence* forwarding_sequence = nullptr;
  {
    ScopedMutex lock(scheduler_->mutex());
    if (forwarding_sequence_ != nullptr) {
      forwarding_sequence = forwarding_sequence_;
    } else {
      work_queue_.push_back(function);
      scheduler_->Signal();
    }
  }
  if (forwarding_sequence != nullptr) {
    forwarding_sequence->Add(function);
  }
}

bool Scheduler::Sequence::RunTasksUntil(int64 timeout_ms, bool* done) {
  scheduler_->mutex()->DCheckLocked();
  DCHECK(forwarding_sequence_ == nullptr);
  Timer* timer = scheduler_->timer();
  int64 start_time_ms = timer->NowMs();
  int64 end_ms = timeout_ms + start_time_ms;
  while (!*done) {
    if (!work_queue_.empty()) {
      Function* function = work_queue_.front();
      work_queue_.pop_front();
      scheduler_->mutex()->Unlock();
      function->CallRun();
      scheduler_->mutex()->Lock();
    } else {
      int64 now_ms = timer->NowMs();
      int64 remaining_ms = end_ms - now_ms;
      if (remaining_ms <= 0) {
        return false;
      }
      scheduler_->BlockingTimedWaitMs(remaining_ms);
    }
  }
  return true;
}

void Scheduler::Sequence::ForwardToSequence(
    net_instaweb::Sequence* forwarding_sequence) {
  scheduler_->mutex()->DCheckLocked();
  DCHECK(forwarding_sequence != nullptr);
  forwarding_sequence_ = forwarding_sequence;
  while (!work_queue_.empty()) {
    Function* function = work_queue_.front();
    work_queue_.pop_front();
    // Takes forwarding_sequence's mutex while holding scheduler_->mutex().
    forwarding_sequence->Add(function);
  }
}

}  // namespace net_instaweb
