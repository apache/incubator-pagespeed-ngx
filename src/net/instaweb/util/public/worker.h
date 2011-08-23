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
// This contains Worker, which is a base class for classes managing running
// of work in background.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_WORKER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_WORKER_H_

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"

namespace net_instaweb {

class Function;
class ThreadSystem;
class Waveform;

// This class is a base for various mechanisms of running things in background.
//
// If you just want to run something in background, you want to use a subclass
// of this, such as a SlowWorker or QueuedWorker instance.
//
// Subclasses should implement bool PermitQueue() and provide an appropriate
// wrapper around QueueIfPermitted().
class Worker {
 public:
  // Tries to start the thread. It will be cleaned up by ~Worker().
  // Returns whether successful.
  bool Start();

  // Tries to start the work thread if it hasn't been started already;
  // returns true if it was started successfully or was previously running.
  bool StartIfNeeded();

  // Returns true if there was a job running or any jobs queued at the time
  // this function was called.
  bool IsBusy();

  // Finishes the currently running jobs, and deletes any queued jobs.
  // No further jobs will be accepted after this call either; they will
  // just be deleted. It is safe to call this method multiple times.
  void ShutDown();

  // Sets up a timed-variable statistic indicating the current queue depth.
  //
  // This must be called prior to starting the thread.
  void set_queue_size_stat(Waveform* x) { queue_size_ = x; }

 protected:
  explicit Worker(ThreadSystem* runtime);
  virtual ~Worker();

  // If IsPermitted() returns true, queues up the given closure to be run,
  // takes ownership of closure, and returns true. (Also wakes up the work
  // thread to actually run it if it's idle)
  //
  // Otherwise it merely returns false, and doesn't do anything else.
  bool QueueIfPermitted(Function* closure);

  // Subclasses should implement this method to implement the policy
  // on whether to run given tasks or not.
  virtual bool IsPermitted(Function* closure) = 0;

  // Returns the number of jobs, including any running and queued jobs.
  // The lock semantics here are as follows:
  // - QueueIfPermitted calls IsPermitted with lock held.
  // - NumJobs assumes lock to be held.
  // => It's safe to call NumJobs from within IsPermitted if desired.
  int NumJobs();

 private:
  class WorkThread;
  friend class WorkThread;

  // This is called whenever a task is added or removed from the queue.
  void UpdateQueueSizeStat(int size);

  scoped_ptr<WorkThread> thread_;
  Waveform* queue_size_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_WORKER_H_
