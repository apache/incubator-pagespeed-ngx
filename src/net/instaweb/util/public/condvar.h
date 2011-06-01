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
// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CONDVAR_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CONDVAR_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

// Abstract interface for implementing a condition variable layered on top of a
// given mutex type, which ought to extend CondvarCapableMutex.
class ThreadSystem::Condvar {
 public:
  Condvar() { }
  virtual ~Condvar();

  // Return the mutex associated with this condition variable.
  virtual const CondvarCapableMutex* mutex() const = 0;

  // Signal the condvar, waking a waiting thread if any.  mutex() must be held
  // by caller.  Example:
  // {
  //   ScopedMutex lock(cv.mutex());
  //   make_resource_available();
  //   cv.Signal();
  // }
  virtual void Signal() = 0;

  // Broadcast to all threads waiting on condvar.  mutex() must be held as with
  // Signal().
  virtual void Broadcast() = 0;

  // Wait for condition to be signaled.  mutex() must be held; it will
  // be released and then reclaimed when a signal is received.  Note
  // that a Wait() may be terminated based on a condition being true,
  // but the condition may no longer be true at the time the thread
  // wakes up.  Example:
  // {
  //   ScopedMutex lock(cv.mutex());
  //   while (status && !resource_available()) status = cv.wait();
  //   if (status) {
  //     use_resource();
  //   }
  // }
  virtual void Wait() = 0;

  // Wait for condition to be signaled, or timeout to occur.  Works like Wait(),
  // and cv.mutex() must be held on entry and re-taken on exit.
  virtual void TimedWait(int64 timeout_ms) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Condvar);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CONDVAR_H_
