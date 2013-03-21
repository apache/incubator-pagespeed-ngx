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

#include "pthread_shared_mem.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
#include <cstddef>
#include <map>
#include <utility>
#include "net/instaweb/util/public/abstract_shared_mem.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

namespace {

// This implementation relies on readonly copies of old memory and shared R/W
// mappings being kept across a fork. It simply stashes addresses of
// shared mmap segments into a map where kid processes can pick them up.

// close() a fd logging failure and dealing with EINTR.
void CheckedClose(int fd, MessageHandler* message_handler) {
  while (close(fd) != 0) {
    if (errno != EINTR) {
      message_handler->Message(kWarning, "Problem closing SHM segment fd:%d",
                               errno);
      return;
    }
  }
}

// Unlike PthreadMutex this doesn't own the lock, but rather refers to an
// external one.
class PthreadSharedMemMutex : public AbstractMutex {
 public:
  explicit PthreadSharedMemMutex(pthread_mutex_t* external_mutex)
      : external_mutex_(external_mutex) {}

  virtual bool TryLock() {
    return (pthread_mutex_trylock(external_mutex_) == 0);
  }

  virtual void Lock() {
    pthread_mutex_lock(external_mutex_);
  }

  virtual void Unlock() {
    pthread_mutex_unlock(external_mutex_);
  }

 private:
  pthread_mutex_t* external_mutex_;

  DISALLOW_COPY_AND_ASSIGN(PthreadSharedMemMutex);
};

class PthreadSharedMemSegment : public AbstractSharedMemSegment {
 public:
  // We will be representing memory mapped in the [base, base + size) range.
  PthreadSharedMemSegment(char* base, size_t size, MessageHandler* handler)
      : base_(base),
        size_(size) {
  }

  virtual ~PthreadSharedMemSegment() {
  }

  virtual volatile char* Base() {
    return base_;
  }

  virtual size_t SharedMutexSize() const {
    return sizeof(pthread_mutex_t);
  }

  virtual bool InitializeSharedMutex(size_t offset, MessageHandler* handler) {
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
      handler->Message(kError, "pthread_mutexattr_init failed with errno:%d",
                       errno);
      return false;
    }

    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
      pthread_mutexattr_destroy(&attr);
      handler->Message(
          kError, "pthread_mutexattr_setpshared failed with errno:%d", errno);
      return false;
    }

    if (pthread_mutex_init(MutexPtr(offset), &attr) != 0) {
      pthread_mutexattr_destroy(&attr);
      handler->Message(kError, "pthread_mutex_init failed with errno:%d",
                       errno);
      return false;
    }

    pthread_mutexattr_destroy(&attr);
    return true;
  }

  virtual AbstractMutex* AttachToSharedMutex(size_t offset) {
    return new PthreadSharedMemMutex(MutexPtr(offset));
  }

 private:
  pthread_mutex_t* MutexPtr(size_t offset) {
    return reinterpret_cast<pthread_mutex_t*>(base_ + offset);
  }

  char* const base_;
  const size_t size_;

  DISALLOW_COPY_AND_ASSIGN(PthreadSharedMemSegment);
};

pthread_mutex_t segment_bases_lock = PTHREAD_MUTEX_INITIALIZER;

}  // namespace

size_t PthreadSharedMem::s_instance_count_ = 0;

PthreadSharedMem::SegmentBaseMap* PthreadSharedMem::segment_bases_ = NULL;

PthreadSharedMem::PthreadSharedMem() {
  instance_number_ = ++s_instance_count_;
}

PthreadSharedMem::~PthreadSharedMem() {
}

size_t PthreadSharedMem::SharedMutexSize() const {
  return sizeof(pthread_mutex_t);
}

AbstractSharedMemSegment* PthreadSharedMem::CreateSegment(
    const GoogleString& name, size_t size, MessageHandler* handler) {
  GoogleString prefixed_name = PrefixSegmentName(name);
  // Create the memory
  int fd = open("/dev/zero", O_RDWR);
  if (fd == -1) {
    handler->Message(kError, "Unable to create SHM segment %s, errno=%d.",
                     prefixed_name.c_str(), errno);
    return NULL;
  }

  // map it
  char* base = reinterpret_cast<char*>(
                   mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  CheckedClose(fd, handler);
  if (base == MAP_FAILED) {
    return NULL;
  }

  SegmentBaseMap* bases = AcquireSegmentBases();
  (*bases)[prefixed_name] = base;
  UnlockSegmentBases();
  return new PthreadSharedMemSegment(base, size, handler);
}

AbstractSharedMemSegment* PthreadSharedMem::AttachToSegment(
    const GoogleString& name, size_t size, MessageHandler* handler) {
  GoogleString prefixed_name = PrefixSegmentName(name);
  SegmentBaseMap* bases = AcquireSegmentBases();
  SegmentBaseMap::const_iterator i = bases->find(prefixed_name);
  if (i == bases->end()) {
    handler->Message(kError, "Unable to find SHM segment %s to attach to.",
                     prefixed_name.c_str());
    UnlockSegmentBases();
    return NULL;
  }
  char* base = i->second;
  UnlockSegmentBases();
  return new PthreadSharedMemSegment(base, size, handler);
}

void PthreadSharedMem::DestroySegment(const GoogleString& name,
                                      MessageHandler* handler) {
  GoogleString prefixed_name = PrefixSegmentName(name);

  // Note that in the process state children will not see any mutations
  // we make here, so it acts mostly for checking in that case.
  SegmentBaseMap* bases = AcquireSegmentBases();
  SegmentBaseMap::iterator i = bases->find(prefixed_name);
  if (i != bases->end()) {
    bases->erase(i);
    if (bases->empty()) {
      delete segment_bases_;
      segment_bases_ = NULL;
    }
  } else {
    handler->Message(kError, "Attempt to destroy unknown SHM segment %s.",
                     prefixed_name.c_str());
  }
  UnlockSegmentBases();
}

PthreadSharedMem::SegmentBaseMap* PthreadSharedMem::AcquireSegmentBases() {
  PthreadSharedMemMutex lock(&segment_bases_lock);
  lock.Lock();

  if (segment_bases_ == NULL) {
    segment_bases_ = new SegmentBaseMap();
  }

  return segment_bases_;
}

void PthreadSharedMem::UnlockSegmentBases() {
  PthreadSharedMemMutex lock(&segment_bases_lock);
  lock.Unlock();
}

GoogleString PthreadSharedMem::PrefixSegmentName(const GoogleString& name)
{
  GoogleString res;
  StrAppend(&res, "[", IntegerToString(instance_number_), "]", name);
  return res;
}


void PthreadSharedMem::Terminate() {
  // Clean up the local memory associated with the maps to shared memory
  // storage.
  PthreadSharedMemMutex lock(&segment_bases_lock);
  lock.Lock();
  if (segment_bases_ != NULL) {
    delete segment_bases_;
    segment_bases_ = NULL;
  }
  lock.Unlock();
}

}  // namespace net_instaweb
