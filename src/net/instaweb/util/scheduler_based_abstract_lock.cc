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

// Author: jmaessen@google.com (Jan Maessen)

#include "net/instaweb/util/public/scheduler_based_abstract_lock.h"

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/debug.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Number of times we busy spin before we start to sleep.
// TODO(jmaessen): Is this the right setting?
const int kBusySpinIterations = 100;
const int64 kMaxSpinSleepMs = Timer::kMinuteMs;  // Never sleep for more than 1m
const int64 kMinTriesPerSteal = 2;  // Try to lock twice / steal interval.

// We back off exponentially, with a constant of 1.5.  We add an extra ms to
// this backoff to avoid problems with wait intervals of 0 or 1.  We bound the
// blocking time at kMaxSpinSleepMs.
int64 Backoff(int64 interval_ms, int64 max_interval_ms) {
  int64 new_interval_ms = 1 + interval_ms + (interval_ms >> 1);
  if (new_interval_ms >= max_interval_ms) {
    new_interval_ms = max_interval_ms;
    // Log the first time we reach or cross the threshold.
    // TODO(jmaessen): LOG(ERROR) is deadlocking.  Why?  We're using cooperative
    // thread cancellation in the tests that hang, and it sometimes succeeds.
    if (false && interval_ms != max_interval_ms) {
      LOG(ERROR) << "Reached maximum sleep time " << StackTraceString().c_str();
    }
  }
  return new_interval_ms;
}

// Compute new backoff time interval given current interval_ms, but don't exceed
// max_interval_ms or have the interval continue much past end_time_ms.
int64 IntervalWithEnd(Timer* timer, int64 interval_ms,
                      int64 max_interval_ms, int64 end_time_ms) {
  int64 now_ms = timer->NowMs();
  int64 remaining = end_time_ms - now_ms;
  interval_ms = Backoff(interval_ms, max_interval_ms);
  if (remaining > interval_ms) {
    return interval_ms;
  } else {
    return remaining;
  }
}

// This object actually contains the state needed for periodically polling the
// provided lock using the try_lock method, and for eventually calling or
// canceling the callback. While it may feel attractive to reuse this object,
// it's not actually safe as it runs into races trying to check the
// delete_after_callback_ bit.
class TimedWaitPollState : public Function {
 public:
  typedef bool (SchedulerBasedAbstractLock::*TryLockMethod)(int64 steal_ms);

  TimedWaitPollState(
      Scheduler* scheduler, Function* callback,
      SchedulerBasedAbstractLock* lock, TryLockMethod try_lock,
      int64 steal_ms, int64 end_time_ms,
      int64 max_interval_ms)
      : scheduler_(scheduler),
        callback_(callback),
        lock_(lock),
        try_lock_(try_lock),
        steal_ms_(steal_ms),
        end_time_ms_(end_time_ms),
        max_interval_ms_(max_interval_ms),
        interval_ms_(0) {}
  virtual ~TimedWaitPollState() { }

  // Note: doesn't actually clone interval_ms_.
  TimedWaitPollState* Clone() {
    return new TimedWaitPollState(scheduler_, callback_, lock_,
                                  try_lock_, steal_ms_, end_time_ms_,
                                  max_interval_ms_);
  }

 protected:
  virtual void Run() {
    if ((lock_->*try_lock_)(steal_ms_)) {
      callback_->CallRun();
      return;
    }
    Timer* timer = scheduler_->timer();
    int64 now_ms = timer->NowMs();
    if (now_ms >= end_time_ms_) {
      callback_->CallCancel();
      return;
    }

    TimedWaitPollState* next_try = Clone();
    next_try->interval_ms_ =
        IntervalWithEnd(timer, interval_ms_, max_interval_ms_, end_time_ms_);
    scheduler_->AddAlarm((now_ms + next_try->interval_ms_) * Timer::kMsUs,
                         next_try);
  }

