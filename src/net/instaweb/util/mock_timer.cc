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

#include <cstddef>
#include <set>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/null_mutex.h"
#include "net/instaweb/util/public/stl_util.h"

namespace net_instaweb {

const int64 MockTimer::kApr_5_2010_ms = 1270493486000LL;

MockTimer::MockTimer(int64 time_ms)
    : time_us_(1000 * time_ms),
      next_index_(0),
      mutex_(new NullMutex) {
}

MockTimer::~MockTimer() {
  STLDeleteElements(&alarms_);
}

MockTimer::Alarm::~Alarm() {
}

int MockTimer::Alarm::Compare(const Alarm* that) const {
  int cmp = 0;
  if (wakeup_time_us_ < that->wakeup_time_us_) {
    cmp = -1;
  } else if (wakeup_time_us_ > that->wakeup_time_us_) {
    cmp = 1;
  } else if (index_ < that->index_) {
    cmp = -1;
  } else if (index_ > that->index_) {
    cmp = 1;
  }
  return cmp;
}

void MockTimer::Alarm::SetIndex(int index) {
  CHECK_EQ(kIndexUninitialized, index_);
  index_ = index;
}

void MockTimer::AddAlarm(Alarm* alarm) {
  ScopedMutex lock(mutex_.get());
  alarm->SetIndex(next_index_++);
  size_t prev_count = alarms_.size();
  alarms_.insert(alarm);
  CHECK_EQ(1 + prev_count, alarms_.size());
}

void MockTimer::CancelAlarm(Alarm* alarm) {
  ScopedMutex lock(mutex_.get());
  int erased = alarms_.erase(alarm);
  if (erased == 1) {
    delete alarm;
  } else {
    LOG(DFATAL) << "Canceled alarm not found";
  }
}

void MockTimer::SetTimeUs(int64 time_us) {
  mutex_->Lock();
  while (!alarms_.empty()) {
    AlarmOrderedSet::iterator p = alarms_.begin();
    Alarm* alarm = *p;
    if (time_us < alarm->wakeup_time_us()) {
      break;
    } else {
      alarms_.erase(p);
      time_us_ = alarm->wakeup_time_us();
      mutex_->Unlock();
      alarm->Run();
      delete alarm;
      mutex_->Lock();
    }
  }

  // If an Alarm::Run function moved us forward in function,
  // don't move us back.
  if (time_us_ < time_us) {
    time_us_ = time_us;
  }

  mutex_->Unlock();
}

}  // namespace net_instaweb
