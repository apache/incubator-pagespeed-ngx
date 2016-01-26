/*
 * Copyright 2016 Google Inc.
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

#ifndef PAGESPEED_KERNEL_THREAD_SCHEDULER_SEQUENCE_H_
#define PAGESPEED_KERNEL_THREAD_SCHEDULER_SEQUENCE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/vector_deque.h"
#include "pagespeed/kernel/thread/scheduler.h"
#include "pagespeed/kernel/thread/sequence.h"

namespace net_instaweb {

// Implements a sequence which is run directly from RunTasksUntil,
// rather than running in a background thread.
class Scheduler::Sequence : public net_instaweb::Sequence {
 public:
  // The scheduler is used for doing timed-waits so that any pending
  // scheduler alarms fire before the wait-period ends.
  explicit Sequence(Scheduler* scheduler);
  virtual ~Sequence();

  void Add(Function* function) override LOCKS_EXCLUDED(scheduler_->mutex());

  // Runs functions for this sequence directly, until *done is true or
  // the timeout expires.  Returns 'false' if the timeout expired prior
  // to 'done' getting set to true.  'done' must be protected
  // by scheduler_->mutex(), and is expected to be set by one of the sequence
  // tasks.
  bool RunTasksUntil(int64 timeout_ms, bool* done)
      EXCLUSIVE_LOCKS_REQUIRED(scheduler_->mutex());

  // Atomically forwards all activity to an alternative Sequence.
  // Any pending functions in the work_queue_ are transferred into
  // the sequence, and new functions passed to Add are added to
  // forwarding_sequence rather than being placed into the WorkQueue.
  //
  // This is intended to be called once, after request-thread
  // activity is done, to handle any pending background tasks.
  //
  // Note: both the scheduler lock (which must be held when calling
  // this method) and the forwarding_sequence's mutex must be held
  // concurrently during the implementation of Schedule::Add.  We
  // don't want to let go of scheduler_->mutex() during this operation
  // because then it would no longer be atomic.  Thus forwarding_sequence
  // must not have the same mutex as scheduler_.
  void ForwardToSequence(net_instaweb::Sequence* forwarding_sequence)
      EXCLUSIVE_LOCKS_REQUIRED(scheduler_->mutex());

 private:
  VectorDeque<Function*> work_queue_ GUARDED_BY(scheduler_->mutex());
  Scheduler* scheduler_;
  net_instaweb::Sequence* forwarding_sequence_ GUARDED_BY(scheduler_->mutex());

  DISALLOW_COPY_AND_ASSIGN(Sequence);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_THREAD_SCHEDULER_SEQUENCE_H_
