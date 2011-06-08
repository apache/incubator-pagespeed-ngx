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
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/stl_util.h"

namespace net_instaweb {

const int64 MockTimer::kApr_5_2010_ms = 1270493486000LL;

MockTimer::MockTimer(int64 time_ms)
    : time_us_(1000 * time_ms),
      next_index_(0),
      setting_(false) {
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
  // TODO(jmarantz): it might be necessary to support reentrant
  // adjustment of the alarm-sets, but it is NYI.  For now, DCHECK.
  DCHECK(!setting_) << "We do not expect to add an alarm from "
      "inside a call to Alarm->Run().";
  alarm->SetIndex(next_index_++);
  size_t prev_count = alarms_.size();
  alarms_.insert(alarm);
  CHECK_EQ(1 + prev_count, alarms_.size());
}

void MockTimer::CancelAlarm(Alarm* alarm) {
  DCHECK(!setting_) << "We do not expect to cancel an alarm from "
      "inside a call to Alarm->Run().";
  int erased = alarms_.erase(alarm);
  if (erased == 1) {
    delete alarm;
  } else {
    LOG(DFATAL) << "Canceled alarm not found";
  }
}

void MockTimer::SetTimeUs(int64 time_us) {
  CHECK(!setting_) << "We do not expect to set the time from "
      "inside a call to Alarm->Run().";
  setting_ = true;
  for (AlarmSet::iterator p = alarms_.begin(), e = alarms_.end(); p != e; ) {
    Alarm* alarm = *p;
    if (time_us < alarm->wakeup_time_us()) {
      break;
    } else {
      AlarmSet::iterator iter_to_erase = p;
      ++p;
      alarms_.erase(iter_to_erase);
      time_us_ = alarm->wakeup_time_us();
      alarm->Run();
      delete alarm;
    }
  }
  time_us_ = time_us;
  setting_ = false;
}

}  // namespace net_instaweb
