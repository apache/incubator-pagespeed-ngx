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

#ifndef PAGESPEED_KERNEL_BASE_MOCK_TIMER_H_
#define PAGESPEED_KERNEL_BASE_MOCK_TIMER_H_

#include <vector>                       // for vector

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

class AbstractMutex;
class Function;

class MockTimer : public Timer {
 public:
  typedef void (*Callback)(void* user_data);

  // A useful recent time-constant for testing.
  static const int64 kApr_5_2010_ms;

  // Takes ownership of mutex.
  MockTimer(AbstractMutex* mutex, int64 time_ms);
  virtual ~MockTimer();

  // Sets the time as in microseconds, calling any outstanding alarms
  // with wakeup times up to and including time_us.
  void SetTimeUs(int64 new_time_us);
  void SetTimeMs(int64 new_time_ms) { SetTimeUs(1000 * new_time_ms); }

  // Advance forward time by the specified number of microseconds.
  void AdvanceUs(int64 delta_us) { SetTimeUs(time_us_ + delta_us); }

  // Advance time, in milliseconds.
  void AdvanceMs(int64 delta_ms) { AdvanceUs(1000 * delta_ms); }

  // Set time advances in microseconds for the next calls to NowUs/NowMs.
  void SetTimeDeltaUs(int64 delta_us) {
    SetTimeDeltaUsWithCallback(delta_us, NULL);
  }

  // Set time advances in microseconds for the next calls to
  // NowUs/NowMs, with the corresponding callback to execute right
  // before that time is returned.
  void SetTimeDeltaUsWithCallback(int64 delta_us,
                                  Function* callback);

  // Set time advances in milliseconds for the next calls to NowUs/NowMs.
  void SetTimeDeltaMs(int64 delta_ms) { SetTimeDeltaUs(1000 * delta_ms); }

  // Returns number of microseconds since 1970.
  virtual int64 NowUs() const;
  virtual void SleepUs(int64 us) { AdvanceUs(us); }
  virtual void SleepMs(int64 ms) { AdvanceUs(1000 * ms); }

 private:
  typedef struct {
    int64 time;
    Function* callback;
  } TimeAndCallback;
  mutable int64 time_us_;
  scoped_ptr<AbstractMutex> mutex_;
  std::vector<TimeAndCallback> deltas_us_;
  mutable unsigned int next_delta_;

  DISALLOW_COPY_AND_ASSIGN(MockTimer);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_MOCK_TIMER_H_
