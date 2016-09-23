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

#ifndef PAGESPEED_KERNEL_BASE_THREAD_H_
#define PAGESPEED_KERNEL_BASE_THREAD_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"

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
  //
  // The 'name' will be used purely for debugging purposes. Note that on
  // many systems (e.g. Linux PThreads) the OS will only keep track of
  // 15 characters, so you may not want to get too wordy.
  Thread(ThreadSystem* runtime, StringPiece name, ThreadFlags flags);

  // Note: it is safe to delete the Thread object from within ::Run
  // as far as this baseclass is concerned.
  virtual ~Thread();

  // Invokes Run() in a separate thread. Returns if successful or not.
  // Threads are not re-startable, you should create new instance of Thread if
  // you want to create another thread.
  bool Start();

  // Whether Start() ran successfully on this object, useful if your Thread
  // may want to call Join() in it's destructor.
  bool Started() const { return started_; }

  // Waits for the thread executing Run() to exit. This must be called on
  // every thread created with kJoinable.
  void Join();

  GoogleString name() const { return name_; }

  virtual void Run() = 0;

 private:
  // There are 2 types involved in the implementation of threading here.
  // One is this class, Thread, which user code subclasses
  //
  // The other code is ThreadImpl which is subclassed by the actual
  // implementation of threading and which does the actual threading work.
  scoped_ptr<ThreadImpl> impl_;

  GoogleString name_;

  ThreadFlags flags_;
  bool started_;
  bool join_called_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_THREAD_H_
