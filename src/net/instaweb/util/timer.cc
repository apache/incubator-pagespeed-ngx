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

#include "net/instaweb/util/public/timer.h"

#include "base/logging.h"
#include <sys/time.h>
#include <time.h>
#include "pagespeed/core/resource_util.h"

namespace net_instaweb {

const int64 Timer::kSecondMs = 1000;
const int64 Timer::kMinuteMs =   60 * Timer::kSecondMs;
const int64 Timer::kHourMs   =   60 * Timer::kMinuteMs;
const int64 Timer::kDayMs    =   24 * Timer::kHourMs;
const int64 Timer::kWeekMs   =    7 * Timer::kDayMs;
const int64 Timer::kMonthMs  =   31 * Timer::kDayMs;
const int64 Timer::kYearMs   =  365 * Timer::kDayMs;

class RealSystemTimer : public Timer {
 public:
  RealSystemTimer() {
  }

  virtual ~RealSystemTimer() {
  }

  virtual int64 NowMs() const {
    // TODO(jmarantz): use pagespeed library instead of replicating
    // this code.
    //
    // I had attempted to use this from the Chromium snapshot in pagespeed:
    //   src/third_party/chromium/src/base/time.h
    // but evidently that file is not currently built in the .gyp
    // file from pagespeed.
    //
    // double now = base::Time::NowFromSystemTime().ToDoubleT();

    struct timeval tv;
    struct timezone tz = { 0, 0 };  // UTC
    if (gettimeofday(&tv, &tz) != 0) {
      // TODO(sligocki): ... what is this?
      CHECK(0 == "Could not determine time of day");
    }
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
  }

 private:
};

Timer::~Timer() {
}

Timer* Timer::NewSystemTimer() {
  return new RealSystemTimer();
}

bool Timer::ParseTime(const char* time_str, int64* time_ms) {
  return pagespeed::resource_util::ParseTimeValuedHeader(time_str, time_ms);
}

}  // namespace net_instaweb
