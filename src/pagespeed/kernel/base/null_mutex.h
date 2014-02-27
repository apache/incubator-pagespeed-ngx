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

#ifndef PAGESPEED_KERNEL_BASE_NULL_MUTEX_H_
#define PAGESPEED_KERNEL_BASE_NULL_MUTEX_H_

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/thread_annotations.h"

namespace net_instaweb {

// Implements an empty mutex for single-threaded programs that need to work
// with interfaces that require mutexes.
class LOCKABLE NullMutex : public AbstractMutex {
 public:
  NullMutex() {}
  virtual ~NullMutex();
  virtual bool TryLock() EXCLUSIVE_TRYLOCK_FUNCTION(true);
  virtual void Lock() EXCLUSIVE_LOCK_FUNCTION();
  virtual void Unlock() UNLOCK_FUNCTION();
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_NULL_MUTEX_H_
