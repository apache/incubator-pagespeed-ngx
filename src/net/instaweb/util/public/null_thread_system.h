/*
 * Copyright 2013 Google Inc.
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
// Zero-dependency mock thread-system for use in tests that don't
// actually use threads, to help test classes that need some mutexing
// or other thread-safety hooks.
//
// Note that this thread-system does not currenlty make threads (even
// co-routines), but check-fails if you attempt to spawn a new thread.
//
// Also, currently, there are no fake condvars, and the methods that
// create condvars will check-fail.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_NULL_THREAD_SYSTEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_NULL_THREAD_SYSTEM_H_

#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class Timer;

// Mock thread system.  This can create mutexes that do no locking,
// but currently cannot create any threads or condvars -- doing so
// will result in a fatal error.
class NullThreadSystem : public ThreadSystem {
 public:
  NullThreadSystem() {}
  virtual ~NullThreadSystem();
  virtual CondvarCapableMutex* NewMutex();
  virtual RWLock* NewRWLock();
  virtual Timer* NewTimer();
  virtual ThreadId* GetThreadId() const;

 private:
  virtual ThreadImpl* NewThreadImpl(Thread* wrapper, ThreadFlags flags);

 private:
  DISALLOW_COPY_AND_ASSIGN(NullThreadSystem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_NULL_THREAD_SYSTEM_H_
