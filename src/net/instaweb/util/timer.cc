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

namespace net_instaweb {

const int64 Timer::kSecondMs = 1000;
const int64 Timer::kMinuteMs =   60 * Timer::kSecondMs;
const int64 Timer::kHourMs   =   60 * Timer::kMinuteMs;
const int64 Timer::kDayMs    =   24 * Timer::kHourMs;
const int64 Timer::kWeekMs   =    7 * Timer::kDayMs;
const int64 Timer::kMonthMs  =   31 * Timer::kDayMs;
const int64 Timer::kYearMs   =  365 * Timer::kDayMs;

Timer::~Timer() {
}

int64 Timer::NowMs() const {
  return NowUs() / 1000;
}

}  // namespace net_instaweb
