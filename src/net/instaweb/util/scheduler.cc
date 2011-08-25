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

#include "net/instaweb/util/public/scheduler.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

const int kIndexNotSet = 0;
const int kMsUs = Timer::kSecondUs / Timer::kSecondMs;

// private class to encapsulate a function being
// scheduled as an alarm.  Owns passed-in function.
class FunctionAlarm : public Scheduler::Alarm {
 public:
  explicit FunctionAlarm(Function* function)
      : function_(function) { }
  virtual ~FunctionAlarm() { }
  virtual void Run() {
    function_->Run();
    delete this;
  }
  virtual void Cancel() {
    function_->Cancel();
    delete this;
  }
 private:
  scoped_ptr<Function> function_;
  DISALLOW_COPY_AND_ASSIGN(FunctionAlarm);
};

}  // namespace

// The following three classes are effectively supposed to be private, and
// should only be used internally to the scheduler, but are semi-exposed due to
// C++ naming restrictions.  The first two implement condvar waiting.  When we
// wait using BlockingTimedWait or TimedWait, we put a single alarm into two
// queues: the outstanding_alarms_ queue, where it will be run if the wait times
// out, and the waiting_alarms_ queue, where it will be canceled if a signal
// arrives.  The system assumes the waiting_alarms_ queue is a subset of the
// outstanding_alarms_ queue, because it holds *only* alarms from ...TimedWait
// operations, so on signal the contents of waiting_alarms are cancelled thus
// removing them from waiting_alarms and invoking the Cancel() method.  However,
// on timeout the Run() method must remove the alarm from the waiting_alarms_
// queue so it can be cleaned up safely; doing so means invoking callbacks and
// requires us to drop the scheduler lock.  This leads to a harmless violation
// of the subset condition; see the comment on CancelWaiting which describes the
// handling of this condition.

// Blocking condvar alarm.  Simply sets a flag for the blocking thread to
// notice.
class Scheduler::CondVarTimeout : public Scheduler::Alarm {
 public:
  CondVarTimeout(bool* set_on_timeout, Scheduler* scheduler)
      : set_on_timeout_(set_on_timeout),
        scheduler_(scheduler) { }
  virtual ~CondVarTimeout() { }
  virtual void Run() {
    *set_on_timeout_ = true;
    scheduler_->CancelWaiting(this);
    delete this;
  }
  virtual void Cancel() {
    delete this;
  }
 private:
  bool* set_on_timeout_;
  Scheduler* scheduler_;
  DISALLOW_COPY_AND_ASSIGN(CondVarTimeout);
};

// Non-blocking condvar alarm.  Must run the passed-in callback on either
// timeout (Run()) or signal (Cancel()).
class Scheduler::CondVarCallbackTimeout : public Scheduler::Alarm {
 public:
  CondVarCallbackTimeout(Function* callback, Scheduler* scheduler)
      : callback_(callback),
        scheduler_(scheduler) { }
  virtual ~CondVarCallbackTimeout() { }
  virtual void Run() {
    scheduler_->CancelWaiting(this);
    Cancel();
  }
  virtual void Cancel() {
    {
      ScopedMutex lock(scheduler_->mutex());
      callback_->Run();
    }
    delete this;
  }
 private:
  scoped_ptr<Function> callback_;
  Scheduler* scheduler_;
  DISALLOW_COPY_AND_ASSIGN(CondVarCallbackTimeout);
};

// This class implements a wrapper around a CondvarCapableMutex to permit
// checking of lock state on entry and exit.
class Scheduler::Mutex : public AbstractMutex {
 public:
  explicit Mutex(ThreadSystem::CondvarCapableMutex* mutex)
      : mutex_(mutex) { }
  virtual ~Mutex() { }

  ThreadSystem::CondvarCapableMutex* mutex() {
    return mutex_.get();
  }

  void EnsureLocked() {
    DCHECK(locked_.value());
  }

  void DropLockControl() {
    EnsureLocked();
    locked_.set_value(false);
  }

  void TakeLockControl() {
    DCHECK(!locked_.value());
    locked_.set_value(true);
  }

  virtual void Lock() {
    mutex_->Lock();
    TakeLockControl();
  }

  virtual void Unlock() {
    DropLockControl();
    mutex_->Unlock();
  }

