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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIMER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIMER_H_

#include <set>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class MockTimer : public Timer {
 public:
  // A useful recent time-constant for testing.
  static const int64 kApr_5_2010_ms;

  // Alarms provide a mechanism for tests employing mock timers
  // to get notified when a certain mock-time passes.  When a
  // MockTimer::set_time_us is called (or any of the other functions
  // that adjust time), time advances in phases broken up by outstanding
  // alarms, so that the system is seen in a consistent state.
  class Alarm {
   private:
    static const int kIndexUninitialized = -1;

   public:
    explicit Alarm(int64 wakeup_time_us)
        : index_(kIndexUninitialized),
          wakeup_time_us_(wakeup_time_us) {}
    virtual ~Alarm();

    virtual void Run() = 0;
    int64 wakeup_time_us() const { return wakeup_time_us_; }

    // Compares two alarms, giving a total deterministic ordering based
    // first, on wakeup time, and, in the event of two simultaneous
    // alarms, the order in which the Alarm was added.
    int Compare(const Alarm* that) const;

   private:
    friend class MockTimer;

    // Provides a mechanism to deterministically order alarms, even
    // if multiple alarms are scheduled for the same point in time.
    void SetIndex(int index_);

    int index_;
    int64 wakeup_time_us_;
    DISALLOW_COPY_AND_ASSIGN(Alarm);
  };

  // Sorting comparator for alarms.  This is public so that std::set
  // can see it.
  struct CompareAlarms {
    bool operator()(const Alarm* a, const Alarm* b) const {
      return a->Compare(b) < 0;
    }
  };

  explicit MockTimer(int64 time_ms);
  virtual ~MockTimer();

  // Sets the time as in microseconds, calling any outstanding alarms
  // with wakeup times up to and including time_us.
  void SetTimeUs(int64 time_us);

  // Advance forward time by the specified number of microseconds.
  void AdvanceUs(int64 delta_us) { SetTimeUs(time_us_ + delta_us); }

  // Advance time, in milliseconds.
  void AdvanceMs(int64 delta_ms) { AdvanceUs(1000 * delta_ms); }

  // Returns number of microseconds since 1970.
  virtual int64 NowUs() const { return time_us_; }
  virtual void SleepUs(int64 us) { AdvanceUs(us); }
  virtual void SleepMs(int64 ms) { AdvanceUs(1000 * ms); }

  // Schedules an alarm, called when the time is advanced to, or beyond,
  // alarm->wakeup_time_us().  Takes ownership of Alarm: it will be
  // deleted when it's finished.
  void AddAlarm(Alarm* alarm);

  // Cancels an outstanding alarm and deletes it.
  void CancelAlarm(Alarm* alarm);

 private:
  int64 time_us_;
  int next_index_;
  typedef std::set<Alarm*, CompareAlarms> AlarmSet;
  AlarmSet alarms_;
  bool setting_;

  DISALLOW_COPY_AND_ASSIGN(MockTimer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIMER_H_
