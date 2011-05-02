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

#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/apache/apr_mutex.h"

#ifndef NET_INSTAWEB_APACHE_APR_CONDVAR_H_
#define NET_INSTAWEB_APACHE_APR_CONDVAR_H_

struct apr_thread_cond_t;

namespace net_instaweb {

// Implementation of ThreadSystem::Condvar using apr_thread_cond_t.
class AprCondvar : public ThreadSystem::Condvar {
 public:
  // The mutex is owned by the caller and must outlive the condvar.
  explicit AprCondvar(AprMutex* mutex);
  virtual ~AprCondvar();

  virtual AprMutex* mutex() const { return mutex_; }

  virtual void Signal();
  virtual void Broadcast();
  virtual void Wait();
  virtual void TimedWait(int64 timeout_ms);

 private:
  AprMutex* mutex_;
  apr_thread_cond_t* condvar_;

  DISALLOW_COPY_AND_ASSIGN(AprCondvar);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_APR_CONDVAR_H_
