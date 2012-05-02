/*
 * Copyright 2012 Google Inc.
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_NULL_RW_LOCK_H_
#define NET_INSTAWEB_UTIL_PUBLIC_NULL_RW_LOCK_H_

#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

// Implements an empty mutex for single-threaded programs that need to work
// with interfaces that require mutexes.
class NullRWLock : public ThreadSystem::RWLock {
 public:
  NullRWLock() {}
  virtual ~NullRWLock();
  virtual void Lock();
  virtual void Unlock();
  virtual void ReaderLock();
  virtual void ReaderUnlock();
  virtual void DCheckReaderLocked();
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_NULL_RW_LOCK_H_
