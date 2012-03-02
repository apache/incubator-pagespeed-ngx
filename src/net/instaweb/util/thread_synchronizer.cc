/*
 * Copyright 2012 Google Inc.
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

// Thread-synchronization utility class for reproducing races in unit tests.

#include "net/instaweb/util/public/thread_synchronizer.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class ThreadSynchronizer::SyncPoint {
 public:
  explicit SyncPoint(ThreadSystem* thread_system)
      : mutex_(thread_system->NewMutex()),
        condvar_(mutex_->NewCondvar()),
        signal_count_(0) {
  }

  ~SyncPoint() {
    // TODO(jmarantz): This highlights that further generality is
    // likely needed here as adoption of this race-injection
    // methodology grows.  Perhaps enabling should be key-specific
    // in addition to applying to the whole class.
    CHECK_EQ(0, signal_count_);
  }

  void Wait() {
    ScopedMutex lock(condvar_->mutex());
    while (signal_count_ == 0) {
      condvar_->Wait();
    }
    --signal_count_;
  }

  void Signal() {
    ScopedMutex lock(condvar_->mutex());
    ++signal_count_;
    condvar_->Signal();
  }

 private:
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> condvar_;
  int signal_count_;

  DISALLOW_COPY_AND_ASSIGN(SyncPoint);
};

ThreadSynchronizer::ThreadSynchronizer(ThreadSystem* thread_system)
    : thread_system_(thread_system),
      enabled_(false),
      map_mutex_(thread_system->NewMutex()) {
}

ThreadSynchronizer::~ThreadSynchronizer() {
  STLDeleteValues(&sync_map_);
}

ThreadSynchronizer::SyncPoint* ThreadSynchronizer::GetSyncPoint(
    const GoogleString& key) {
  ScopedMutex lock(map_mutex_.get());
  SyncPoint* sync_point = sync_map_[key];
  if (sync_point == NULL) {
    sync_point = new SyncPoint(thread_system_);
    sync_map_[key] = sync_point;
  }
  return sync_point;
}

void ThreadSynchronizer::DoWait(const char* key) {
  GetSyncPoint(key)->Wait();
}

void ThreadSynchronizer::DoSignal(const char* key) {
  GetSyncPoint(key)->Signal();
}

}  // namespace net_instaweb
