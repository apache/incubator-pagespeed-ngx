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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_THREAD_SYNCHRONIZER_H__
#define NET_INSTAWEB_UTIL_PUBLIC_THREAD_SYNCHRONIZER_H__

#include <map>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class ThreadSystem;

// Helps create deterministic multi-threaded tests targeting
// programmer-identified race conditions.
//
// Note that the goal of this class is not to provoke, or test for
// potential race conditions.  That is a noble goal, left for another
// day.
//
// The goal of this class is to help programmers that suspect they have
// found a race condition reproduce it robustly in a way that can be
// checked in as a unit-test and run instantly.
//
// This methodology does not rely on sleep().  It uses condition
// variables so it will run as fast as possible.  The sync-points can
// be left in all but the most nano-second-optimized production code
// as they are no-ops (single inlined if-statement) when turned off.
//
// This class is disabled by default, so that calls to it can be left in
// production code, but enabled for targeted tests.
class ThreadSynchronizer {
 public:
  explicit ThreadSynchronizer(ThreadSystem* thread_system);
  ~ThreadSynchronizer();

  // By default, the synchronizer is a no-op so we can inject sync-points
  // in production code at sensitive points with minimal overhead.  To
  // enable in a test, call EnableForPrefix("Prefix"), which will enable
  // any synchronization key beginning with "Prefix".
  //
  // EnableForPrefix should be called prior to any threading.
  void EnableForPrefix(StringPiece prefix) {
    enabled_ = true;
    prefix.CopyToString(StringVectorAdd(&prefixes_));
  }

  // Waits for a thread to signal the specified key.
  void Wait(const char* key) {
    if (enabled_) {
      DoWait(key);
    }
  }

  // Signals any thread waiting for a key that it can continue.  Signals
  // delivered in advance of a wait are remembered.  It is an error to
  // destruct the ThreadSynchronizer with pending signals.
  void Signal(const char* key) {
    if (enabled_) {
      DoSignal(key);
    }
  }

 private:
  class SyncPoint;
  typedef std::map<GoogleString, SyncPoint*> SyncMap;

  SyncPoint* GetSyncPoint(const GoogleString& key);
  void DoWait(const char* key);
  void DoSignal(const char* key);
  bool MatchesPrefix(const char* key) const;

  bool enabled_;
  ThreadSystem* thread_system_;
  SyncMap sync_map_;
  scoped_ptr<AbstractMutex> map_mutex_;
  StringVector prefixes_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSynchronizer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_THREAD_SYNCHRONIZER_H__
