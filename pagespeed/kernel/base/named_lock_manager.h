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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef PAGESPEED_KERNEL_BASE_NAMED_LOCK_MANAGER_H_
#define PAGESPEED_KERNEL_BASE_NAMED_LOCK_MANAGER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class Function;

// Non-blocking locking class.
class NamedLock {
 public:
  // Destructors of extending classes must unlock the lock if held on destruct.
  virtual ~NamedLock();

  // Attempts to take a lock.  callback->Run() is called if the lock was
  // granted, and callback->Cancel() is called if the lock could not be
  // obtained within wait_ms.  Note that the callback may be called directly
  // from this method, or from another thread.
  //
  // The caller is responsible for making sure that callback does not block.
  //
  // TODO(jmarantz): consider removing this method as it has no callers in
  // production code, though it does have callers in tests.
  virtual void LockTimedWait(int64 wait_ms, Function* callback) = 0;

  // Attempts to take a lock, calling callback->Run() when it is granted.
  // If the current lock holder has locked it for more than steal_ms, the
  // lock is "stolen".  If the lock cannot be obtained within wait_ms from
  // when this method was called, the lock is denied, and callback->Cancel()
  // is called.
  //
  // Note that the callback may be called directly from this method, or from
  // another thread.
  //
  // The caller is responsible for making sure that callback does not block.
  //
  // Note that even if wait_ms > steal_ms, callback->Cancel() may be
  // called if there are multiple concurrent attempts to take the
  // lock.
  virtual void LockTimedWaitStealOld(int64 wait_ms, int64 steal_ms,
                                     Function* callback) = 0;

  // Relinquish lock.  Non-blocking, however note when this lock is relinquished
  // another lock may be granted, resulting in its callback->Run() method
  // being called from Unlock.
  virtual void Unlock() = 0;

  // Returns true if this lock is held by this particular lock object.
  //
  // Note: in some implementations Held() may remain true until Unlock
  // regardless if another lock steals.
  virtual bool Held() = 0;

  // The name the lock was created with, for debugging/logging purposes.
  virtual GoogleString name() const = 0;
};

// A named_lock_manager provides global locks named by strings (with the same
// naming limitations in general as file names).  They provide a fairly rich
// api, with blocking and try versions and various timeout / steal behaviors.
class NamedLockManager {
 public:
  virtual ~NamedLockManager();
  virtual NamedLock* CreateNamedLock(const StringPiece& name) = 0;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_NAMED_LOCK_MANAGER_H_
