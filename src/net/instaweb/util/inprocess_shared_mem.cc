// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: morlovich@google.com (Maksim Orlovich)
//
// The InProcessSharedMem class emulates the normally cross-process shared
// memory API within a single process on top of threading APIs, in order to
// permit deploying classes built for shared memory into single-process
// servers or tests. Also living here are the ::Segment and ::Mutex classes
// InProcessSharedMem returns from its factory methods.

#include "net/instaweb/util/public/inprocess_shared_mem.h"

#include <cstring>
#include <map>
#include <vector>
#include <utility>

#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

// This object is a wrapping delegate around an existing AbstractMutex.
// We need it because AttachToSharedMutex is supposed to return fresh
// objects passing ownership to the caller.
class InProcessSharedMem::DelegateMutex : public AbstractMutex {
 public:
  // Does not take ownership of actual.
  explicit DelegateMutex(AbstractMutex* actual) : actual_(actual) {
  }

  virtual ~DelegateMutex() {
  }

  virtual bool TryLock() {
    return actual_->TryLock();
  }

  virtual void Lock() {
    actual_->Lock();
  }

  virtual void Unlock() {
    actual_->Unlock();
  }

  virtual void DCheckLocked() {
    actual_->DCheckLocked();
  }

 private:
  AbstractMutex* actual_;
  DISALLOW_COPY_AND_ASSIGN(DelegateMutex);
};

// Likewise for segments and AttachToSegment.
class InProcessSharedMem::DelegateSegment : public AbstractSharedMemSegment {
 public:
  explicit DelegateSegment(AbstractSharedMemSegment* actual) : actual_(actual) {
  }

  virtual ~DelegateSegment() {
  }

  virtual volatile char* Base() {
    return actual_->Base();
  }

  virtual size_t SharedMutexSize() const {
    return actual_->SharedMutexSize();
  }

  virtual bool InitializeSharedMutex(size_t offset,
                                     MessageHandler* handler) {
    return actual_->InitializeSharedMutex(offset, handler);
  }

  virtual AbstractMutex* AttachToSharedMutex(size_t offset) {
    return actual_->AttachToSharedMutex(offset);
  }

 private:
  AbstractSharedMemSegment* actual_;
  DISALLOW_COPY_AND_ASSIGN(DelegateSegment);
};

class InProcessSharedMem::Segment : public AbstractSharedMemSegment {
 public:
  Segment(ThreadSystem* thread_system, size_t size)
      : thread_system_(thread_system),
        storage_(new char[size]) {
    std::memset(storage_, 0, size);
  }

  virtual ~Segment() {
    STLDeleteElements(&mutexes_);
    delete[] storage_;
  }

  virtual volatile char* Base() {
    return storage_;
  }

  virtual size_t SharedMutexSize() const {
    return sizeof(AbstractMutex*);
  }

  virtual bool InitializeSharedMutex(size_t offset,
                                     MessageHandler* handler) {
    AbstractMutex* mutex = thread_system_->NewMutex();
    mutexes_.push_back(mutex);
    *MutexPtr(offset) = mutex;
    return true;
  }

  virtual AbstractMutex* AttachToSharedMutex(size_t offset) {
    return new DelegateMutex(*MutexPtr(offset));
  }

 private:
  AbstractMutex** MutexPtr(size_t offset) {
    return reinterpret_cast<AbstractMutex**>(storage_ + offset);
  }

  ThreadSystem* thread_system_;
  char* storage_;
  std::vector<AbstractMutex*> mutexes_;  // for memory ownership purpuses.

  DISALLOW_COPY_AND_ASSIGN(Segment);
};

InProcessSharedMem::InProcessSharedMem(ThreadSystem* thread_system)
    : thread_system_(thread_system) {
}

InProcessSharedMem::~InProcessSharedMem() {
  STLDeleteValues(&segments_);
}

size_t InProcessSharedMem::SharedMutexSize() const {
  // We just store the pointer to the actual thread system mutex object inline.
  return sizeof(AbstractMutex*);
}

AbstractSharedMemSegment* InProcessSharedMem::CreateSegment(
    const GoogleString& name, size_t size, MessageHandler* handler) {
  Segment* seg = new Segment(thread_system_, size);

  SegmentMap::iterator prev = segments_.find(name);
  if (prev != segments_.end()) {
    handler->Message(kError, "CreateSegment done twice for name:%s",
                     name.c_str());
    delete prev->second;
  }

  segments_[name] = seg;

  // We want to return a DelegateSegment here as well to decouple memory
  // ownership of Segment objects from DestroySegment calls.
  return new DelegateSegment(seg);
}

AbstractSharedMemSegment* InProcessSharedMem::AttachToSegment(
      const GoogleString& name, size_t size, MessageHandler* handler) {
  SegmentMap::iterator prev = segments_.find(name);
  if (prev != segments_.end()) {
    return new DelegateSegment(prev->second);
  } else {
    handler->Message(kError, "AttachToSegment unable to find segment:%s",
                     name.c_str());
    return NULL;
  }
}

void InProcessSharedMem::DestroySegment(
    const GoogleString& name, MessageHandler* handler) {
  SegmentMap::iterator prev = segments_.find(name);
  if (prev != segments_.end()) {
    // This deletes the actual Segment, but not any DelegateSegment.
    delete prev->second;
    segments_.erase(prev);
  } else {
    handler->Message(kError, "DestroySegment unable to find segment:%s",
                     name.c_str());
  }
}

}  // namespace net_instaweb
