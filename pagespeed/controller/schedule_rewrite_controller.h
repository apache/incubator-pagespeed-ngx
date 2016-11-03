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

#ifndef PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_CONTROLLER_H_
#define PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_CONTROLLER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

// Abstract interface class that supports PSOL operations for controlling
// which rewrites should be run when. Ensures that only one rewrite will
// run concurrently for each supplied key. For use in CentralController.

class ScheduleRewriteController {
 public:
  virtual ~ScheduleRewriteController() { }

  // Run callback at an indeterminate time in the future when the rewrite
  // for the supplied key should be performed. Will call Cancel immediately
  // if the supplied key is currently in progress. May also call Cancel at
  // some point in the future, for instance if it decides the key isn't worth
  // rewriting.
  virtual void ScheduleRewrite(const GoogleString& key, Function* callback) = 0;

  // Inform controller that the rewrite has been completed. Should only be
  // called if Run() was invoked on callback above. Controller implemenations
  // may wish to behave differently depending on success or failure of the
  // result, for instance by retrying failures ASAP. Failure should not be
  // used in the case of permanent failure, such as a badly formed input.
  virtual void NotifyRewriteComplete(const GoogleString& key) = 0;
  virtual void NotifyRewriteFailed(const GoogleString& key) = 0;

  // Implementations of this method should try to cancel any pending operations
  // ASAP, and configure the object to immediately reject new incoming ones.
  //
  // Default implementation does nothing.
  virtual void ShutDown() { }

 protected:
  ScheduleRewriteController() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScheduleRewriteController);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_CONTROLLER_H_
