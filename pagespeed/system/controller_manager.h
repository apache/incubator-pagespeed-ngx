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
// Author: jefftk@google.com (Jeff Kaufman)

#ifndef PAGESPEED_CONTROLLER_MANAGER_H_
#define PAGESPEED_CONTROLLER_MANAGER_H_

#include <memory>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/system/controller_process.h"
#include "pagespeed/system/system_rewrite_driver_factory.h"

namespace net_instaweb {

// Handles forking off a controller process, restarting it if it dies, and
// shutting down the process if the host reloads config or shuts down.
//
// We fork a babysitter process, which forks a controller process.  If the
// controller process dies without calling exit(0) the babysitter will fork off
// another controller.
//
// The controller runs a thread that watches for the root process to die, or to
// ask it to quit.  We use pipes for communication between the master process
// and the controller.  If the master process goes away, the controller reading
// will get EOF.  If the master process wants the controller to shut down so it
// can be replaced, it writes a byte.
//
// (All methods in the ControllerManager are static.  When you call
//  ForkControllerProcess() it keeps running until process exit.)
class ControllerManager {
 public:
  // Called on system startup, before forking off any workers.  Starts up a
  // babysitter process that starts a controller process and restarts the
  // controller if it dies.  Also called (again) on configuration reloading.
  static void ForkControllerProcess(
      std::unique_ptr<ControllerProcess>&& process,
      SystemRewriteDriverFactory* factory, ThreadSystem* thread_system,
      MessageHandler* handler);

  // Relinquishes the reference from us to the controller process. This may be
  // needed if our current process is going to go on and do something unrelated.
  static void DetachFromControllerProcess();

 private:
  // Set us up as a proper daemon, with no stdin/out/err and not process group.
  static void Daemonize(MessageHandler* handler);

  // Actually start the ControllerProcess. Returns an exit status.
  static int RunController(int controller_read_fd, ControllerProcess* process,
                           ThreadSystem* thread_system,
                           MessageHandler* handler);

  class ProcessDeathWatcherThread : public ThreadSystem::Thread {
   public:
    // Takes ownership of parent_read_fd, in that it will be closed on
    // destruction.
    ProcessDeathWatcherThread(ThreadSystem* thread_system,
                              int parent_read_fd,
                              ControllerProcess* process,
                              MessageHandler* handler);

    ~ProcessDeathWatcherThread();

    void Run() override;

    // Called from another thread so must be thread safe. Blocks until the
    // thread has exited.
    void Stop();

    bool parent_death_detected() const { return parent_death_detected_; }

   private:
    MessageHandler* handler_;
    const int parent_read_fd_;
    int stop_read_fd_;
    int stop_write_fd_;
    ControllerProcess* process_;
    bool parent_death_detected_;

    DISALLOW_COPY_AND_ASSIGN(ProcessDeathWatcherThread);
  };

  static int controller_write_fd_;

  DISALLOW_COPY_AND_ASSIGN(ControllerManager);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_MANAGER_H_
