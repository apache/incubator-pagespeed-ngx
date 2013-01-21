/*
 * Copyright 2013 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)
//
// See ApacheThreadSystem.
//
//

#ifndef NGX_THREAD_SYSTEM_H_
#define NGX_THREAD_SYSTEM_H_

#include "base/logging.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/pthread_thread_system.h"

namespace net_instaweb {

class Timer;

class NgxThreadSystem : public PthreadThreadSystem {
 public:
  NgxThreadSystem();
  virtual ~NgxThreadSystem();

  // In nginx we may only start threads after forking a worker process.  In
  // order to enforce this, we call PermitThreadStarting() in the worker process
  // right after forking, and CHECK-fail if something tries to start a thread
  // before then.
  void PermitThreadStarting();

 protected:
  virtual void BeforeThreadRunHook();

 private:
  bool may_start_threads_;

  DISALLOW_COPY_AND_ASSIGN(NgxThreadSystem);
};

}  // namespace net_instaweb

#endif  // NGX_THREAD_SYSTEM_H_
