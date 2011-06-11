/*
 * Copyright 2011 Google Inc.
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
//
// This mock thread system uses a real ThreadSystem to create a testable
// multi-threaded environment with mock timer.
//
// All the mechanisms to create threads and mutexes basically delegate to
// the ThreadSystem implementation provided in the MockThradSystem constructor,
// except the we overlay a mock_time_condvar, which wakes up based on the
// advancment of a MockTimer.

#include "net/instaweb/util/public/mock_thread_system.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/mock_time_condvar.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

MockThreadSystem::MockThreadSystem(ThreadSystem* thread_system,
                                   MockTimer* mock_timer)
    : thread_system_(thread_system),
      mock_timer_(mock_timer) {
  mock_timer_->set_mutex(NewMutex());
}

MockThreadSystem::~MockThreadSystem() {
}

class MockCondvarCapableMutex : public ThreadSystem::CondvarCapableMutex {
 public:
  MockCondvarCapableMutex(MockTimer* timer, CondvarCapableMutex* mutex)
      : timer_(timer),
        mutex_(mutex) {
  }
  virtual ~MockCondvarCapableMutex() {}
  virtual ThreadSystem::Condvar* NewCondvar() {
    return new MockTimeCondvar(mutex_->NewCondvar(), timer_);
  }
  virtual void Lock() { mutex_->Lock(); }
  virtual void Unlock() { mutex_->Unlock(); }

 private:
  MockTimer* timer_;
  scoped_ptr<CondvarCapableMutex> mutex_;
};

ThreadSystem::CondvarCapableMutex* MockThreadSystem::NewMutex() {
  return new MockCondvarCapableMutex(mock_timer_, thread_system_->NewMutex());
}

ThreadSystem::ThreadImpl* MockThreadSystem::NewThreadImpl(
    Thread* wrapper, ThreadFlags flags) {
  return thread_system_->NewThreadImpl(wrapper, flags);
}

}  // namespace net_instaweb
