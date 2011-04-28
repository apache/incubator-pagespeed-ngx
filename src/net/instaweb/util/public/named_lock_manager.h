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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_NAMED_LOCK_MANAGER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_NAMED_LOCK_MANAGER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractLock : public AbstractMutex {
 public:
  virtual ~AbstractLock();
  // If lock is held, return false, otherwise lock and return true.  Note that
  // implementations of this and other similar 'try' routines are permitted to
  // return false conservatively.  TryLock must *eventually* succeed if called
  // repeatedly on an unheld lock, however.
  virtual bool TryLock() = 0;
  // Wait bounded amount of time to take lock, otherwise return false.
  virtual bool LockTimedWait(int64 wait_ms) = 0;

  // ...StealOld versions of locking routines steal the lock if its current
  // holder has locked it for more than timeout_ms.  *WARNING* If you use
  // any ...StealOld methods, your lock becomes "best-effort" and there may
  // be multiple workers in a critical section! *WARNING*

  // LockStealOld will block until the lock has been locked successfully, either
  // because it was unlocked by its current holder or because it was stolen from
  // its current holder.
  virtual void LockStealOld(int64 timeout_ms) = 0;
  // TryLockStealOld immediately attempts to lock the lock, succeeding if the
  // lock is unlocked or the lock can be stolen from the current holder.  The
  // return value is true if the lock was obtained, and false otherwise.
  // cf TryLock() for other caveats.
  virtual bool TryLockStealOld(int64 timeout_ms) = 0;
  // LockTimedWaitStealOld will block until unlocked, the lock has been held for
  // timeout_ms, or the caller has waited for wait_ms.
  virtual bool LockTimedWaitStealOld(int64 wait_ms, int64 timeout_ms) = 0;

  // The name the lock was created with, for debugging/logging purposes.
  virtual GoogleString name() = 0;
};

// A named_lock_manager provides global locks named by strings (with the same
// naming limitations in general as file names).  They provide a fairly rich
// api, with blocking and try versions and various timeout / steal behaviors.
class NamedLockManager {
 public:
  virtual ~NamedLockManager();
  virtual AbstractLock* CreateNamedLock(const StringPiece& name) = 0;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_NAMED_LOCK_MANAGER_H_
