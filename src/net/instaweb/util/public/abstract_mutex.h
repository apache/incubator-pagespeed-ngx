/*
 * Copyright 2010 Google Inc.
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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_ABSTRACT_MUTEX_H_
#define NET_INSTAWEB_UTIL_PUBLIC_ABSTRACT_MUTEX_H_

#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

// Abstract interface for implementing a mutex.
class AbstractMutex {
 public:
  virtual ~AbstractMutex();
  virtual void Lock() = 0;
  virtual void Unlock() = 0;
  // Optionally checks that lock is held (for invariant checking purposes).
  // Default implementation does no checking.
  virtual void DCheckLocked();
};

// Helper class for lexically scoped mutexing.
class ScopedMutex {
 public:
  explicit ScopedMutex(AbstractMutex* mutex) : mutex_(mutex) {
    mutex_->Lock();
  }

  ~ScopedMutex() {
    mutex_->Unlock();
  }
 private:
  AbstractMutex* mutex_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMutex);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_ABSTRACT_MUTEX_H_
