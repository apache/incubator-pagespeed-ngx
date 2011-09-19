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

// Author: morlovich@google.com (Maksim Orlovich)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_LOCK_MANAGER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_LOCK_MANAGER_H_

#include <cstddef>
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class AbstractSharedMem;
class AbstractSharedMemSegment;
class Hasher;
class MessageHandler;
class Scheduler;

namespace SharedMemLockData {

struct Bucket;

}  // namespace SharedMemLockData

// A simple shared memory named locking manager, which uses scheduler alarms
// (via SchedulerBasedAbstractLock) when it needs to block.
//
// TODO(morlovich): Implement condvars?
class SharedMemLockManager : public NamedLockManager {
 public:
  // Note that you must call Initialize() in the root process, and Attach in
  // child processes to finish the initialization.
  //
  // Locks created by this object must not live after it dies.
  SharedMemLockManager(
      AbstractSharedMem* shm, const GoogleString& path, Scheduler* scheduler,
      Hasher* hasher, MessageHandler* handler);
  virtual ~SharedMemLockManager();

  // Sets up our shared state for use of all child processes. Returns
  // whether successful.
  bool Initialize();

  // Connects to already initialized state from a child process.
  // Returns whether successful.
  bool Attach();

  // This should be called from the root process as it is about to exit,
  // with the same value as were passed to the constructor of any
  // instance on which Initialize() was called, except the message_handler
  // may be different (if for example the original one is no longer available
  // due to the cleanup sequence).
  static void GlobalCleanup(AbstractSharedMem* shm, const GoogleString& path,
                            MessageHandler* message_handler);

  virtual NamedLock* CreateNamedLock(const StringPiece& name);

 private:
  friend class SharedMemLock;

  SharedMemLockData::Bucket* Bucket(size_t bucket);

  // Offset of mutex wrt to segment base.
  size_t MutexOffset(SharedMemLockData::Bucket*);

  AbstractSharedMem* shm_runtime_;
  GoogleString path_;

  scoped_ptr<AbstractSharedMemSegment> seg_;
  Scheduler* scheduler_;
  Hasher* hasher_;
  MessageHandler* handler_;
  size_t lock_size_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemLockManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SHARED_MEM_LOCK_MANAGER_H_
