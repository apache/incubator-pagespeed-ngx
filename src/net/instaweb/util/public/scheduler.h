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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SCHEDULER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SCHEDULER_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class AbstractMutex;

// Implements a simple schedule that allows a thread to block until either
// time expires, or a condition variable is signaled.  The default
// implementation uses a condition variable to block the calling thread
// until one of these occurs.  However this implementation can be overridden,
// e.g. see MockScheduler.
class Scheduler {
 public:
  explicit Scheduler(ThreadSystem* thread_system);
  virtual ~Scheduler();

  AbstractMutex* mutex() { return mutex_.get(); }
  ThreadSystem::Condvar* condvar() { return condvar_.get(); }

  // mutex() must be held when calling TimedWait.
  virtual void TimedWait(int64 timeout_ms);

  // mutex() must be held when calling TimedWait.
  void Signal() { condvar_->Signal(); }

 private:
  ThreadSystem* thread_system_;
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> condvar_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SCHEDULER_H_
