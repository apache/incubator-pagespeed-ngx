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

#include "pagespeed/kernel/base/mock_timer.h"

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/function.h"

namespace net_instaweb {

const int64 MockTimer::kApr_5_2010_ms = 1270493486000LL;

MockTimer::MockTimer(int64 time_ms)
    : time_us_(1000 * time_ms),
      mutex_(new NullMutex),
      next_delta_(0) {
}

MockTimer::~MockTimer() {
  while (next_delta_ < deltas_us_.size()) {
    TimeAndCallback time_and_callback = deltas_us_[next_delta_++];
    if (time_and_callback.callback) {
      time_and_callback.callback->CallCancel();
    }
  }
}

void MockTimer::SetTimeUs(int64 new_time_us) {
  mutex_->Lock();

  // If an Alarm::Run function moved us forward in time, don't move us back.
  if (time_us_ < new_time_us) {
    time_us_ = new_time_us;
  }
  mutex_->Unlock();
}

void MockTimer::SetTimeDeltaUsWithCallback(int64 delta_us,
                                           Function* callback) {
  TimeAndCallback time_and_callback;
  time_and_callback.time = delta_us;
  time_and_callback.callback = callback;
  {
    ScopedMutex lock(mutex_.get());
    deltas_us_.push_back(time_and_callback);
  }
}

int64 MockTimer::NowUs() const {
  ScopedMutex lock(mutex_.get());
  if (next_delta_ < deltas_us_.size()) {
    TimeAndCallback time_and_callback = deltas_us_[next_delta_++];
    if (time_and_callback.callback) {
      time_and_callback.callback->CallRun();
    }
    const_cast<MockTimer*>(this)->AdvanceUs(time_and_callback.time);
  }
  return time_us_;
}

}  // namespace net_instaweb
