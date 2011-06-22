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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIME_CONDVAR_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MOCK_TIME_CONDVAR_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class MockTimer;

// Implements a condition-variable whose TimedWait works with
// MockTime.  This is implemented on top of a real condition
// variable.
class MockTimeCondvar : public ThreadSystem::Condvar {
 public:
  MockTimeCondvar(ThreadSystem::Condvar* condvar, MockTimer* timer);
  virtual ~MockTimeCondvar();

  virtual ThreadSystem::CondvarCapableMutex* mutex() const {
    return condvar_->mutex();
  }

  // Broadcast is NYI in the mock system.
  virtual void Broadcast();
  virtual void Wait() { condvar_->Wait(); }
  virtual void Signal() { condvar_->Signal(); }

  // TimedWait should not be called on MockCondvar.  See
  // MockThreadSystem::TimedWait.  This call will CHECK-fail.
  virtual void TimedWait(int64 timeout_ms);

 private:
  scoped_ptr<ThreadSystem::Condvar> condvar_;
  MockTimer* timer_;

  DISALLOW_COPY_AND_ASSIGN(MockTimeCondvar);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CONDVAR_H_