 private:
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  AtomicBool locked_;
  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

Scheduler::Alarm::Alarm()
    : wakeup_time_us_(0),
      index_(kIndexNotSet) { }

Scheduler::Alarm::~Alarm() { }

int Scheduler::Alarm::Compare(const Alarm* other) const {
  int cmp = 0;
  if (this != other) {
    if (wakeup_time_us_ < other->wakeup_time_us_) {
      cmp = -1;
    } else if (wakeup_time_us_ > other->wakeup_time_us_) {
      cmp = 1;
    } else if (index_ < other->index_) {
      cmp = -1;
    } else {
      DCHECK(index_ > other->index_);
      cmp = 1;
    }
  }
  return cmp;
}

Scheduler::Scheduler(ThreadSystem* thread_system, Timer* timer)
    : thread_system_(thread_system),
      timer_(timer),
      mutex_(new Mutex(thread_system->NewMutex())),
      condvar_(mutex_->mutex()->NewCondvar()),
      index_(kIndexNotSet),
      signal_count_(0) {
}

Scheduler::~Scheduler() {
}

AbstractMutex* Scheduler::mutex() { return mutex_.get(); }

void Scheduler::EnsureLocked() { mutex_->EnsureLocked(); }

void Scheduler::BlockingTimedWait(int64 timeout_ms) {
  mutex_->EnsureLocked();
  int64 now_us = timer_->NowUs();
  int64 wakeup_time_us = now_us + timeout_ms * kMsUs;
  // We block until signal_count_ changes or we time out.
  int64 original_signal_count = signal_count_;
  bool timed_out = false;
  // Schedule a timeout alarm.
  CondVarTimeout* alarm = new CondVarTimeout(&timed_out, this);
  AddAlarmMutexHeld(wakeup_time_us, alarm);
  waiting_alarms_.insert(alarm);
  int64 next_wakeup_us = RunAlarms(NULL);
  while (signal_count_ == original_signal_count && !timed_out &&
         next_wakeup_us > 0) {
    // Now we have to block until either we time out, or we are signaled.  We
    // stop when outstanding_alarms_ is empty as a belt and suspenders
    // protection against programmer error; this ought to imply timed_out.
    AwaitWakeup(wakeup_time_us);
    next_wakeup_us = RunAlarms(NULL);
  }
}

void Scheduler::TimedWait(int64 timeout_ms, Function* callback) {
  mutex_->EnsureLocked();
  int64 now_us = timer_->NowUs();
  int64 completion_time_us = now_us + timeout_ms * kMsUs;
  // We create the alarm for this callback, and register it.  We also register
  // the alarm with the signal queue, where the callback will be run on
  // cancellation.
  CondVarCallbackTimeout* alarm = new CondVarCallbackTimeout(callback, this);
  AddAlarmMutexHeld(completion_time_us, alarm);
  waiting_alarms_.insert(alarm);
  RunAlarms(NULL);
}

void Scheduler::CancelWaiting(Alarm* alarm) {
  // Called to clean up a [Blocking]TimedWait that timed out.  This wins races
  // with a pending Signal, but the erase operation below might find the alarm
  // already gone.  This happens on timeout due to this sequence of events:
  //   ... mutex() held when looking for timeouts...
  //   Remove alarm from outstanding_alarms_ in preparation to run it
  //   Unlock mutex()
  //    call Run()
  //     ... signal operation can run between unlock and lock
  //     ... it will find alarm missing from outstanding_alarms_ and won't
  //     signal it.
  //     ... but it will remove it from waiting_alarms_.
  //     Lock mutex()
  //     CancelWaiting()  [ removed from waiting_alarms_, but that's OK. ]
  // Permitting that harmless race lets us use successful removal from
  // outstanding_alarms_ as the source of truth about whether timeout will occur
  // or not.
  ScopedMutex lock(mutex_.get());
  waiting_alarms_.erase(alarm);
}

void Scheduler::Signal() {
  mutex_->EnsureLocked();
  ++signal_count_;
  if (!waiting_alarms_.empty()) {
    AlarmSet alarms_to_cancel;
    // Empty the waiting_alarms_ set, calling the Cancel() callback for the ones
    // that are still waiting to time out in the outstanding_alarms_ set.  We
    // may drop the lock while running the Cancel() callback, so in order to
    // guarantee atomicity we first remove all the waiting alarms and *then* we
    // invoke their callbacks.  Only if we successfully remove them from
    // outstanding_alarms_ are we responsible for calling Cancel().
    for (AlarmSet::iterator i = waiting_alarms_.begin();
         i != waiting_alarms_.end(); ++i) {
      Alarm* alarm = *i;
      if (outstanding_alarms_.erase(alarm) != 0) {
        alarms_to_cancel.insert(alarm);
      }
    }
    waiting_alarms_.clear();
    condvar_->Broadcast();
    // TODO(jmaessen): This looks like it may be the wrong locking convention
    // here.  The question is this: should we require lock hold for Signal?  If
    // so, is it OK to relinquish the lock within Signal?  For a generic Condvar
    // it is not, but we're not actually cooking up a generic condvar here.
    // That said, it might be simplest to hold the lock across the callback.
    // Can we hold the lock for all alarm operations, requiring them never to
    // drop it, or is that too pricey?  That would be the simplest invariant to
    // enforce.
    mutex()->Unlock();
    for (AlarmSet::iterator i = alarms_to_cancel.begin();
         i != alarms_to_cancel.end(); ++i) {
      Alarm* alarm = *i;
      alarm->Cancel();
    }
    mutex()->Lock();
  }
  RunAlarms(NULL);
}

// Add alarm while holding mutex.  Don't run any alarms or otherwise drop mutex.
void Scheduler::AddAlarmMutexHeld(int64 wakeup_time_us, Alarm* alarm) {
  mutex_->EnsureLocked();
  alarm->wakeup_time_us_ = wakeup_time_us;
  alarm->index_ = ++index_;
  if (!outstanding_alarms_.empty()) {
    // Someone may care about changes in wait time.  Signal if any occurred.
    Alarm* first_alarm = *outstanding_alarms_.begin();
    if (wakeup_time_us < first_alarm->wakeup_time_us_) {
      condvar_->Broadcast();
    }
  }
  outstanding_alarms_.insert(alarm);
}

void Scheduler::AddAlarm(int64 wakeup_time_us, Alarm* alarm) {
  ScopedMutex lock(mutex_.get());
  AddAlarmMutexHeld(wakeup_time_us, alarm);
  RunAlarms(NULL);
}

Scheduler::Alarm* Scheduler::AddAlarmFunction(int64 wakeup_time_us,
                                              Function* callback) {
  Alarm* result = new FunctionAlarm(callback);
  AddAlarm(wakeup_time_us, result);
  return result;
}

bool Scheduler::CancelAlarmMutexHeld(Alarm* alarm) {
  mutex_->EnsureLocked();
  if (outstanding_alarms_.erase(alarm) != 0) {
    mutex_->Unlock();
    alarm->Cancel();
    mutex_->Lock();
    return true;
  } else {
    return false;
  }
}

bool Scheduler::CancelAlarm(Alarm* alarm) {
  ScopedMutex lock(mutex_.get());
  return CancelAlarmMutexHeld(alarm);
}

// Run any alarms that have reached their deadline.  Requires that we hold
// mutex_ before calling.  Returns the time of the next deadline, or 0 if no
// further deadlines loom.  Sets *ran_alarms if non-NULL and any alarms were
// run, otherwise leaves it untouched.
int64 Scheduler::RunAlarms(bool* ran_alarms) {
  while (!outstanding_alarms_.empty()) {
    mutex_->EnsureLocked();
    // We don't use the iterator to go through the set, because we're dropping
    // the lock in mid-loop thus permitting new insertions and cancellations.
    AlarmSet::iterator first_alarm_iterator = outstanding_alarms_.begin();
    Alarm* first_alarm = *first_alarm_iterator;
    int64 now_us = timer_->NowUs();
    if (now_us < first_alarm->wakeup_time_us_) {
      // The next deadline lies in the future.
      return first_alarm->wakeup_time_us_;
    }
    // first_alarm should be run.  It can't have been cancelled as we've held
    // the lock since we found it.
    outstanding_alarms_.erase(first_alarm_iterator);  // Prevent cancellation.
    if (ran_alarms != NULL) {
      *ran_alarms = true;
    }
    mutex_->Unlock();                     // Don't hold the lock when running.
    first_alarm->Run();
    mutex_->Lock();                       // Continue looking for expired work.
  }
  return 0;
}

void Scheduler::AwaitWakeup(int64 wakeup_time_us) {
  mutex_->EnsureLocked();
  int64 now_us = timer_->NowUs();
  // Compute how long we should wait.  Note: we overshoot, which may lead us
  // to wake a bit later than expected.  We assume the system is likely to
  // round wakeup time off for us in some arbitrary fashion in any case.
  int64 wakeup_interval_ms = (wakeup_time_us - now_us + kMsUs - 1) / kMsUs;
  mutex_->DropLockControl();
  condvar_->TimedWait(wakeup_interval_ms);
  mutex_->TakeLockControl();
}

void Scheduler::Wakeup() {
  condvar_->Broadcast();
}

void Scheduler::ProcessAlarms(int64 timeout_us) {
  ScopedMutex lock(mutex_.get());
  bool ran_alarms = false;
  int64 finish_us = timer_->NowUs() + timeout_us;
  int64 next_wakeup_us = RunAlarms(&ran_alarms);
  if (timeout_us > 0 && !ran_alarms) {
    AwaitWakeup(std::min(finish_us, next_wakeup_us));
    RunAlarms(&ran_alarms);
  }
}

}  // namespace net_instaweb
