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
// This contains the class ThreadSystem::Thread which should be subclassed
// by things that wish to run in a thread.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_THREAD_H_
#define NET_INSTAWEB_UTIL_PUBLIC_THREAD_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

// Base class for client thread code.
class ThreadSystem::Thread {
 public:
  // Initializes the thread object for given runtime, but does not start it.
  // (You need to call Start() for that)
  //
  // If you pass in kJoinable for flags, you must explicitly call Join() to
  // wait for thread to complete and release associated resources. That is not
  // needed with kDetach, but you are still responsible for cleaning up
  // the Thread object.
  //
  // Any mutexes and condvars you use must be compatible with the passed in
  // 'runtime'.
  Thread(ThreadSystem* runtime, ThreadFlags flags);

  // Note: it is safe to delete the Thread object from within ::Run
  // as far as this baseclass is concerned.
  virtual ~Thread();

  // Invokes Run() in a separate thread. Returns if successful or not.
  // ### MessageHandler?
  bool Start();

  // Waits for the thread executing Run() to exit. This must be called on
  // every thread created with kJoinable.
  void Join();

  virtual void Run() = 0;

 private:
  // There are 2 types involved in the implementation of threading here.
  // One is this class, Thread, which user code subclasses
  //
  // The other code is ThreadImpl which is subclassed by the actual
  // implementation of threading and which does the actual threading work.
  scoped_ptr<ThreadImpl> impl_;

  ThreadFlags flags_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_THREAD_H_
