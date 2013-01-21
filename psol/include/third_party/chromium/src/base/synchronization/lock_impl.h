// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_LOCK_IMPL_H_
#define BASE_SYNCHRONIZATION_LOCK_IMPL_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX)
#include <pthread.h>
#endif

#include "base/base_api.h"
#include "base/basictypes.h"

namespace base {
namespace internal {

// This class implements the underlying platform-specific spin-lock mechanism
// used for the Lock class.  Most users should not use LockImpl directly, but
// should instead use Lock.
class BASE_API LockImpl {
 public:
#if defined(OS_WIN)
  typedef CRITICAL_SECTION OSLockType;
#elif defined(OS_POSIX)
  typedef pthread_mutex_t OSLockType;
#endif

  LockImpl();
  ~LockImpl();

  // If the lock is not held, take it and return true.  If the lock is already
  // held by something else, immediately return false.
  bool Try();

  // Take the lock, blocking until it is available if necessary.
  void Lock();

  // Release the lock.  This must only be called by the lock's holder: after
  // a successful call to Try, or a call to Lock.
  void Unlock();

  // Return the native underlying lock.  Not supported for Windows builds.
  // TODO(awalker): refactor lock and condition variables so that this is
  // unnecessary.
#if !defined(OS_WIN)
  OSLockType* os_lock() { return &os_lock_; }
#endif

 private:
  OSLockType os_lock_;

  DISALLOW_COPY_AND_ASSIGN(LockImpl);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_SYNCHRONIZATION_LOCK_IMPL_H_
