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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_H_

#include "net/instaweb/rewriter/public/compatible_central_controller.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/statistics.h"

namespace net_instaweb {

// Concrete implementation of CentralControllerInterface, suitable for calling
// directly by workers that run in the same process as the controller.
// TODO(cheesy): Put some code in here, instead of just inheriting from
// CompatibleCentralController.

class CentralController : public CompatibleCentralController {
 public:
  CentralController(int max_expensive_operations, Statistics* statistics);
  virtual ~CentralController();

 private:
  DISALLOW_COPY_AND_ASSIGN(CentralController);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CENTRAL_CONTROLLER_H_
