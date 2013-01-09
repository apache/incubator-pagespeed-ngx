// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The Watchdog class creates a second thread that can Alarm if a specific
// duration of time passes without proper attention.  The duration of time is
// specified at construction time.  The Watchdog may be used many times by
// simply calling Arm() (to start timing) and Disarm() (to reset the timer).
// The Watchdog is typically used under a debugger, where the stack traces on
// other threads can be examined if/when the Watchdog alarms.

// Some watchdogs will be enabled or disabled via command line switches. To
// facilitate such code, an "enabled" argument for the constuctor can be used
// to permanently disable the watchdog.  Disabled watchdogs don't even spawn
// a second thread, and their methods call (Arm() and Disarm()) return very
// quickly.

#ifndef BASE_THREADING_WATCHDOG_H_
#define BASE_THREADING_WATCHDOG_H_
#pragma once

#include <string>

#include "base/base_api.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"

namespace base {

class BASE_API Watchdog {
 public:
  // Constructor specifies how long the Watchdog will wait before alarming.
  Watchdog(const TimeDelta& duration,
           const std::string& thread_watched_name,
           bool enabled);
  virtual ~Watchdog();

  // Start timing, and alarm when time expires (unless we're disarm()ed.)
  void Arm();  // Arm  starting now.
  void ArmSomeTimeDeltaAgo(const TimeDelta& time_delta);
  void ArmAtStartTime(const TimeTicks start_time);

  // Reset time, and do not set off the alarm.
  void Disarm();

  // Alarm is called if the time expires after an Arm() without someone calling
  // Disarm().  This method can be overridden to create testable classes.
  virtual void Alarm();

  // Reset static data to initial state. Useful for tests, to ensure
  // they are independent.
  static void ResetStaticData();

 private:
  class ThreadDelegate : public PlatformThread::Delegate {
   public:
    explicit ThreadDelegate(Watchdog* watchdog) : watchdog_(watchdog) {
    }
    virtual void ThreadMain();
   private:
    void SetThreadName() const;

    Watchdog* watchdog_;
  };

  enum State {ARMED, DISARMED, SHUTDOWN };

  bool init_successful_;

  Lock lock_;  // Mutex for state_.
  ConditionVariable condition_variable_;
  State state_;
  const TimeDelta duration_;  // How long after start_time_ do we alarm?
  const std::string thread_watched_name_;
  PlatformThreadHandle handle_;
  ThreadDelegate delegate_;  // Store it, because it must outlive the thread.

  TimeTicks start_time_;  // Start of epoch, and alarm after duration_.

  // When the debugger breaks (when we alarm), all the other alarms that are
  // armed will expire (also alarm).  To diminish this effect, we track any
  // delay due to debugger breaks, and we *try* to adjust the effective start
  // time of other alarms to step past the debugging break.
  // Without this safety net, any alarm will typically trigger a host of follow
  // on alarms from callers that specify old times.
  static Lock static_lock_;  // Lock for access of static data...
  // When did we last alarm and get stuck (for a while) in a debugger?
  static TimeTicks last_debugged_alarm_time_;
  // How long did we sit on a break in the debugger?
  static TimeDelta last_debugged_alarm_delay_;

  DISALLOW_COPY_AND_ASSIGN(Watchdog);
};

}  // namespace base

#endif  // BASE_THREADING_WATCHDOG_H_
