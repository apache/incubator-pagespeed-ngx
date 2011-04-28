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

#include "net/instaweb/util/public/pthread_condvar.h"

#include <pthread.h>
#include <sys/time.h>
#include <cerrno>
#include <ctime>

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/pthread_mutex.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

PthreadCondvar::~PthreadCondvar() {
  pthread_cond_destroy(&condvar_);
}

void PthreadCondvar::Signal() {
  pthread_cond_signal(&condvar_);
}

void PthreadCondvar::Broadcast() {
  pthread_cond_broadcast(&condvar_);
}

void PthreadCondvar::Wait() {
  pthread_cond_wait(&condvar_, &mutex_->mutex_);
}

void PthreadCondvar::TimedWait(int64 timeout_ms) {
  const int64 kMsNs = Timer::kSecondNs / Timer::kSecondMs;
  struct timespec timeout;
  struct timeval current_time;
  // convert timeout_ms to seconds and ms
  int64 timeout_sec_incr = timeout_ms / Timer::kSecondMs;
  timeout_ms %= Timer::kSecondMs;
  // Figure out current time, compute absolute time for timeout
  // Carrying ns to s as appropriate.  As morlovich notes, we
  // get *really close* to overflowing a 32-bit tv_nsec here,
  // so this code should be modified with caution.
  if (gettimeofday(&current_time, NULL) != 0) {
    CHECK(false) << "Could not determine time of day";
  }
  timeout.tv_nsec = current_time.tv_usec * 1000 + timeout_ms * kMsNs;
  timeout_sec_incr += timeout.tv_nsec / Timer::kSecondNs;
  timeout.tv_nsec %= Timer::kSecondNs;
  timeout.tv_sec = current_time.tv_sec + static_cast<time_t>(timeout_sec_incr);
  // Finally we actually get to wait.
  pthread_cond_timedwait(&condvar_, &mutex_->mutex_, &timeout);
}

void PthreadCondvar::Init() {
  while (pthread_cond_init(&condvar_, NULL) == EAGAIN) { }
}

}  // namespace net_instaweb
