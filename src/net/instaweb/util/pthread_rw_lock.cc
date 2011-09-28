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

// Author: pulkitg@google.com (Pulkit Goyal)

#include "net/instaweb/util/public/pthread_rw_lock.h"

#include <pthread.h>

namespace net_instaweb {

PthreadRWLock::PthreadRWLock() {
  pthread_rwlockattr_init(&attr_);
  // New writer lock call is given preference over existing reader lock calls,
  // so that writer lock call will never get starved. However, it is not allowed
  // if there exists any recursive reader lock call to prevent deadlocks.
  pthread_rwlockattr_setkind_np(&attr_,
                                PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
  pthread_rwlock_init(&rwlock_, &attr_);
}

PthreadRWLock::~PthreadRWLock() {
  pthread_rwlockattr_destroy(&attr_);
  pthread_rwlock_destroy(&rwlock_);
}

void PthreadRWLock::Lock() {
  pthread_rwlock_wrlock(&rwlock_);
}

void PthreadRWLock::Unlock() {
  pthread_rwlock_unlock(&rwlock_);
}

void PthreadRWLock::ReaderLock() {
  pthread_rwlock_rdlock(&rwlock_);
}

void PthreadRWLock::ReaderUnlock() {
  pthread_rwlock_unlock(&rwlock_);
}

}  // namespace net_instaweb
