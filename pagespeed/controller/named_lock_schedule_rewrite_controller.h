// Copyright 2015 Google Inc.
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
// Author: cheesy@google.com (Steve Hill)

#ifndef PAGESPEED_CONTROLLER_NAMED_LOCK_SCHEDULE_REWRITE_CONTROLLER_H_
#define PAGESPEED_CONTROLLER_NAMED_LOCK_SCHEDULE_REWRITE_CONTROLLER_H_

#include <unordered_set>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/controller/schedule_rewrite_controller.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/rde_hash_map.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_hash.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// Implements ScheduleRewriteController by wrapping NamedLockManager.
// This is a backwards compatible implementation, for use in CentralController.
class NamedLockScheduleRewriteController : public ScheduleRewriteController {
 public:
  static const char kLocksGranted[];
  static const char kLocksDenied[];
  static const char kLocksStolen[];
  static const char kLocksReleasedWhenNotHeld[];
  static const char kLocksCurrentlyHeld[];

  static const int kStealMs;

  NamedLockScheduleRewriteController(NamedLockManager* lock_manager,
                                     ThreadSystem* thread_system,
                                     Statistics* statistics);
  virtual ~NamedLockScheduleRewriteController();

  // ScheduleRewriteController interface.
  virtual void ScheduleRewrite(const GoogleString& key, Function* callback);
  virtual void NotifyRewriteComplete(const GoogleString& key);
  virtual void NotifyRewriteFailed(const GoogleString& key);

  void ShutDown() override;

  static void InitStats(Statistics* stats);

 private:
  struct LockInfo {
    LockInfo() : pin_count(0) { }
    // lock is only non-NULL when we have successfully obtained it.
    scoped_ptr<NamedLock> lock;

    std::unordered_set<Function*> pending_callbacks;

    // "Extra" refcount on top of lock and pending_callbacks we need
    // occassionally to protect this data against a temporary lock
    // relinquishment.
    int pin_count;

   private:
    DISALLOW_COPY_AND_ASSIGN(LockInfo);
  };

  typedef rde::hash_map<GoogleString, LockInfo*, CasePreserveStringHash>
      LockMap;

  void LockObtained(Function* callback, const GoogleString key, NamedLock* lock)
      LOCKS_EXCLUDED(mutex_);
  void LockFailed(Function* callback, const GoogleString key, NamedLock* lock)
      LOCKS_EXCLUDED(mutex_);

  LockInfo* GetLockInfo(const GoogleString& key)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void DeleteInfoIfUnused(LockInfo* info, const GoogleString& key)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  scoped_ptr<AbstractMutex> mutex_;
  NamedLockManager* lock_manager_;
  LockMap locks_ GUARDED_BY(mutex_);
  bool shut_down_ GUARDED_BY(mutex_);

  TimedVariable* locks_granted_;
  TimedVariable* locks_denied_;
  TimedVariable* locks_stolen_;
  TimedVariable* locks_released_when_not_held_;
  UpDownCounter* locks_currently_held_;

  DISALLOW_COPY_AND_ASSIGN(NamedLockScheduleRewriteController);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_NAMED_LOCK_SCHEDULE_REWRITE_CONTROLLER_H_
