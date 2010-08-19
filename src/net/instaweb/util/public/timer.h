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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_TIMER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_TIMER_H_

#include "base/basictypes.h"

namespace net_instaweb {

// Timer interface, made virtual so it can be mocked for tests.
class Timer {
 public:
  static const int64 kSecondMs;
  static const int64 kMinuteMs;
  static const int64 kHourMs;
  static const int64 kDayMs;
  static const int64 kWeekMs;
  static const int64 kMonthMs;
  static const int64 kYearMs;

  virtual ~Timer();

  // Returns number of milliseconds since 1970.
  virtual int64 NowMs() const;

  // Returns number of microseconds since 1970.
  virtual int64 NowUs() const = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_TIMER_H_
