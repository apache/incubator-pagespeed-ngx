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

#include "pagespeed/kernel/base/countdown_timer.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

CountdownTimer::CountdownTimer(Timer* timer,
                               void* user_data,
                               int64 allowed_time_ms) : timer_(timer),
                                                        user_data_(user_data),
                                                        limit_time_us_(0),
                                                        start_time_us_(0) {
  Reset(allowed_time_ms);
}

void CountdownTimer::Reset(int64 allowed_time_ms) {
  start_time_us_ = timer_->NowUs();
  limit_time_us_ = ((allowed_time_ms >= 0) && (timer_ != NULL)) ?
      start_time_us_ + 1000 * allowed_time_ms : 0;
}

bool CountdownTimer::HaveTimeLeft() const {
  return (limit_time_us_ == 0) || (timer_->NowUs() < limit_time_us_);
}

int64 CountdownTimer::TimeLeftMs() const {
  return (limit_time_us_ == 0) ? 0 :
      (limit_time_us_ - timer_->NowUs()) / 1000;
}

int64 CountdownTimer::TimeElapsedMs() const {
  return (timer_->NowUs() -  start_time_us_) / 1000;
}

}  // namespace net_instaweb
