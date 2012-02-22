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
// Authors: jmarantz@google.com (Joshua Marantz)
//          jmaessen@google.com (Jan Maessen)

#include "net/instaweb/util/public/scheduler.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

namespace {

const int kIndexNotSet = 0;

}  // namespace

// Basic Alarm type (forward declared in the .h file).  Note that
// Alarms are self-cleaning; it is not valid to make use of an Alarm*
// after RunAlarm() or CancelAlarm() has been called.  See note below
// for AddAlarm.  Note also that Alarms hold the scheduler lock when
// they are invoked; the alarm drops the lock before invoking its
// embedded callback and re-takes it afterwards if that is necessary.
class Scheduler::Alarm {
 public:
  virtual void RunAlarm() = 0;
  virtual void CancelAlarm() = 0;

  // Compare two alarms, based on wakeup time and insertion order.  Result
  // like strcmp (<0 for this < that, >0 for this > that), based on wakeup
  // time and index.
  int Compare(const Alarm* other) const {
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

  bool in_wait_dispatch() const { return in_wait_dispatch_; }
  void set_in_wait_dispatch(bool w) { in_wait_dispatch_ = w; }

 protected:
  Alarm() : wakeup_time_us_(0),
            index_(kIndexNotSet),
            in_wait_dispatch_(false) { }
  virtual ~Alarm() { }

 private:
  friend class Scheduler;
  int64 wakeup_time_us_;
  uint32 index_;  // Set by scheduler to disambiguate equal wakeup times.

  // This is used to mark a wait alarm that's being considered by ::Signal
  // as owned by it for purposes of cleanup, so any concurrent timeout will
  // know not to delete it.
  bool in_wait_dispatch_;
  DISALLOW_COPY_AND_ASSIGN(Alarm);
};

namespace {

// private class to encapsulate a function being
// scheduled as an alarm.  Owns passed-in function.
class FunctionAlarm : public Scheduler::Alarm {
 public:
  explicit FunctionAlarm(Function* function, Scheduler* scheduler)
      : scheduler_(scheduler), function_(function) { }
  virtual ~FunctionAlarm() { }

