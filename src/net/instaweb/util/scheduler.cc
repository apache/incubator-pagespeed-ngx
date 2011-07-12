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

#include "net/instaweb/util/public/scheduler.h"
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/condvar.h"

namespace net_instaweb {

Scheduler::Scheduler(ThreadSystem* thread_system)
    : thread_system_(thread_system),
      mutex_(thread_system->NewMutex()),
      condvar_(mutex_->NewCondvar()) {
}

Scheduler::~Scheduler() {
}

void Scheduler::TimedWait(int64 timeout_ms) {
  return condvar_->TimedWait(timeout_ms);
}

}  // namespace net_instaweb
