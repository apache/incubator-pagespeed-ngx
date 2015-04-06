/*
 * Copyright 2013 Google Inc.
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

// Author: vchudnov@google.com (Victor Chudnovsky)

#ifndef PAGESPEED_KERNEL_BASE_COUNTDOWN_TIMER_H_
#define PAGESPEED_KERNEL_BASE_COUNTDOWN_TIMER_H_

#include <time.h>
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class Timer;

// Once initialized with a non-negative interval 'allowed_time_ms' via
// the ctor or Reset(), repeated calls to HaveTimeLeft() will return
// true only until the interval has elapsed. If the interval is
// negative number, HaveTimeLeft() always returns true.
class CountdownTimer {
 public:
  CountdownTimer(Timer* timer,
                 void* user_data,
                 int64 allowed_time_ms);

  void Reset(int64 allowed_time_ms);

  bool HaveTimeLeft() const;

  void* user_data() const { return user_data_; }

  int64 TimeLeftMs() const;

  int64 TimeElapsedMs() const;

 private:
  Timer* timer_;
  void* user_data_;
  clock_t limit_time_us_;
  clock_t start_time_us_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_COUNTDOWN_TIMER_H_
