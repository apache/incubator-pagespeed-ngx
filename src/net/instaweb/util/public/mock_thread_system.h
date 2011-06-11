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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_MOCK_THREAD_SYSTEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_MOCK_THREAD_SYSTEM_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class MockTimer;

// Thread System wrapper used to help build tests and debugging
// environments with deterministic behavior.  When MockThreadSystem is
// constructed, it makes its MockTimer* variable thread-safe by
// injecting a real mutex.
//
// It should be noted that the MockThreadSystem uses real threads --
// it just allows condition-variables to be created that will work
// in mock-time.
class MockThreadSystem : public ThreadSystem {
 public:
  MockThreadSystem(ThreadSystem* thread_system, MockTimer* mock_timer);
  virtual ~MockThreadSystem();
  virtual CondvarCapableMutex* NewMutex();
  virtual ThreadImpl* NewThreadImpl(Thread* wrapper, ThreadFlags flags);

 private:
  ThreadSystem* thread_system_;
  MockTimer* mock_timer_;

  DISALLOW_COPY_AND_ASSIGN(MockThreadSystem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_MOCK_THREAD_SYSTEM_H_
