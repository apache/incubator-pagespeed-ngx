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

// Author: pulkitg@google.com (Pulkit Goyal)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_RW_MUTEX_H_
#define NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_RW_MUTEX_H_

#include <pthread.h>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

// Implementation of RWLock for Pthread mutexes.
class PthreadRWLock : public ThreadSystem::RWLock {
 public:
  PthreadRWLock();
  virtual ~PthreadRWLock();
  virtual void Lock();
  virtual void Unlock();
  virtual void ReaderLock();
  virtual void ReaderUnlock();

 private:
  pthread_rwlock_t rwlock_;
  pthread_rwlockattr_t attr_;

  DISALLOW_COPY_AND_ASSIGN(PthreadRWLock);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_RW_MUTEX_H_
