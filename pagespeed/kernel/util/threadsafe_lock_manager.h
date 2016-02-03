/*
 * Copyright 2015 Google Inc.
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

#ifndef PAGESPEED_KERNEL_UTIL_THREADSAFE_LOCK_MANAGER_H_
#define PAGESPEED_KERNEL_UTIL_THREADSAFE_LOCK_MANAGER_H_

#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/thread/scheduler.h"

namespace net_instaweb {

// Threadsafe wrapper for a non-threadsafe lock manager: MemLockManager.
class ThreadSafeLockManager : public NamedLockManager {
 public:
  explicit ThreadSafeLockManager(Scheduler* scheduler);
  virtual ~ThreadSafeLockManager();
  virtual NamedLock* CreateNamedLock(const StringPiece& name);

 private:
  class Lock;
  class LockHolder;
  typedef RefCountedPtr<LockHolder> LockHolderPtr;

  LockHolderPtr lock_holder_;
};

}  //  namespace net_instaweb

#endif  // PAGESPEED_KERNEL_UTIL_THREADSAFE_LOCK_MANAGER_H_
