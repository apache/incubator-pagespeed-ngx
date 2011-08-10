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
// Implements Worker, base class for various run-in-a-thread classes,
// via Worker::WorkThread.

#include "net/instaweb/util/public/worker.h"

#include <deque>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/thread.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/waveform.h"

namespace net_instaweb {

void Worker::RunIdleCallback() {
  if (idle_callback_.get() != NULL) {
    idle_callback_->Run();
  }
}

// The actual thread that does the work.
class Worker::WorkThread : public ThreadSystem::Thread {
 public:
  WorkThread(Worker* owner, ThreadSystem* runtime)
    : Thread(runtime, ThreadSystem::kJoinable),
      owner_(owner),
      mutex_(runtime->NewMutex()),
      state_change_(mutex_->NewCondvar()),
      current_task_(NULL),
      exit_(false),
      started_(false) {
  }

  // If worker thread exit is requested, returns false.
  // Returns true and fetches next task into current_task_ otherwise.
  // Takes care of synchronization, including waiting for next state change.
  bool WaitForNextTask() {
    ScopedMutex lock(mutex_.get());

    bool was_running = (current_task_ != NULL);

    // Clean any task we were running last iteration
    delete current_task_;
    current_task_ = NULL;

    while (!exit_ && tasks_.empty()) {
      if (was_running) {
        was_running = false;
        owner_->RunIdleCallback();
      }
      state_change_->Wait();
    }

    // Handle exit.
    if (exit_) {
      if (was_running && tasks_.empty()) {
        owner_->RunIdleCallback();
      }
      return false;
    }

    // Get task.
    current_task_ = tasks_.front();
    tasks_.pop_front();
    owner_->UpdateQueueSizeStat(tasks_.size());

    return true;
  }

  virtual void Run() {
    while (WaitForNextTask()) {
      // Run tasks (not holding the lock, so new tasks can be added).
      current_task_->Run();
    }
  }

  void ShutDown() {
    if (!started_) {
      return;
    }

    {
      ScopedMutex lock(mutex_.get());

      exit_ = true;
      if (current_task_ != NULL) {
        current_task_->set_quit_requested(true);
      }
      state_change_->Signal();
    }

    Join();
    STLDeleteElements(&tasks_);
    owner_->UpdateQueueSizeStat(0);
    started_ = false;  // Reject further jobs on explicit shutdown.
  }

  bool Start() {
    started_ = Thread::Start();
    return started_;
  }

  bool StartIfNeeded() {
    if (started_) {
      return true;
    }
    return Start();
  }

  bool QueueIfPermitted(Function* closure) {
    if (!started_) {
      delete closure;
      return true;
    }

    ScopedMutex lock(mutex_.get());
    if (owner_->IsPermitted(closure)) {
      tasks_.push_back(closure);
      owner_->UpdateQueueSizeStat(tasks_.size());
      if (current_task_ == NULL) {  // wake the thread up if it's idle.
        state_change_->Signal();
      }
      return true;
    } else {
      return false;
    }
  }

  // Must be called with mutex held.
  int NumJobs() {
    int num = static_cast<int>(tasks_.size());
    if (current_task_ != NULL) {
      ++num;
    }
    return num;
  }

  bool IsBusy() const {
    ScopedMutex lock(mutex_.get());
    return (current_task_ != NULL) || !tasks_.empty();
  }

 private:
  Worker* owner_;

  // guards state_change_, current_task_, tasks_, exit_;
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> state_change_;
  Function* current_task_;  // non-NULL if we are actually running something.
  std::deque<Function*> tasks_;  // things waiting to be run.
  bool exit_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(WorkThread);
};

Worker::Worker(ThreadSystem* runtime)
    : thread_(new WorkThread(this, runtime)),
      queue_size_(NULL) {
}

Worker::~Worker() {
  thread_->ShutDown();
}

bool Worker::Start() {
  return thread_->Start();
}

bool Worker::StartIfNeeded() {
  return thread_->StartIfNeeded();
}

bool Worker::IsBusy() {
  return thread_->IsBusy();
}

bool Worker::QueueIfPermitted(Function* closure) {
  return thread_->QueueIfPermitted(closure);
}

int Worker::NumJobs() {
  return thread_->NumJobs();
}

void Worker::ShutDown() {
  thread_->ShutDown();
}

void Worker::UpdateQueueSizeStat(int value) {
  if (queue_size_ != NULL) {
    queue_size_->Add(value);
  }
}


}  // namespace net_instaweb
