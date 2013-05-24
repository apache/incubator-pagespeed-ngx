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

#include "pagespeed/kernel/thread/pthread_thread_system.h"

#ifdef linux
#include <features.h>
#endif
#include <pthread.h>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/thread/pthread_mutex.h"
#include "pagespeed/kernel/thread/pthread_rw_lock.h"

namespace net_instaweb {

namespace {

class PthreadId : public ThreadSystem::ThreadId {
 public:
  PthreadId() : id_(pthread_self()) {}
  virtual ~PthreadId() {}

  virtual bool IsEqual(const ThreadId& that) const {
    return pthread_equal(id_, dynamic_cast<const PthreadId&>(that).id_) != 0;
  }

  virtual bool IsCurrentThread() const {
    return pthread_equal(id_, pthread_self()) != 0;
  }

 private:
  pthread_t id_;

  DISALLOW_COPY_AND_ASSIGN(PthreadId);
};

}  // namespace

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
#ifdef __GLIBC_PREREQ
#if __GLIBC_PREREQ(2, 12)
    std::string name = self->wrapper_->name();
    // We need to truncate any long names to 15 characters or they might
    // not take.
    if (name.length() > 15) {
      name = name.substr(0, 15);
    }
    pthread_setname_np(self->thread_obj_, name.c_str());
#endif
#endif
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
  return new PosixTimer;
}

ThreadSystem::ThreadId* PthreadThreadSystem::GetThreadId() const {
  return new PthreadId;
}

}  // namespace net_instaweb
