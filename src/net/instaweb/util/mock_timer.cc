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

#include "net/instaweb/util/public/mock_timer.h"

#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_mutex.h"
#include "net/instaweb/util/public/scoped_ptr.h"

namespace net_instaweb {

const int64 MockTimer::kApr_5_2010_ms = 1270493486000LL;

MockTimer::MockTimer(int64 time_ms)
    : time_us_(1000 * time_ms),
      mutex_(new NullMutex),
      next_delta_(0) {
}

MockTimer::~MockTimer() {
}

void MockTimer::SetTimeUs(int64 new_time_us) {
  mutex_->Lock();

  // If an Alarm::Run function moved us forward in time, don't move us back.
  if (time_us_ < new_time_us) {
    time_us_ = new_time_us;
  }
  mutex_->Unlock();
}

int64 MockTimer::NowUs() const {
  ScopedMutex lock(mutex_.get());
  if (next_delta_ < deltas_us_.size()) {
    const_cast<MockTimer*>(this)->AdvanceUs(deltas_us_[next_delta_++]);
  }
  return time_us_;
}

}  // namespace net_instaweb
