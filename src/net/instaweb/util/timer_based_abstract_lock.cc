/**
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

#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

// Number of times we busy spin before we start to sleep.
int kBusySpinIterations = 100;

// We back off exponentially, with a constant of 1.5.
int64 Backoff(int64 interval_ms) {
  return (1 + interval_ms + (interval_ms >> 1));
}

int64 IntervalWithEnd(Timer* timer, int64 interval_ms, int64 end_time_ms) {
  int64 now_ms = timer->NowMs();
  int64 remaining = end_time_ms - now_ms;
  interval_ms = Backoff(interval_ms);
  if (remaining > interval_ms) {
    return interval_ms;
  } else {
    return remaining;
  }
}

}  // namespace

TimerBasedAbstractLock::~TimerBasedAbstractLock() { }

void TimerBasedAbstractLock::Lock() {
  if (TryLock() ||
      BusySpin(&TimerBasedAbstractLock::TryLockIgnoreTimeout, 0)) {
    return;
  }
  Spin(&TimerBasedAbstractLock::TryLockIgnoreTimeout, 0);
}

bool TimerBasedAbstractLock::LockTimedWait(int64 wait_ms) {
  return (TryLock() ||
          SpinFor(&TimerBasedAbstractLock::TryLockIgnoreTimeout, 0, wait_ms));
}

void TimerBasedAbstractLock::LockStealOld(int64 timeout_ms) {
  if (LockTimedWaitStealOld(timeout_ms, timeout_ms)) {
    return;
  }
  // Contention!  Restart spin, but don't decay again in future.
  Spin(&TimerBasedAbstractLock::TryLockStealOld, timeout_ms);
}

bool TimerBasedAbstractLock::LockTimedWaitStealOld(
    int64 wait_ms, int64 timeout_ms) {
  return
      (TryLock() ||
       SpinFor(&TimerBasedAbstractLock::TryLockStealOld, timeout_ms, wait_ms));
}

// For uniformity, we implement spinning without regard to whether the
// underlying lock primitive can time out or not.  This wrapper method is just
// TryLock with a bogus timeout parameter for uniformity.
bool TimerBasedAbstractLock::TryLockIgnoreTimeout(int64 timeout_ignored) {
  return TryLock();
}

// Actively attempt to take lock without pausing.
bool TimerBasedAbstractLock::BusySpin(TryLockMethod try_lock,
                                      int64 timeout_ms) {
  for (int i = 0; i < kBusySpinIterations; i++) {
    if ((this->*try_lock)(timeout_ms)) {
      return true;
    }
  }
  return false;
}

// Attempt to take lock, backing off forever.
void TimerBasedAbstractLock::Spin(TryLockMethod try_lock, int64 timeout_ms) {
  Timer* the_timer = timer();
  int64 interval_ms = 1;
  while (!(this->*try_lock)(timeout_ms)) {
    the_timer->SleepMs(interval_ms);
    interval_ms = Backoff(interval_ms);
    // interval_ms can't realistically overflow in geologic time.
  }
}

// Attempt to take lock until end_time_ms.
bool TimerBasedAbstractLock::SpinFor(TryLockMethod try_lock, int64 timeout_ms,
                                     int64 wait_ms) {
  int64 end_time_ms = timer()->NowMs() + wait_ms;
  if (BusySpin(try_lock, timeout_ms)) {
    return true;
  }
  Timer* the_timer = timer();
  int64 interval_ms = IntervalWithEnd(the_timer, 0, end_time_ms);
  bool result;
  // Ugh.  Here we're spinning until result is true (we got the lock), or we run
  // out of time.
  while (!((result = (this->*try_lock)(timeout_ms))) && interval_ms > 0) {
    the_timer->SleepMs(interval_ms);
    interval_ms = IntervalWithEnd(the_timer, interval_ms, end_time_ms);
  }
  return result;
}

}  // namespace net_instaweb