  virtual void RunAlarm() {
    DropMutexActAndCleanup(&Function::CallRun);
  }
  virtual void CancelAlarm() {
    DropMutexActAndCleanup(&Function::CallCancel);
  }
 private:
  typedef void (Function::*FunctionAction)();
  void DropMutexActAndCleanup(FunctionAction act) {
    AbstractMutex* mutex = scheduler_->mutex();  // Save across delete.
    mutex->Unlock();
    ((function_)->*(act))();
    delete this;
    mutex->Lock();
  }
  Scheduler* scheduler_;
  Function* function_;
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
  virtual void RunAlarm() {
    *set_on_timeout_ = true;
    scheduler_->CancelWaiting(this);
    if (!in_wait_dispatch()) {
      delete this;
    }
  }
  virtual void CancelAlarm() {
    DCHECK(in_wait_dispatch());
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
  virtual void RunAlarm() {
    // We may get deleted at tail end of Signal if the lock gets dropped during
    // CallRun(), so save this into a local.
    bool saved_in_wait_dispatch = in_wait_dispatch();
    scheduler_->CancelWaiting(this);
    callback_->CallRun();
    if (!saved_in_wait_dispatch) {
      delete this;
    }
  }
  virtual void CancelAlarm() {
    DCHECK(in_wait_dispatch());
    callback_->CallRun();
    delete this;
  }
 private:
  Function* callback_;
  Scheduler* scheduler_;
  DISALLOW_COPY_AND_ASSIGN(CondVarCallbackTimeout);
};

// Comparison on Alarms.
bool Scheduler::CompareAlarms::operator()(const Alarm* a,
                                          const Alarm* b) const {
  return a->Compare(b) < 0;
}

Scheduler::Scheduler(ThreadSystem* thread_system, Timer* timer)
    : thread_system_(thread_system),
      timer_(timer),
      mutex_(thread_system->NewMutex()),
      condvar_(mutex_->NewCondvar()),
      index_(kIndexNotSet),
      signal_count_(0),
      running_waiting_alarms_(false) {
}

Scheduler::~Scheduler() {
}

ThreadSystem::CondvarCapableMutex* Scheduler::mutex() {
  return mutex_.get();
}

void Scheduler::DCheckLocked() { mutex_->DCheckLocked(); }

void Scheduler::BlockingTimedWait(int64 timeout_ms) {
  mutex_->DCheckLocked();
  int64 now_us = timer_->NowUs();
  int64 wakeup_time_us = now_us + timeout_ms * Timer::kMsUs;
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
    // stop when outstanding_alarms_ is empty (and thus RunAlarms(NULL) == 0) as
    // a belt and suspenders protection against programmer error; this ought to
    // imply timed_out.
    AwaitWakeupUntilUs(std::min(wakeup_time_us, next_wakeup_us));
    next_wakeup_us = RunAlarms(NULL);
  }
}

void Scheduler::TimedWait(int64 timeout_ms, Function* callback) {
  mutex_->DCheckLocked();
  int64 now_us = timer_->NowUs();
  int64 completion_time_us = now_us + timeout_ms * Timer::kMsUs;
  // We create the alarm for this callback, and register it.  We also register
  // the alarm with the signal queue, where the callback will be run on
  // cancellation.
  CondVarCallbackTimeout* alarm = new CondVarCallbackTimeout(callback, this);
  AddAlarmMutexHeld(completion_time_us, alarm);
  waiting_alarms_.insert(alarm);
  RunAlarms(NULL);
}

void Scheduler::CancelWaiting(Alarm* alarm) {
  // Called to clean up a [Blocking]TimedWait that timed out.  There used to be
  // a benign race here that meant alarm had been erased from waiting_alarms_ by
  // a pending Signal operation.  Tighter locking on Alarm objects should have
  // eliminated this hole, but we continue to use presence / absence in
  // outstanding_alarms_ to resolve signal/cancel races.
  mutex_->DCheckLocked();
  waiting_alarms_.erase(alarm);
}

void Scheduler::Signal() {
  mutex_->DCheckLocked();
  ++signal_count_;
  // We have to be careful to not just walk over waiting_alarms_ here
  // as new entries can be added to it by TimedWait invocations from the
  // callbacks we run.
  AlarmSet waiting_alarms_to_dispatch;
  waiting_alarms_to_dispatch.swap(waiting_alarms_);
  running_waiting_alarms_ = true;
  if (!waiting_alarms_to_dispatch.empty()) {
    // First, mark them all as owned by us, so any concurrent timeouts
    // that happen while we're releasing the lock to run user code
    // do not delete them from under us.
    for (AlarmSet::iterator i = waiting_alarms_to_dispatch.begin();
         i != waiting_alarms_to_dispatch.end(); ++i) {
      (*i)->set_in_wait_dispatch(true);
    }

    // Now actually signal those that didn't timeout yet.
    for (AlarmSet::iterator i = waiting_alarms_to_dispatch.begin();
         i != waiting_alarms_to_dispatch.end(); ++i) {
      if (!CancelAlarm(*i)) {
        // If CancelAlarm returned false, this means the alarm actually
        // got run by a timeout. In that case, since we set in_wait_dispatch
        // to true, it deferred the deletion to us, so take care of it.
        delete *i;
      }
    }
  }
  condvar_->Broadcast();
  running_waiting_alarms_ = false;
  RunAlarms(NULL);
}

// Add alarm while holding mutex.  Don't run any alarms or otherwise drop mutex.
void Scheduler::AddAlarmMutexHeld(int64 wakeup_time_us, Alarm* alarm) {
  mutex_->DCheckLocked();
  alarm->wakeup_time_us_ = wakeup_time_us;
  alarm->index_ = ++index_;
  // Someone may care about changes in wait time.  Broadcast if any occurred.
  if (!outstanding_alarms_.empty()) {
    Alarm* first_alarm = *outstanding_alarms_.begin();
    if (wakeup_time_us < first_alarm->wakeup_time_us_) {
      condvar_->Broadcast();
    }
  } else {
    condvar_->Broadcast();
  }
  outstanding_alarms_.insert(alarm);
}

Scheduler::Alarm* Scheduler::AddAlarm(int64 wakeup_time_us,
                                      Function* callback) {
  Alarm* result = new FunctionAlarm(callback, this);
  ScopedMutex lock(mutex_.get());
  AddAlarmMutexHeld(wakeup_time_us, result);
  RunAlarms(NULL);
  return result;
}

bool Scheduler::CancelAlarm(Alarm* alarm) {
  mutex_->DCheckLocked();
  if (outstanding_alarms_.erase(alarm) != 0) {
    // Note: the following call may drop and re-lock the scheduler mutex.
    alarm->CancelAlarm();
    return true;
  } else {
    return false;
  }
}

// Run any alarms that have reached their deadline.  Requires that we hold
// mutex_ before calling.  Returns the time of the next deadline, or 0 if no
// further deadlines loom.  Sets *ran_alarms if non-NULL and any alarms were
// run, otherwise leaves it untouched.
int64 Scheduler::RunAlarms(bool* ran_alarms) {
  while (!outstanding_alarms_.empty()) {
    mutex_->DCheckLocked();
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
    // Note that the following call may drop and re-lock the scheduler lock.
    first_alarm->RunAlarm();
  }
  return 0;
}

void Scheduler::AwaitWakeupUntilUs(int64 wakeup_time_us) {
  mutex_->DCheckLocked();
  int64 now_us = timer_->NowUs();
  if (wakeup_time_us > now_us) {
    // Compute how long we should wait.  Note: we overshoot, which may lead us
    // to wake a bit later than expected.  We assume the system is likely to
    // round wakeup time off for us in some arbitrary fashion in any case.
    int64 wakeup_interval_ms =
        (wakeup_time_us - now_us + Timer::kMsUs - 1) / Timer::kMsUs;
    condvar_->TimedWait(wakeup_interval_ms);
  }
}

void Scheduler::Wakeup() {
  condvar_->Broadcast();
}

void Scheduler::ProcessAlarms(int64 timeout_us) {
  mutex_->DCheckLocked();
  bool ran_alarms = false;
  int64 finish_us = timer_->NowUs() + timeout_us;
  int64 next_wakeup_us = RunAlarms(&ran_alarms);

  if (timeout_us > 0 && !ran_alarms) {
    // Note: next_wakeup_us may be 0 here.
    if (next_wakeup_us == 0 || next_wakeup_us > finish_us) {
      next_wakeup_us = finish_us;
    }
    AwaitWakeupUntilUs(next_wakeup_us);

    RunAlarms(&ran_alarms);
  }
}

// For testing purposes, let a tester know when the scheduler has quiesced.
bool Scheduler::NoPendingAlarms() {
  mutex_->DCheckLocked();
  return (outstanding_alarms_.empty());
}

SchedulerBlockingFunction::SchedulerBlockingFunction(Scheduler* scheduler)
    : scheduler_(scheduler), success_(false) {
  set_delete_after_callback(false);
}

SchedulerBlockingFunction::~SchedulerBlockingFunction() { }

void SchedulerBlockingFunction::Run() {
  success_ = true;
  Cancel();
}

void SchedulerBlockingFunction::Cancel() {
  done_.set_value(true);
  scheduler_->Wakeup();
}

bool SchedulerBlockingFunction::Block() {
  ScopedMutex lock(scheduler_->mutex());
  while (!done_.value()) {
    scheduler_->ProcessAlarms(10 * Timer::kSecondUs);
  }
  return success_;
}

void Scheduler::RegisterWorker(QueuedWorkerPool::Sequence* w) {}
void Scheduler::UnregisterWorker(QueuedWorkerPool::Sequence* w) {}

}  // namespace net_instaweb
