// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/mock_time_condvar.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

MockTimeCondvar::MockTimeCondvar(ThreadSystem::Condvar* condvar,
                                 MockTimer* timer)
    : condvar_(condvar),
      timer_(timer),
      alarm_(NULL) {
}

MockTimeCondvar::~MockTimeCondvar() {
}

class MockTimeCondvarAlarm : public MockTimer::Alarm {
 public:
  MockTimeCondvarAlarm(int64 wakeup_time_us, MockTimeCondvar* condvar)
      : MockTimer::Alarm(wakeup_time_us),
        condvar_(condvar) {
  }
  virtual ~MockTimeCondvarAlarm() {}
  virtual void Run() { condvar_->AlarmFired(); }

 private:
  MockTimeCondvar* condvar_;
};

void MockTimeCondvar::TimedWait(int64 timeout_ms) {
  int64 timeout_us = 1000 * timeout_ms;
  int64 wakeup_time_us = timeout_us + timer_->NowUs();
  CHECK(alarm_ == NULL);
  alarm_ = new MockTimeCondvarAlarm(wakeup_time_us, this);
  timer_->AddAlarm(alarm_);
  condvar_->Wait();
  if (alarm_ != NULL) {
    timer_->CancelAlarm(alarm_);
    alarm_ = NULL;
  }
}

void MockTimeCondvar::AlarmFired() {
  ScopedMutex lock(mutex());
  if (alarm_ != NULL) {
    // TODO(jmarantz): Add advancement of time into RewriteContext so that we
    // can exploit this mechanism.
    //
    // For now, see MockTimeCondvarTest.WakeupOnAdvancementOfSimulatedTime
    // to see how this will work.
    alarm_ = NULL;
    condvar_->Signal();
  }
}

void MockTimeCondvar::Broadcast() {
  LOG(DFATAL) << "MockTimeCondvar::Broadcast is not yet implemented";
  condvar_->Broadcast();
}

}  // namespace net_instaweb
