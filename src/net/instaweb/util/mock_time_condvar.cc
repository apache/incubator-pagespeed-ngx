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
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class MockTimer;

MockTimeCondvar::MockTimeCondvar(ThreadSystem::Condvar* condvar,
                                 MockTimer* timer)
    : condvar_(condvar),
      timer_(timer) {
}

MockTimeCondvar::~MockTimeCondvar() {
}

void MockTimeCondvar::Broadcast() {
  LOG(DFATAL) << "MockTimeCondvar::Broadcast is not yet implemented";
  condvar_->Broadcast();
}

void MockTimeCondvar::TimedWait(int64 timeout_ms) {
  LOG(DFATAL) << "TimedWait should not be called on MockCondvar.  See "
              << "MockThreadSystem::TimedWait.";
  condvar_->TimedWait(timeout_ms);
}

}  // namespace net_instaweb
