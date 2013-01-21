/*
 * Copyright 2010 Google Inc.
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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_FILE_SYSTEM_LOCK_MANAGER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_FILE_SYSTEM_LOCK_MANAGER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class FileSystem;
class MessageHandler;
class Scheduler;

// Use the locking routines in FileSystem to implement named locks.  Requires a
// Scheduler as well because the FileSystem locks are non-blocking and we must
// deal with blocking until they are available.  A MessageHandler is used to
// report file system errors during lock creation and cleanup.
class FileSystemLockManager : public NamedLockManager {
 public:
  // Note: a FileSystemLockManager must outlive
  // any and all locks that it creates.
  // It does not assume ownership of the passed-in constructor arguments.
  // (Except it does copy in base_path). The caller is responsible for ensuring
  // that base_path exists.
  FileSystemLockManager(FileSystem* file_system,
                        const StringPiece& base_path,
                        Scheduler* scheduler,
                        MessageHandler* handler);
  virtual ~FileSystemLockManager();

  // Multiple lock objects with the same name will manage the same underlying
  // lock.  Lock names must be legal file names according to file_system.
  //
  // A lock created by CreateNamedLock will be Unlocked when it is destructed if
  // the NamedLock object appears to still be locked at destruction time.
  // This attempts to ensure that the file system is not littered with the
  // remnants of dead locks.  A given NamedLock object should Lock and Unlock
  // in matched pairs; DO NOT use separate NamedLock objects created with the
  // same name to perform a Lock and the corresponding Unlock.
  virtual NamedLock* CreateNamedLock(const StringPiece& name);

  // Simple accessors for constructor arguments
  FileSystem* file_system() const { return file_system_; }
  Scheduler* scheduler() const { return scheduler_; }
  MessageHandler* handler() const { return handler_; }

 private:
  FileSystem* file_system_;
  GoogleString base_path_;
  Scheduler* scheduler_;
  MessageHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemLockManager);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_FILE_SYSTEM_LOCK_MANAGER_H_
