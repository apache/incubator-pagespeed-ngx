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
// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_CONDVAR_H_
#define NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_CONDVAR_H_

#include <pthread.h>

#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/pthread_mutex.h"

namespace net_instaweb {

class PthreadCondvar : public ThreadSystem::Condvar {
 public:
  // The mutex is owned by the caller and must outlive the condvar.
  explicit PthreadCondvar(PthreadMutex* mutex)
      : mutex_(mutex) {
    Init();
  }
  virtual ~PthreadCondvar();

  virtual PthreadMutex* mutex() const { return mutex_; }

  virtual void Signal();
  virtual void Broadcast();
  virtual void Wait();
  virtual void TimedWait(int64 timeout_ms);

 private:
  void Init();

  PthreadMutex* mutex_;
  pthread_cond_t condvar_;

  DISALLOW_COPY_AND_ASSIGN(PthreadCondvar);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_PTHREAD_CONDVAR_H_