 private:
  Scheduler* scheduler_;
  Function* callback_;
  SchedulerBasedAbstractLock* lock_;
  TryLockMethod try_lock_;
  const int64 steal_ms_;
  const int64 end_time_ms_;
  const int64 max_interval_ms_;
  int64 interval_ms_;
};

}  // namespace

SchedulerBasedAbstractLock::~SchedulerBasedAbstractLock() { }

void SchedulerBasedAbstractLock::PollAndCallback(
    TryLockMethod try_lock, int64 steal_ms, int64 wait_ms, Function* callback) {
  // Measure ending time from immediately after failure of the fast path.
  int64 end_time_ms = scheduler()->timer()->NowMs() + wait_ms;
  if (BusySpin(try_lock, steal_ms)) {
    callback->CallRun();
    return;
  }
  // Slow path.  Allocate a TimedWaitPollState object and cede control to it.
  int64 max_interval_ms = (steal_ms + 1) / kMinTriesPerSteal;
  TimedWaitPollState* poller =
      new TimedWaitPollState(scheduler(), callback, this,
                             try_lock, steal_ms,
                             end_time_ms, max_interval_ms);
  poller->CallRun();
}

// The basic structure of each locking operation is the same:
// Quick check for a free lock using TryLock().
// If that fails, call PollAndCallBack, which:
//   * First busy spins attempting to obtain the lock
//   * If that fails, schedules an alarm that attempts to take the lock,
//     or failing that backs off and schedules another alarm.
// We run callbacks as soon as possible.  We could instead defer them
// to a scheduler sequence, but in practice we don't have an appropriate
// sequence to hand when we we stand up the lock manager.  So it's up to
// callers to schedule appropriate tasks when locks have been obtained.

bool SchedulerBasedAbstractLock::LockTimedWait(int64 wait_ms) {
  if (TryLock()) {
    // Fast path.
    return true;
  } else {
    SchedulerBlockingFunction block(scheduler());
    PollAndCallback(&SchedulerBasedAbstractLock::TryLockIgnoreSteal,
                    kMinTriesPerSteal * kMaxSpinSleepMs, wait_ms, &block);
    return block.Block();
  }
}

void SchedulerBasedAbstractLock::LockTimedWait(
    int64 wait_ms, Function* callback) {
  if (TryLock()) {
    // Fast path.
    callback->CallRun();
  } else {
    PollAndCallback(&SchedulerBasedAbstractLock::TryLockIgnoreSteal,
                    kMinTriesPerSteal * kMaxSpinSleepMs, wait_ms, callback);
  }
}

bool SchedulerBasedAbstractLock::LockTimedWaitStealOld(
    int64 wait_ms, int64 steal_ms) {
  if (TryLock()) {
    // Fast path.
    return true;
  } else {
    SchedulerBlockingFunction block(scheduler());
    PollAndCallback(&SchedulerBasedAbstractLock::TryLockStealOld,
                    steal_ms, wait_ms, &block);
    return block.Block();
  }
}

void SchedulerBasedAbstractLock::LockTimedWaitStealOld(
    int64 wait_ms, int64 steal_ms, Function* callback) {
  if (TryLock()) {
    // Fast path.
    callback->CallRun();
  } else {
    PollAndCallback(&SchedulerBasedAbstractLock::TryLockStealOld,
                    steal_ms, wait_ms, callback);
  }
}

// We implement spinning without regard to whether the underlying lock primitive
// can time out or not.  This wrapper method is just TryLock with a bogus
// timeout parameter, so that we can pass timeout-free TryLock to a spin
// routine.
bool SchedulerBasedAbstractLock::TryLockIgnoreSteal(int64 steal_ignored) {
  return TryLock();
}

// Actively attempt to take lock without pausing.
bool SchedulerBasedAbstractLock::BusySpin(TryLockMethod try_lock,
                                          int64 steal_ms) {
  for (int i = 0; i < kBusySpinIterations; i++) {
    if ((this->*try_lock)(steal_ms)) {
      return true;
    }
  }
  return false;
}

}  // namespace net_instaweb
