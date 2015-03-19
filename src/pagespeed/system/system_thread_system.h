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
//
// A wrapper around PthreadThreadSystem that takes care of some signal masking
// issues that arise in forking servers.  We prefer pthreads to APR as APR
// mutex, etc., creation requires pools which are generally thread unsafe,
// introducing some additional risks.

#ifndef PAGESPEED_SYSTEM_SYSTEM_THREAD_SYSTEM_H_
#define PAGESPEED_SYSTEM_SYSTEM_THREAD_SYSTEM_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/thread/pthread_thread_system.h"

namespace net_instaweb {

class SystemThreadSystem : public PthreadThreadSystem {
 public:
  SystemThreadSystem();
  virtual ~SystemThreadSystem();

  // It's not safe to start threads in a process that will later fork.  In order
  // to enforce this, call PermitThreadStarting() in the child process right
  // after forking, and DCHECK-fail if something tries to start a thread before
  // then.
  void PermitThreadStarting();

 protected:
  virtual void BeforeThreadRunHook();

 private:
  bool may_start_threads_;

  DISALLOW_COPY_AND_ASSIGN(SystemThreadSystem);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_SYSTEM_THREAD_SYSTEM_H_
