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

#include "net/instaweb/apache/apr_condvar.h"

#include "apr_pools.h"
#include "apr_thread_mutex.h"
#include "apr_thread_cond.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

AprCondvar::AprCondvar(AprMutex* mutex)
    : mutex_(mutex) {
  apr_pool_t* pool = apr_thread_mutex_pool_get(mutex_->thread_mutex_);
  // TODO(jmaessen): Error checking.  Why can this fail?
  apr_thread_cond_create(&condvar_, pool);
}

AprCondvar::~AprCondvar() {
  // TODO(jmaessen): Error checking.  Why can this fail?
  apr_thread_cond_destroy(condvar_);
}

void AprCondvar::Signal() {
  apr_thread_cond_signal(condvar_);
}

void AprCondvar::Broadcast() {
  apr_thread_cond_broadcast(condvar_);
}

void AprCondvar::Wait() {
  apr_thread_cond_wait(condvar_, mutex_->thread_mutex_);
}

void AprCondvar::TimedWait(int64 timeout_ms) {
  const int64 kMsUs = Timer::kSecondUs / Timer::kSecondMs;
  apr_thread_cond_timedwait(condvar_, mutex_->thread_mutex_,
                            timeout_ms * kMsUs);
}

}  // namespace net_isntaweb
