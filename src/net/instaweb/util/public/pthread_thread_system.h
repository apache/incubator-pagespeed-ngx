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

// Author: morlovich@google.com (Maksim Orlovich)
//
// Implementation of thread-creation for pthreads

#ifndef NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_THREAD_SYSTEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_THREAD_SYSTEM_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class PthreadThreadSystem : public ThreadSystem {
 public:
  PthreadThreadSystem();
  virtual ~PthreadThreadSystem();

  virtual CondvarCapableMutex* NewMutex();

  virtual RWLock* NewRWLock();

 protected:
  // This hook will get invoked by the implementation in the context of a
  // thread before invoking its Run() method.
  virtual void BeforeThreadRunHook();

 private:
  friend class PthreadThreadImpl;

  virtual ThreadImpl* NewThreadImpl(Thread* wrapper, ThreadFlags flags);

  DISALLOW_COPY_AND_ASSIGN(PthreadThreadSystem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_THREAD_SYSTEM_H_
