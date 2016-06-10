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

#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

int ControllerManager::controller_write_fd_ = -1;

ControllerManager::ProcessDeathWatcherThread::ProcessDeathWatcherThread(
    ThreadSystem* thread_system, int controller_read_fd,
    ControllerProcess* process, MessageHandler* handler)
    : Thread(thread_system, "process death watcher", ThreadSystem::kJoinable),
      handler_(handler),
      parent_read_fd_(controller_read_fd),
      stop_read_fd_(-1),
      stop_write_fd_(-1),
      process_(process),
      parent_death_detected_(false) {
  int fds[2];
  if (pipe(fds) < 0) {
    LOG(FATAL) << "ProcessDeathWatcherThread: pipe failed: " << strerror(errno);
    exit(1);  // NOTREACHED
  }
  stop_read_fd_ = fds[0];
  stop_write_fd_ = fds[1];
}

ControllerManager::ProcessDeathWatcherThread::~ProcessDeathWatcherThread() {
  close(parent_read_fd_);
  close(stop_read_fd_);
  close(stop_write_fd_);  // May be -1.
}

void ControllerManager::ProcessDeathWatcherThread::Stop() {
  if (stop_write_fd_ >= 0) {
    close(stop_write_fd_);
    stop_write_fd_ = -1;
  }
  this->Join();
}

void ControllerManager::ProcessDeathWatcherThread::Run() {
  CHECK_GE(stop_read_fd_, 0);
  CHECK_GE(parent_read_fd_, 0);

  // This message is used by system/system_test.sh.
  handler_->Message(kInfo, "Watching the root process to exit if it dies.");

  struct pollfd fds[2];
  memset(&fds, 0, sizeof(fds));
  fds[0].fd = parent_read_fd_;
  fds[0].events = POLLIN;
  fds[1].fd = stop_read_fd_;
  fds[1].events = POLLIN;

  int nready = 0;
  while (nready <= 0) {
    nready = poll(fds, arraysize(fds), -1 /* infinite timeout */);

    // Activity on parent_read_fd_. That means the Apache/Nginx root either died
    // or asked us to quit.
    if (fds[0].revents) {
      DCHECK_EQ(fds[0].fd, parent_read_fd_);
      parent_death_detected_ = true;

      char buf[1];
      ssize_t status = read(parent_read_fd_, buf, 1);
      if (status == -1) {
        // It's very unlikely, but it could be that errno is EINTR here.
        // Given that these messages are diagnostic only, it's just fine to
        // ignore that and just exit the loop anyway.
        handler_->Message(
            kWarning,
            "Controller got error %d reading from pipe, shutting down", errno);
      } else if (status == 0 /* EOF */) {
        handler_->Message(kInfo,
                          "Root process exited; controller shutting down.");
      } else if (status == 1 /* read a byte */) {
        handler_->Message(
            kInfo, "Root process is starting a new controller; shutting down.");
      } else {
        LOG(FATAL) << "Status of " << status << " doesn't make sense";
        exit(1);  // NOTREACHED
      }
      // Note that it is possible that ControllerProcess::Run has already exited
      // at this point. However, the API requires that calling Stop() is still
      // OK.
      process_->Stop();
    }

    // Activity on stop_read_fd_. That means ControllerProcess::Run completed
    // and now we are being shutdown.
    if (fds[1].revents) {
      DCHECK_EQ(fds[1].fd, stop_read_fd_);
      handler_->Message(kInfo,
                        "Child process complete, stopping root watcher.");
    }
  }
}

void ControllerManager::DetachFromControllerProcess() {
  if (controller_write_fd_ != -1) {
    close(controller_write_fd_);
    controller_write_fd_ = -1;
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

int ControllerManager::RunController(int controller_read_fd,
                                     ControllerProcess* process,
                                     ThreadSystem* thread_system,
                                     MessageHandler* handler) {
  int exit_status = process->Setup();
  if (exit_status == 0) {
    // Start a thread to watch to see if the root process dies,
    // and quit if it does.
    std::unique_ptr<ProcessDeathWatcherThread> process_death_watcher_thread(
        new ProcessDeathWatcherThread(thread_system, controller_read_fd,
                                      process, handler));
    CHECK(process_death_watcher_thread->Start());

    exit_status = process->Run();
    process_death_watcher_thread->Stop();

    // Run may have returned because the parent died, or because of voluntary
    // exit. If the parent died, we need to trap that and force the exit status
    // to zero, otherwise the babysitter will unnecessarily respawn us.
    if (process_death_watcher_thread->parent_death_detected()) {
      exit_status = 0;
    }
  }
  return exit_status;
}

void ControllerManager::ForkControllerProcess(
    std::unique_ptr<ControllerProcess>&& process,
    SystemRewriteDriverFactory* factory,
    ThreadSystem* thread_system,
    MessageHandler* handler) {
  handler->Message(kInfo, "Forking controller process from PID %d", getpid());

  // Whenever we fork off a controller we save the fd for a pipe to it.  Then if
  // we fork off another controller we can write a byte to the pipe to tell the
  // old controller to clean up and exit.
  if (controller_write_fd_ != -1) {
    // We already forked off a controller earlier.  Tell it to quit by writing a
    // byte.  If there's no one still with the pipe open we'll get SIGPIPE and
    // die horribly, but as long as the babysitter hasn't died that won't
    // happen.
    handler->Message(
        kInfo, "Writing a byte to a pipe to tell the old controller to exit.");
    ssize_t status;
    do {
      status = write(controller_write_fd_, "Q", 1);
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
    controller_write_fd_ = file_descriptors[1];

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

  // This message is used by system/system_test.sh.
  handler->Message(kInfo, "Babysitter running with PID %d", getpid());

  while (true) {
    pid = fork();
    CHECK(pid != -1) << "Couldn't fork a controller process";

    if (pid == 0) {
      factory->PrepareForkedProcess("controller");
      factory->PrepareControllerProcess();
      // This message is used by get_controller_pid in system/system_test.sh.
      handler->Message(kInfo, "Controller running with PID %d", getpid());
      int exit_status = RunController(controller_read_fd, process.get(),
                                      thread_system, handler);
      handler->Message(kInfo, "Controller %d exiting with status %d",
                       getpid(), exit_status);
      exit(exit_status);
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
      if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
        handler->Message(kInfo,
            "Controller process %d exited normally, not restarting it. "
            "Shutting down babysitter.", child_pid);
        exit(EXIT_SUCCESS);
      }
      // system/system_test.sh and the nginx system test look at these messages.
      handler->Message(
          kWarning, "Controller process %d exited with wait status %d",
          child_pid, status);
      // If the controller used an unclean exit, it probably had a problem
      // binding to a port or similar. Don't try and restart it immediately.
      if (WIFEXITED(status)) {
        sleep(1);
      }
    }
  }
}

}  // namespace net_instaweb
