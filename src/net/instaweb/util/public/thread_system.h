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
// This contains classes that abstract away creation of threads and
// synchronization primitives.
// - ThreadSystem (base class): acts as a factory for mutexes compatible
//   with some runtime environment and must be passed to Thread ctor to use its
//   threading abilities.
// - ThreadImpl: abstract interface used to communicate with threading
//   backends by Thread instances.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_THREAD_SYSTEM_H_
#define NET_INSTAWEB_UTIL_PUBLIC_THREAD_SYSTEM_H_

#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class Timer;

// Subclasses of this represent threading support under given environment,
// and help create various primitives for it.
class ThreadSystem {
 public:
  class Condvar;
  class Thread;
  class ThreadImpl;

  class CondvarCapableMutex : public AbstractMutex {
   public:
    CondvarCapableMutex() { }
    virtual ~CondvarCapableMutex();

    // Creates a new condition variable associated with 'this' mutex.
    virtual Condvar* NewCondvar() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(CondvarCapableMutex);
  };

  // Interface for a Mutex with ReaderLocks().  It is possible for multiple
  // Readers to simultaneously hold an RWLock.  A reader cannot hold the
  // lock at the same time as a Writer, nor can two Writers hold the lock.
  class RWLock : public AbstractMutex {
   public:
    RWLock() {}
    virtual ~RWLock();

    // ReaderLock/Unlock are different from normal locks. Reader locks are
    // shared while normal locks are exclusive. Normal lock cannot happen when
    // reader has a lock.
    // Block until this Mutex is free, or shared, then acquire a share of it.
    virtual void ReaderLock() = 0;
    // Release a read share of this Mutex.
    virtual void ReaderUnlock() = 0;

    // Optionally checks that reader lock is held (for invariant checking
    // purposes). Default implementation does no checking.
    virtual void DCheckReaderLocked();

   private:
    DISALLOW_COPY_AND_ASSIGN(RWLock);
  };

  // Scoped reader-lock for using RWLock*.  Facilitates grabbing a
  // reader-lock on entry to a scope, and releasing it on exit.
  // Similar to ScopedMutex found in AbstractMutex, except that
  // multiple ScopedReaders can be simultaneously instantiated on
  // the same RWLock*.
  class ScopedReader {
   public:
    explicit ScopedReader(RWLock* lock) : lock_(lock) {
      lock_->ReaderLock();
    }

    void Release() {
      // We allow Release called explicitly, before the ScopedReader goes
      // out of scope and is destructed, calling Release again.
      if (lock_ != NULL) {
        lock_->ReaderUnlock();
        lock_ = NULL;
      }
    }

    ~ScopedReader() {
      Release();
    }

   private:
    RWLock* lock_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReader);
  };

  enum ThreadFlags {
    kDetached = 0,
    kJoinable = 1
  };

  virtual ~ThreadSystem();
  ThreadSystem() {}

  // Makes a new mutex for this system.
  //
  // See also CondvarCapableMutex::NewCondvar.
  virtual CondvarCapableMutex* NewMutex() = 0;

  // This lock will provide following guarantee -
  // - Reader reentrant safe.
  // - Writer Priority, this ensures no writer starvation.
  virtual RWLock* NewRWLock() = 0;

  // Creates an appropriate ThreadSystem for the platform.
  static ThreadSystem* CreateThreadSystem();

  // Creates and returns a real-time timer.  Caller is responsible for deleting.
  virtual Timer* NewTimer() = 0;

 private:
  friend class Thread;
  friend class MockThreadSystem;
  friend class CheckingThreadSystem;
  virtual ThreadImpl* NewThreadImpl(Thread* wrapper, ThreadFlags flags) = 0;

  DISALLOW_COPY_AND_ASSIGN(ThreadSystem);
};

// ThreadImpl is the class that's inherited off when implementing threading ---
// ThreadSystem::NewThreadImpl is responsible for creating an appropriate
// instance that's hooked up to a given user Thread object.
class ThreadSystem::ThreadImpl {
 public:
  virtual bool StartImpl() = 0;
  virtual void JoinImpl() = 0;
  virtual ~ThreadImpl();

 protected:
  ThreadImpl() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadImpl);
};

// Catch bug where variable name is omitted with ScopedReader, e.g.
// ThreadSystem::ScopedReader(&lock);
#define ScopedReader(x) COMPILE_ASSERT(0, mutex_lock_decl_missing_var_name)

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_THREAD_SYSTEM_H_
