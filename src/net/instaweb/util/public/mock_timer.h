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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIMER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIMER_H_

#include "base/basictypes.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class MockTimer : public Timer {
 public:
  // A useful recent time-constant for testing.
  static const int64 kApr_5_2010_ms;

  explicit MockTimer(int64 time_ms) : time_ms_(time_ms) {}
  virtual ~MockTimer();

  // Returns number of microseconds since 1970.
  virtual int64 NowUs() const;

  void set_time_ms(int64 time_ms) { time_ms_ = time_ms; }
  void advance_ms(int64 delta_ms) { time_ms_ += delta_ms; }

 private:
  int64 time_ms_;

  DISALLOW_COPY_AND_ASSIGN(MockTimer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIMER_H_
