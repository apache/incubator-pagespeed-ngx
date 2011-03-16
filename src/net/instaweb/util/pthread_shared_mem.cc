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

#include "net/instaweb/util/public/pthread_shared_mem.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/md5_hasher.h"

namespace net_instaweb {

namespace {

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
  PthreadSharedMemSegment(int fd, size_t size, MessageHandler* handler)
      : size_(size) {
    base_ = reinterpret_cast<char*>(
                mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    CheckedClose(fd, handler);
  }

  virtual ~PthreadSharedMemSegment() {
    if (base_ != MAP_FAILED) {
      munmap(base_, size_);
    }
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
      handler->Message(
          kError, "pthread_mutexattr_setpshared failed with errno:%d", errno);
      return false;
    }

    if (pthread_mutex_init(MutexPtr(offset), &attr) != 0) {
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

  bool IsAttached() {
    return base_ != MAP_FAILED;
  }

 private:
  pthread_mutex_t* MutexPtr(size_t offset) {
    return reinterpret_cast<pthread_mutex_t*>(base_ + offset);
  }

  char* base_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(PthreadSharedMemSegment);
};

}  // namespace

PthreadSharedMem::PthreadSharedMem() {
}

PthreadSharedMem::~PthreadSharedMem() {
}

size_t PthreadSharedMem::SharedMutexSize() const {
  return sizeof(pthread_mutex_t);
}

std::string PthreadSharedMem::EncodeName(const std::string& name) {
  // shm_open/shm_unlink are only well-defined for paths that contain
  // a leading slash and no other slashes; however our clients give us
  // hierarchical path names. We hence must flatten them by hashing.
  //
  // Note that O_EXCL below prevents files from created by other users from
  // doing symlink attacks.
  // ### They could prevent us from creating shmem segments, however?
  // ### (Feedback from reviewers would be appreciated)
  MD5Hasher hasher;
  return StrCat("/mod_pagespeed", hasher.Hash(name));
}

AbstractSharedMemSegment* PthreadSharedMem::CreateSegment(
    const std::string& sym_name, size_t size, MessageHandler* handler) {
  std::string name = EncodeName(sym_name);
  // cleanup any old segment.
  shm_unlink(name.c_str());

  // open the new one...
  int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    handler->Message(kError, "Unable to create SHM segment %s, errno=%d",
                     name.c_str(), errno);
    return NULL;
  }

  // allocate the memory...
  if (ftruncate(fd, size) == -1) {
    handler->Message(
        kError, "Unable to resize SHM segment %s to %ld bytes, errno=%d",
        name.c_str(), static_cast<long>(size), errno);  // NOLINT
    CheckedClose(fd, handler);
    shm_unlink(name.c_str());
    return NULL;
  }

  // map it
  scoped_ptr<PthreadSharedMemSegment> seg(
      new PthreadSharedMemSegment(fd, size, handler));
  if (!seg->IsAttached()) {
    shm_unlink(name.c_str());
    return NULL;
  }
  return seg.release();
}

AbstractSharedMemSegment* PthreadSharedMem::AttachToSegment(
    const std::string& sym_name, size_t size, MessageHandler* handler) {
  std::string name = EncodeName(sym_name);
  int fd = shm_open(name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    handler->Message(kError, "Unable to attach to SHM segment %s, errno=%d",
                     name.c_str(), errno);
    return NULL;
  }

  scoped_ptr<PthreadSharedMemSegment> seg(
      new PthreadSharedMemSegment(fd, size, handler));
  if (seg->IsAttached()) {
    return seg.release();
  }
  return NULL;
}

void PthreadSharedMem::DestroySegment(const std::string& sym_name,
                                      MessageHandler* handler) {
  std::string name = EncodeName(sym_name);
  if (shm_unlink(name.c_str()) == -1) {
    handler->Message(kWarning, "Unable to unlink SHM segment %s, errno=%d",
                     name.c_str(), errno);
  }
}

}  // namespace net_instaweb
