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

#ifndef PAGESPEED_CONTROLLER_COMPATIBLE_CENTRAL_CONTROLLER_H_
#define PAGESPEED_CONTROLLER_COMPATIBLE_CENTRAL_CONTROLLER_H_

#include "pagespeed/controller/in_process_central_controller.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// Implements CentralController, delegating to implementations that match
// pre-CentralController code. This is just a convenience wrapper around
// InProcessCentralController with appropriate delegates.

class CompatibleCentralController : public InProcessCentralController {
 public:
  CompatibleCentralController(int max_expensive_operations, Statistics* stats,
                              ThreadSystem* thread_system,
                              NamedLockManager* lock_manager);

  virtual ~CompatibleCentralController();

 private:
  DISALLOW_COPY_AND_ASSIGN(CompatibleCentralController);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_COMPATIBLE_CENTRAL_CONTROLLER_H_
