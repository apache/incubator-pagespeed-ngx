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

#include "pagespeed/system/controller_manager.h"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>

#include "base/logging.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {


ControllerManager::ProcessDeathWatcherThread::ProcessDeathWatcherThread(
    ThreadSystem* thread_system,
    int controller_read_fd,
    MessageHandler* handler) : Thread(thread_system,
                                      "process death watcher",
                                      ThreadSystem::kDetached),
                               handler_(handler),
                               controller_read_fd_(controller_read_fd) {
}

void ControllerManager::ProcessDeathWatcherThread::Run() {
  handler_->Message(kInfo, "Watching the root process to exit if it does.");

  ssize_t status;
  char buf[1];
  do {
    status = read(controller_read_fd_, buf, 1);
  } while (status == -1 && (errno == EINTR ||
                            errno == EAGAIN));
  if (status == -1) {
    handler_->Message(
        kWarning, "Controller got error %d reading from pipe, shutting down",
        errno);
  } else if (status == 0 /* EOF */) {
    handler_->Message(kInfo, "Root process exited; controller shutting down.");
  } else if (status == 1 /* read a byte */) {
    handler_->Message(
        kInfo, "Root process is starting a new controller; shutting down.");
  } else {
    LOG(FATAL) << "Status of " << status << " doesn't make sense";
  }

  // TODO(jefftk): Once the controller is doing real work and there's real
  // cleanup work to do, we can initiate that from here.

  exit(EXIT_SUCCESS);

  // Because the controller is exiting via exit(), the babysitter process will
  // see that and quit itself instead of restarting it.
}

void ControllerManager::RunController(SystemRewriteDriverFactory* factory,
                                      MessageHandler* handler) {
  handler->Message(kInfo, "Controller running with PID %d", getpid());

  // Would set up gRPC server here and pass control to its event loop.  Instead
  // just hang out sleeping until the ProcessDeathWatcherThread decides it's
  // time to exit.
  while (true) {
    sleep(60);  // 1m
  }
}

void ControllerManager::Daemonize(MessageHandler* handler) {
  // Make a new session (process group).
  if (setsid() < 0) {
    handler->Message(kWarning, "Daemonize: Failed to setsid().");
  }

  // We need to fork again to make sure there is no session group leader.
  pid_t pid = fork();
  CHECK(pid != -1) << "Couldn't fork to daemonize.";
  if (pid != 0) {
    exit(EXIT_SUCCESS);
  }

  // If we keep the current directory we might keep them from being able to
  // unmount their filesystem.
  if (chdir("/") < 0) {
    handler->Message(kWarning, "Daemonize: Failed to chdir(/).");
  }

  // If we disconnect file descriptors then logging will break, so don't.
}

void ControllerManager::ForkControllerProcess(
    SystemRewriteDriverFactory* factory,
    ThreadSystem* thread_system,
    MessageHandler* handler) {

  handler->Message(kInfo, "Forking controller process off of %d", getpid());

  // Whenever we fork off a controller we save the fd for a pipe to it.  Then if
  // we fork off another controller we can write a byte to the pipe to tell the
  // old controller to clean up and exit.
  static int controller_write_fd = -1;
  if (controller_write_fd != -1) {
    // We already forked off a controller earlier.  Tell it to quit by writing a
    // byte.  If there's no one still with the pipe open we'll get SIGPIPE and
    // die horribly, but as long as the babysitter hasn't died that won't
    // happen.
    handler->Message(
        kInfo, "Writing a byte to a pipe to tell the old controller to exit.");
    ssize_t status;
    do {
      status = write(controller_write_fd, "Q", 1);
    } while (status == -1 && (errno == EAGAIN ||
                              errno == EINTR));
    if (status == -1) {
      handler->Message(kWarning, "killing old controller failed: %s",
                        strerror(errno));
    }
  }

  int file_descriptors[2];
  int pipe_status = pipe(file_descriptors);
  CHECK(pipe_status != -1) << "Couldn't create a root-controller pipe.";

  pid_t pid = fork();
  CHECK(pid != -1) << "Couldn't fork a controller babysitter process";

  if (pid != 0) {
    // Parent process.

    // Close the reading end of the pipe.  We'll never write to it, but when we
    // (and all our children) die there will be no more processes that could
    // potentially write to it, and so the people who do have it open for
    // reading can see that death.
    close(file_descriptors[0]);

    // Save the writing end of the pipe.
    controller_write_fd = file_descriptors[1];

    return;
  }

  // Now we're in the child process.  Set this up as a babysitter process,
  // that forks off a controller and restarts it if it dies.

  Daemonize(handler);

  // We need to clear inherited signal handlers. There's no portable way to get
  // a list of all possible signals, and they're not even guaranteed to be in
  // order.  But NSIG is usually defined these days, and if it is then we just
  // want ascending numbers up to to NSIG.
  for (int i = 0; i < NSIG; i++) {
    signal(i, SIG_DFL);
  }

  factory->PrepareForkedProcess("babysitter");

  // Close the writing end of the pipe.  If we read a byte from the pipe it
  // means we should quit because a new controller is starting up.  If we get
  // EOF from the pipe it means we should quit because the master process shut
  // down.
  close(file_descriptors[1]);
  int controller_read_fd = file_descriptors[0];

  handler->Message(kInfo, "Babysitter running with PID %d", getpid());

  while (true) {
    pid = fork();
    CHECK(pid != -1) << "Couldn't fork a controller process";

    if (pid == 0) {
      factory->PrepareForkedProcess("controller");
      factory->PrepareControllerProcess();

      // Start a thread to watch to see if the root or babysitter process
      // dies, and quit if it does.
      scoped_ptr<ProcessDeathWatcherThread> process_death_watcher_thread(
          new ProcessDeathWatcherThread(thread_system,
                                        controller_read_fd,
                                        handler));
      CHECK(process_death_watcher_thread->Start());

      RunController(factory, handler);
      LOG(FATAL) << "Controller should run until exit().";
    } else {
      // Wait for controller process to die, then continue with the loop by
      // restarting it.
      int status;
      pid_t child_pid;
      do {
        child_pid = waitpid(pid, &status, 0);
      } while (child_pid == -1 && errno == EINTR);
      CHECK(child_pid != -1) << "Call to waitpid failed with status "
                             << child_pid;
      if (WIFEXITED(status)) {
        handler->Message(kInfo,
            "Controller process %d exited normally, not restarting it. "
            "Shutting down babysitter.", child_pid);
        exit(EXIT_SUCCESS);
      }
      handler->Message(
          kWarning, "Controller process %d exited with status code %d",
          child_pid, status);
    }
  }
}

}  // namespace net_instaweb
