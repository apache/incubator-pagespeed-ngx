/*
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/util/public/timer_based_abstract_lock.h"

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/debug.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Number of times we busy spin before we start to sleep.
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

}  // namespace

TimerBasedAbstractLock::~TimerBasedAbstractLock() { }

void TimerBasedAbstractLock::Lock() {
  if (!TryLock()) {
    Spin(&TimerBasedAbstractLock::TryLockIgnoreSteal, 0, kMaxSpinSleepMs);
  }
}

bool TimerBasedAbstractLock::LockTimedWait(int64 wait_ms) {
  return (TryLock() ||
          SpinFor(&TimerBasedAbstractLock::TryLockIgnoreSteal,
                  kMinTriesPerSteal * kMaxSpinSleepMs, wait_ms));
}

void TimerBasedAbstractLock::LockStealOld(int64 steal_ms) {
  if (!TryLock()) {
    int64 max_sleep_ms = (steal_ms + 1) / kMinTriesPerSteal;
    Spin(&TimerBasedAbstractLock::TryLockStealOld, steal_ms, max_sleep_ms);
  }
}

bool TimerBasedAbstractLock::LockTimedWaitStealOld(
    int64 wait_ms, int64 steal_ms) {
  return
      (TryLock() ||
       SpinFor(&TimerBasedAbstractLock::TryLockStealOld, steal_ms, wait_ms));
}

// We implement spinning without regard to whether the underlying lock primitive
// can time out or not.  This wrapper method is just TryLock with a bogus
// timeout parameter, so that we can pass timeout-free TryLock to a spin
// routine.
bool TimerBasedAbstractLock::TryLockIgnoreSteal(int64 steal_ignored) {
  return TryLock();
}

// Actively attempt to take lock without pausing.
bool TimerBasedAbstractLock::BusySpin(TryLockMethod try_lock,
                                      int64 steal_ms) {
  for (int i = 0; i < kBusySpinIterations; i++) {
    if ((this->*try_lock)(steal_ms)) {
      return true;
    }
  }
  return false;
}

// Attempt to take lock, starting with a busy spin, and spinning forever if the
// lock is never obtained or stolen due to timeout.
void TimerBasedAbstractLock::Spin(
    TryLockMethod try_lock, int64 steal_ms, int64 max_interval_ms) {
  if (BusySpin(try_lock, steal_ms)) {
    return;
  }
  Timer* the_timer = timer();
  int64 interval_ms = 0;
  while (!(this->*try_lock)(steal_ms)) {
    the_timer->SleepMs(interval_ms);
    interval_ms = Backoff(interval_ms, max_interval_ms);
  }
}

// Attempt to take or steal lock, but block for approximately at most wait_ms.
// If we obtain the lock, immediately return true.
bool TimerBasedAbstractLock::SpinFor(TryLockMethod try_lock, int64 steal_ms,
                                     int64 wait_ms) {
  Timer* the_timer = timer();
  int64 end_time_ms = the_timer->NowMs() + wait_ms;
  if (BusySpin(try_lock, steal_ms)) {
    return true;
  }
  int64 max_interval_ms = (steal_ms + 1) / kMinTriesPerSteal;
  int64 interval_ms =
      IntervalWithEnd(the_timer, 0, max_interval_ms, end_time_ms);
  // Spin until the result is true (we got the lock) or we run out of time.
  while (interval_ms > 0) {
    if ((this->*try_lock)(steal_ms)) {
      return true;
    }
    the_timer->SleepMs(interval_ms);
    interval_ms =
        IntervalWithEnd(the_timer, interval_ms, max_interval_ms, end_time_ms);
  }
  // Timed out.
  return false;
}

}  // namespace net_instaweb
