/*
 * Copyright 2013 Google Inc.
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

// Author: jmarantz@google.com (Joshua Marantz)

#include "pagespeed/kernel/base/null_thread_system.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/mock_timer.h"

namespace net_instaweb {

namespace {

const int64 kMockThreadId = 6765;

// Mock condvar-capable mutex.  Note that this does no actual locking,
// and will check-fail if you attempt to create a condvar.
class NullCondvarCapableMutex : public ThreadSystem::CondvarCapableMutex {
 public:
  NullCondvarCapableMutex() {}
  virtual ~NullCondvarCapableMutex();
  virtual bool TryLock() { return true; }
  virtual void Lock() {}
  virtual void Unlock() {}
  virtual ThreadSystem::Condvar* NewCondvar() {
    LOG(FATAL) << "Creating condvars in null thread system not supported";
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCondvarCapableMutex);
};

// Mock read-write-lock.  This does no locking.
class NullRWLock : public ThreadSystem::RWLock {
 public:
  NullRWLock() {}
  virtual ~NullRWLock();
  virtual bool ReaderTryLock() { return true; }
  virtual void ReaderLock() {}
  virtual void ReaderUnlock() {}
  virtual bool TryLock() { return true; }
  virtual void Lock() {}
  virtual void Unlock() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NullRWLock);
};

class NullThreadId : public ThreadSystem::ThreadId {
 public:
  explicit NullThreadId(const NullThreadSystem* system)
      : id_(system->current_thread()),
        system_(system) {
  }

  virtual ~NullThreadId() {}

  virtual bool IsEqual(const ThreadId& that) const {
    return (id_ == dynamic_cast<const NullThreadId&>(that).id_);
  }

  virtual bool IsCurrentThread() const {
    return id_ == system_->current_thread();
  }

 private:
  int id_;
  const NullThreadSystem* system_;

  DISALLOW_COPY_AND_ASSIGN(NullThreadId);
};

}  // namespace

NullCondvarCapableMutex::~NullCondvarCapableMutex() {
}

NullRWLock::~NullRWLock() {
}

NullThreadSystem::~NullThreadSystem() {
}

ThreadSystem::CondvarCapableMutex* NullThreadSystem::NewMutex() {
  return new NullCondvarCapableMutex;
}

ThreadSystem::RWLock* NullThreadSystem::NewRWLock() {
  return new NullRWLock;
}

Timer* NullThreadSystem::NewTimer() {
  // TODO(jmarantz): consider removing the responsibility of creating timers
  // from the thread system.
  return new MockTimer(0);
}

ThreadSystem::ThreadId* NullThreadSystem::GetThreadId() const {
  return new NullThreadId(this);
}

ThreadSystem::ThreadImpl* NullThreadSystem::NewThreadImpl(Thread* wrapper,
                                                          ThreadFlags flags) {
  LOG(FATAL) << "Creating threads in null thread system not supported";
  return NULL;
}

}  // namespace net_instaweb
