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
// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SCHEDULER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SCHEDULER_H_

#include <set>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class AbstractMutex;
class Function;
class Timer;

// Implements a simple scheduler that allows a thread to block until either time
// expires, or a condition variable is signaled.  Also permits various alarms to
// be scheduled; these are lightweight short-lived callbacks that must be safely
// runnable from any thread in any lock state in which scheduler invocations
// occur.  Finally, implements a hybrid between these: a callback that can be
// run when the condition variable is signaled.
//
// This class is designed to be overridden, but only to re-implement its
// internal notion of blocking to permit time to be mocked by MockScheduler.
class Scheduler {
 public:
  // A callback for a scheduler alarm, with an associated wakeup time (absolute
  // time after which the callback will be invoked with Run() by the scheduler).
  // Alarm should be treated as an opaque type.
  class Alarm;

  // Sorting comparator for Alarms, so that they can be retrieved in time
  // order.  For use by std::set, thus public.
  struct CompareAlarms {
    bool operator()(const Alarm* a, const Alarm* b) const;
  };

  Scheduler(ThreadSystem* thread_system, Timer* timer);
  virtual ~Scheduler();

  AbstractMutex* mutex();

  // Optionally check that mutex is locked for debugging purposes.
  void EnsureLocked();

  // Condition-style methods: The following three methods provide a simple
  // condition-variable-style interface that can be used to coordinate the
  // threads sharing the scheduler.

  // Wait at most timeout_us, or until Signal() is called.  mutex() must be held
  // when calling BlockingTimedWait.
  void BlockingTimedWait(int64 timeout_ms);

  // Non-blocking invocation of callback either when Signal() is called, or
  // after timeout_us have passed.  Ownership of callback passes to the
  // scheduler, which deallocates it after invocation.  mutex() must be held on
  // the initial call, and is locked for the duration of callback.  Note that
  // callback may be invoked in a different thread from the calling thread.
  void TimedWait(int64 timeout_ms, Function* callback);

  // Signal threads in BlockingTimedWait and invoke TimedWait callbacks.
  // mutex() must be held when calling Signal.  Performs outstanding work,
  // including any triggered by the signal, before returning; note that this
  // means it may drop the scheduler lock internally while doing callback
  // invocation, which is different from the usual condition variable signal
  // semantics.
  void Signal();

  // Alarms.  The following two methods provide a mechanism for scheduling
  // alarm tasks, each run at a particular time.

  // Schedules an alarm for absolute time wakeup_time_us, using the passed-in
  // Function* as the alarm callback.  Returns the created Alarm.  Performs
  // outstanding work.  The returned alarm will own the callback and will clean
  // itself and the callback when it is run or cancelled.  NOTE in particular
  // that calls to CancelAlarm must ensure the callback has not been invoked
  // yet.  This is why the scheduler mutex must be held for CancelAlarm.
  Alarm* AddAlarm(int64 wakeup_time_us, Function* callback);

  // Cancels an alarm, calling the Cancel() method and deleting the alarm
  // object.  Scheduler mutex must be held before call to ensure that alarm is
  // not called back before cancellation occurs.  Doesn't perform outstanding
  // work.  Returns true if the cancellation occurred.  If false is returned,
  // the alarm is already being run / has been run in another thread; if the
  // alarm deletes itself on Cancel(), it may no longer safely be used.
  bool CancelAlarm(Alarm* alarm);

  // Finally, ProcessAlarms provides a mechanism to ensure that pending alarms
  // are executed in the absence of other scheduler activity.  ProcessAlarms:
  // handle outstanding alarms, or if there are none wait until the next wakeup
  // and handle alarms then before relinquishing control.  Idle no longer than
  // timeout_us.  Passing in timeout_us=0 will run without blocking.
  void ProcessAlarms(int64 timeout_us);

  // Internal method to kick the system because something of interest to the
  // overridden AwaitWakeup method has happened.  Exported here because C++
  // naming hates you.
  void Wakeup();

 protected:
  // Internal method to await a wakeup event.  Block until wakeup_time_us (an
  // absolute time since the epoch), or until something interesting (such as a
  // call to Signal) occurs.  This is virtual to permit us to mock it out (the
  // mock simply advances time).
  virtual void AwaitWakeup(int64 wakeup_time_us);

 private:
  class CondVarTimeout;
  class CondVarCallbackTimeout;
  class Mutex;

  typedef std::set<Alarm*, CompareAlarms> AlarmSet;

  int64 RunAlarms(bool* ran_alarms);
  void AddAlarmMutexHeld(int64 wakeup_time_us, Alarm* alarm);
  void CancelWaiting(Alarm* alarm);

  ThreadSystem* thread_system_;
  Timer* timer_;
  scoped_ptr<Mutex> mutex_;
  // condvar_ tracks whether interesting (next-wakeup decreasing or
  // signal_count_ increasing) events occur.
  scoped_ptr<ThreadSystem::Condvar> condvar_;
  uint32 index_;  // Used to disambiguate alarms with equal deadlines
  AlarmSet outstanding_alarms_;  // Priority queue of future alarms
  // An alarm may be deleted iff it is successfully removed from
  // outstanding_alarms_.
  int64 signal_count_;           // Number of times Signal has been called
  AlarmSet waiting_alarms_;      // Alarms waiting for signal_count to change
  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SCHEDULER_H_
