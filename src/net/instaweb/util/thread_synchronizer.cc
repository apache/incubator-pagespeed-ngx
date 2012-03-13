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
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class ThreadSynchronizer::SyncPoint {
 public:
  SyncPoint(ThreadSystem* thread_system, const GoogleString& key)
      : mutex_(thread_system->NewMutex()),
        condvar_(mutex_->NewCondvar()),
        signal_count_(0),
        key_(key),
        allow_sloppy_(false) {
  }

  ~SyncPoint() {
    // TODO(jmarantz): This highlights that further generality is
    // likely needed here as adoption of this race-injection
    // methodology grows.  Perhaps enabling should be key-specific
    // in addition to applying to the whole class.
    if (!allow_sloppy_) {
      CHECK_EQ(0, signal_count_) << key_;
    }
  }

  void Wait() {
    ScopedMutex lock(condvar_->mutex());
    while (signal_count_ <= 0) {
      condvar_->Wait();
    }
    --signal_count_;
  }

  void TimedWait(int64 timeout_ms, Timer* timer) {
    ScopedMutex lock(condvar_->mutex());
    int64 now_ms = timer->NowMs();
    int64 end_ms = now_ms + timeout_ms;
    while ((signal_count_ <= 0) && (now_ms < end_ms)) {
      condvar_->TimedWait(end_ms - now_ms);
      now_ms = timer->NowMs();
    }

    // Note: we decrement signal even if we exited the loop via
    // timeout.  This is because we still expect Signal/*Wait to be
    // balanced.  We can allow SloppyTermination in cases where that
    // doesn't work, although then we must be careful of desired
    // semantics if a signal is never delivered for the first
    // call to TimedWait, but is delivered for the second one.
    --signal_count_;
  }

  void Signal() {
    ScopedMutex lock(condvar_->mutex());
    ++signal_count_;
    condvar_->Signal();
  }

  void AllowSloppyTermination() { allow_sloppy_ = true; }

 private:
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> condvar_;
  int signal_count_;
  GoogleString key_;  // for debugging;
  bool allow_sloppy_;

  DISALLOW_COPY_AND_ASSIGN(SyncPoint);
};

ThreadSynchronizer::ThreadSynchronizer(ThreadSystem* thread_system)
    : enabled_(false),
      thread_system_(thread_system),
      map_mutex_(thread_system->NewMutex()),
      timer_(thread_system->NewTimer()) {
}

ThreadSynchronizer::~ThreadSynchronizer() {
  STLDeleteValues(&sync_map_);
}

ThreadSynchronizer::SyncPoint* ThreadSynchronizer::GetSyncPoint(
    const GoogleString& key) {
  ScopedMutex lock(map_mutex_.get());
  SyncPoint* sync_point = sync_map_[key];
  if (sync_point == NULL) {
    sync_point = new SyncPoint(thread_system_, key);
    sync_map_[key] = sync_point;
  }
  return sync_point;
}

void ThreadSynchronizer::DoWait(const char* key) {
  if (MatchesPrefix(key)) {
    GetSyncPoint(key)->Wait();
  }
}

void ThreadSynchronizer::DoTimedWait(const char* key, int64 timeout_ms) {
  if (MatchesPrefix(key)) {
    GetSyncPoint(key)->TimedWait(timeout_ms, timer_.get());
  }
}

void ThreadSynchronizer::DoSignal(const char* key) {
  if (MatchesPrefix(key)) {
    GetSyncPoint(key)->Signal();
  }
}

bool ThreadSynchronizer::MatchesPrefix(const char* key) const {
  StringPiece key_piece(key);
  for (int i = 0, n = prefixes_.size(); i < n; ++i) {
    if (key_piece.starts_with(prefixes_[i])) {
      return true;
    }
  }
  return false;
}

void ThreadSynchronizer::AllowSloppyTermination(const char* key) {
  if (MatchesPrefix(key)) {
    GetSyncPoint(key)->AllowSloppyTermination();
  }
}

}  // namespace net_instaweb
