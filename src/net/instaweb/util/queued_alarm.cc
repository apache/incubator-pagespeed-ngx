// Copyright 2011 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/util/public/queued_alarm.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/queued_worker_pool.h"
#include "net/instaweb/util/public/thread_system.h"

#include "base/logging.h"

namespace net_instaweb {

QueuedAlarm::QueuedAlarm(Scheduler* scheduler,
                         QueuedWorkerPool::Sequence* sequence,
                         int64 wakeup_time_us,
                         Function* callback)
    : mutex_(scheduler->thread_system()->NewMutex()),
      scheduler_(scheduler),
      sequence_(sequence),
      callback_(callback),
      canceled_(false),
      queued_sequence_portion_(false) {
  set_delete_after_callback(false);
  alarm_ = scheduler_->AddAlarm(wakeup_time_us, this);
}

QueuedAlarm::~QueuedAlarm() {
  if (callback_ != NULL) {
    callback_->CallCancel();
  }
}

void QueuedAlarm::CancelAlarm() {
  // Note that this has to be serialized with respect to SequencePortionOfRun
  // and the user callback due to use of sequences, but it may overlap Run().

  ScopedMutex hold_our_mutex(mutex_.get());
  if (queued_sequence_portion_) {
    // The actual underlying alarm has run, and we have queued invocation of
    // SequencePortionOfRun(); so we just need to tell it to quash itself.
    // Note that it's unsafe to call CancelAlarm at this point ---
    // the underlying Scheduler::Alarm object is dead.
    canceled_ = true;
    return;
  }

  ScopedMutex hold_scheduler_mutex(scheduler_->mutex());
  if (scheduler_->CancelAlarm(alarm_)) {
    // Everything canceled nice and clean, so we can go home.
    hold_our_mutex.Release();  // before deleting self
    hold_scheduler_mutex.Release();  // so we don't hold it during CallCancel
    delete this;
  } else {
    // We're in process of invoking Run(), but didn't actually run its
    // body (serialized on our mutex). In this case, signal to it via
    // canceled_ to wrap things up.
    canceled_ = true;
  }
}

// Runs in arbitrary thread.
void QueuedAlarm::Run() {
  ScopedMutex lock(mutex_.get());
  if (canceled_) {
    lock.Release();  // before deleting self
    delete this;
  } else {
    queued_sequence_portion_ = true;
    sequence_->Add(MakeFunction(this,
                                &QueuedAlarm::SequencePortionOfRun,
                                &QueuedAlarm::SequencePortionOfRunCancelled));
  }
}

// Runs in the sequence context
void QueuedAlarm::SequencePortionOfRun() {
  bool canceled;
  {
    ScopedMutex lock(mutex_.get());
    canceled = canceled_;
  }

  if (!canceled) {
    callback_->CallRun();
    callback_ = NULL;  // so we don't do ->CallCancel in ~QueuedAlarm
  }

  delete this;
}

void QueuedAlarm::SequencePortionOfRunCancelled() {
  {
    ScopedMutex lock(mutex_.get());
    DCHECK(canceled_);
  }
  delete this;
}

}  // namespace net_instaweb
