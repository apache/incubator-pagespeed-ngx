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

#include "net/instaweb/util/public/pthread_thread_system.h"

#include <pthread.h>

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_timer.h"
#include "net/instaweb/util/public/pthread_rw_lock.h"
#include "net/instaweb/util/public/pthread_mutex.h"
#include "net/instaweb/util/public/thread.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class Timer;

class PthreadThreadImpl : public ThreadSystem::ThreadImpl {
 public:
  PthreadThreadImpl(PthreadThreadSystem* thread_system,
                    ThreadSystem::Thread* wrapper,
                    ThreadSystem::ThreadFlags flags)
      : thread_system_(thread_system),
        wrapper_(wrapper),
        flags_(flags) {
  }

  virtual ~PthreadThreadImpl() {
  }

  virtual bool StartImpl() {
    int result;

    pthread_attr_t attr;
    result = pthread_attr_init(&attr);
    if (result != 0) {
      return false;
    }

    int mode = PTHREAD_CREATE_DETACHED;
    if ((flags_ & ThreadSystem::kJoinable) != 0) {
      mode = PTHREAD_CREATE_JOINABLE;
    }

    result = pthread_attr_setdetachstate(&attr, mode);
    if (result != 0) {
      return false;
    }

    result = pthread_create(&thread_obj_, &attr, InvokeRun, this);
    if (result != 0) {
      return false;
    }

    pthread_attr_destroy(&attr);
    return true;
  }

  virtual void JoinImpl() {
    void* ignored;
    pthread_join(thread_obj_, &ignored);
  }

 private:
  static void* InvokeRun(void* self_ptr) {
    PthreadThreadImpl* self = static_cast<PthreadThreadImpl*>(self_ptr);
    self->thread_system_->BeforeThreadRunHook();
    self->wrapper_->Run();
    return NULL;
  }

  PthreadThreadSystem* thread_system_;
  ThreadSystem::Thread* wrapper_;
  ThreadSystem::ThreadFlags flags_;
  pthread_t thread_obj_;

  DISALLOW_COPY_AND_ASSIGN(PthreadThreadImpl);
};

PthreadThreadSystem::PthreadThreadSystem() {
}

PthreadThreadSystem::~PthreadThreadSystem() {
}

ThreadSystem::CondvarCapableMutex* PthreadThreadSystem::NewMutex() {
  return new PthreadMutex;
}

ThreadSystem::RWLock* PthreadThreadSystem::NewRWLock() {
  return new PthreadRWLock;
}

void PthreadThreadSystem::BeforeThreadRunHook() {
}

ThreadSystem::ThreadImpl* PthreadThreadSystem::NewThreadImpl(
    ThreadSystem::Thread* wrapper, ThreadSystem::ThreadFlags flags) {
  return new PthreadThreadImpl(this, wrapper, flags);
}

Timer* PthreadThreadSystem::NewTimer() {
  return new GoogleTimer;
}

}  // namespace net_instaweb
