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

// Author: jmaessen@google.com (Jan-Willem Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CHECKING_THREAD_SYSTEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CHECKING_THREAD_SYSTEM_H_

#include "net/instaweb/util/public/thread_system.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/atomic_bool.h"
#include "net/instaweb/util/public/atomic_int32.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class Timer;

// A thread system whose mutex and condvar factories yield implementations that
// permit checking of lock invariants using DCheckLocked().  This can be wrapped
// around an unchecked implementation.  This implementation checks invariants
// using CHECK (so does checking unconditionally).  To check conditionally, do
// the wrapping depending upon the setting of NDEBUG.  This is done by the
// ThreadSystem::CreateThreadSystem() factory by default, which is why the
// invariant checking method is called DCheckLock (Debug check lock) and not
// CheckLock.
class CheckingThreadSystem : public ThreadSystem {
 private:
  // C++ requires us to forward-declare this private class so that we can
  // friend it in CheckingThreadSystem::Mutex.
  // We otherwise try to keep it hidden from view.
  class CheckingCondvar;
 public:
  // We also expose CheckingThreadSystem::Mutex, which wraps a
  // CondvarCapableMutex to provide checked condvars and lock checking (these
  // two must be done together, so we must wrap the mutex from which the condvar
  // is created and use the wrapped mutex to create the condvar).  This class
  // can be used to wrap unchecked mutexes provided by other
  // CheckingThreadSystems.
  class Mutex : public ThreadSystem::CondvarCapableMutex {
   public:
    Mutex(ThreadSystem::CondvarCapableMutex* mutex) : mutex_(mutex) { }
    virtual ~Mutex();

    virtual void Lock();
    virtual void Unlock();
    // This implementation of DCheckLocked CHECK-fails if lock is not held.
    virtual void DCheckLocked();
    // The condvars provided perform lock checking for ....Wait operations.
    virtual ThreadSystem::Condvar* NewCondvar();

   private:
    friend class CheckingCondvar;
    void TakeLockControl();
    void DropLockControl();

    scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
    AtomicBool locked_;
    DISALLOW_COPY_AND_ASSIGN(Mutex);
  };

  // We also expose CheckingThreadSystem::RWLock, which wraps a
  // RWLock to provide read/write capable locks. This class
  // can be used to wrap unchecked mutexes provided by other
  // CheckingThreadSystems.
  class RWLock : public ThreadSystem::RWLock {
   public:
    RWLock(ThreadSystem::RWLock* lock) : lock_(lock) { }
    virtual ~RWLock();

    virtual void Lock();
    virtual void Unlock();
    virtual void ReaderLock();
    virtual void ReaderUnlock();

    // This implementation of DCheckLocked CHECK-fails if lock is not held.
    virtual void DCheckLocked();
    virtual void DCheckReaderLocked();

   private:
    void TakeLockControl();
    void DropLockControl();
    void TakeReaderLockControl();
    void DropReaderLockControl();

    scoped_ptr<ThreadSystem::RWLock> lock_;
    AtomicInt32 locked_;
    DISALLOW_COPY_AND_ASSIGN(RWLock);
  };

  CheckingThreadSystem(ThreadSystem* thread_system)
      : thread_system_(thread_system) { }
  virtual ~CheckingThreadSystem();

  virtual Mutex* NewMutex();
  virtual RWLock* NewRWLock();
  virtual Timer* NewTimer();

 private:
  friend class Mutex;

  virtual ThreadImpl* NewThreadImpl(Thread* wrapper, ThreadFlags flags);

  scoped_ptr<ThreadSystem> thread_system_;
  DISALLOW_COPY_AND_ASSIGN(CheckingThreadSystem);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CHECKING_THREAD_SYSTEM_H_
