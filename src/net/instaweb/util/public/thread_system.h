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

class QueuedWorker;

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

  // Creates an appropriate ThreadSystem for the platform.
  static ThreadSystem* CreateThreadSystem();

  // Execute a timed wait on the specified condition variable.  Under
  // normal server operation this just runs the condition variable's
  // TimedWait method.  When running fast simulated-time unit tests,
  // however, we need to synchronize timer-advance events using the
  // rewrite_worker so we use this entry-point.
  //
  // TODO(jmarantz): refactor out the dependence from ThreadSystem
  // to QueuedWorker.  This can be done in at least two different ways:
  //  1. Make a separate virtual class hierarchy to wrap Time, ThreadSystem,
  //     and condition-variables.
  //  2. Change the general architecture to have only one QueuedWorker which the
  //     MockThreadSystem will know about.
  // This was the most expedient approach but it's not that hard to change it.
  virtual void TimedWait(QueuedWorker* worker,
                         ThreadSystem::Condvar* condvar,
                         int64 timeout_ms);

 private:
  friend class Thread;
  friend class MockThreadSystem;
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

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_THREAD_SYSTEM_H_
