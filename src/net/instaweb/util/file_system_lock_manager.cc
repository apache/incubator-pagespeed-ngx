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
#include "net/instaweb/util/public/file_system_lock_manager.h"

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/file_system.h"
#include "net/instaweb/util/public/scheduler_based_abstract_lock.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;
class Scheduler;

class FileSystemLock : public SchedulerBasedAbstractLock {
 public:
  virtual ~FileSystemLock() {
    if (held_) {
      Unlock();
    }
  }

  virtual bool TryLock() {
    bool result = false;
    if (manager_->file_system()->
        TryLock(name_, manager_->handler()).is_true()) {
      held_ = result = true;
    }
    return result;
  }

  virtual bool TryLockStealOld(int64 timeout_ms) {
    bool result = false;
    if (manager_->file_system()->
        TryLockWithTimeout(name_, timeout_ms, manager_->handler()).is_true()) {
      held_ = result = true;
    }
    return result;
  }

  virtual void Unlock() {
    held_ = !manager_->file_system()->Unlock(name_, manager_->handler());
  }

  virtual GoogleString name() {
    return name_;
  }

 protected:
  virtual Scheduler* scheduler() const {
    return manager_->scheduler();
  }

 private:
  friend class FileSystemLockManager;

  // ctor should only be called by CreateNamedLock below.
  FileSystemLock(const StringPiece& name, FileSystemLockManager* manager)
      : name_(name.data(), name.size()),
        manager_(manager),
        held_(false) { }

  GoogleString name_;
  FileSystemLockManager* manager_;
  // The held_ field contains an approximation to whether the lock is locked or
  // not.  If we believe the lock to be locked, we will unlock on destruction.
  // We therefore try to conservatively leave it "true" when we aren't sure.
  bool held_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemLock);
};

FileSystemLockManager::FileSystemLockManager(
    FileSystem* file_system, const StringPiece& base_path, Scheduler* scheduler,
    MessageHandler* handler)
    : file_system_(file_system),
      base_path_(base_path.as_string()),
      scheduler_(scheduler),
      handler_(handler) {
  EnsureEndsInSlash(&base_path_);
}

FileSystemLockManager::~FileSystemLockManager() { }

NamedLock* FileSystemLockManager::CreateNamedLock(const StringPiece& name) {
  return new FileSystemLock(StrCat(base_path_, name), this);
}

}  // namespace net_instaweb
