// Copyright 2016 Google Inc.
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

#ifndef PAGESPEED_SYSTEM_CONTROLLER_PROCESS_H_
#define PAGESPEED_SYSTEM_CONTROLLER_PROCESS_H_

#include "base/macros.h"

namespace net_instaweb {

// Abstract class for delegating work to ControllerManager. All of the following
// is performed in the child process. If the child dies with a non-zero exit
// status, the babysitter will restart it.
//
// The ControllerManager will first invoke Setup(). If that returns non-zero,
// the forked process will immediately exit using that return code.
// Otherwise, the ControllerManager starts a thread monitoring for parent death
// and invokes Run(). This means Run() and Stop() may be invoked in any order
// and will be invoked from different threads. The child will exit with the
// return value of Run() unless Stop() is called (see below).
//
// Note that in the process from which ForkControllerProcess is invoked, this
// object will be created and then destroyed without any of the methods being
// invoked.

class ControllerProcess {
 public:
  ControllerProcess() {}
  virtual ~ControllerProcess() {}

  // Perform any required setup actions. We don't respond to the death of the
  // Apache/Nginx process that spawned us until Setup() returns, so should not
  // spin waiting for a resource. Returns exit status.
  virtual int Setup() { return 0; }

  // Perform your work and return the exit status. Invoked only if Setup()
  // returns 0. Stop() may be called before Run(), in which case Run() should
  // return immediately.
  //
  // The babysitter respawns the child whenever it dies for any reason other
  // than exit(0). Thus, if the child is dying because it was asked to by the
  // parent process (ie: Stop() was called) the return value of Run() is ignored
  // and an exit status of 0 will be used.
  virtual int Run() = 0;

  // Notify the Run() thread to stop. Called from a different thread so must be
  // thread safe. May be called before, during or after Run(), possibly multiple
  // times.
  virtual void Stop() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ControllerProcess);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_CONTROLLER_PROCESS_H_
