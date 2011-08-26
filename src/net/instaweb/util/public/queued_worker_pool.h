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

// Author: jmarantz@google.com (Joshua Marantz)
//
// implements a simple worker pool, allowing arbitrary functions to run
// using a pool of threads of predefined maximum size.
//
// This differs from QueuedWorker, which always uses exactly one thread.
// In this interface, any task can be assigned to any thread.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_QUEUED_WORKER_POOL_H_
#define NET_INSTAWEB_UTIL_PUBLIC_QUEUED_WORKER_POOL_H_

#include <cstddef>  // for size_t
#include <deque>
#include <set>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class AbstractMutex;
class Function;
class QueuedWorker;

// Maintains a predefined number of worker threads, and dispatches any
// number of groups of sequential tasks to those threads.
class QueuedWorkerPool {
 public:
  QueuedWorkerPool(int max_workers, ThreadSystem* thread_system);
  ~QueuedWorkerPool();

  // Functions added to a Sequence will be run sequentially, though not
  // necessarily always from the same worker thread.  The scheduler will
  // continue to schedule new work added to the sequence until
  // FreeSequence is called.
  class Sequence {
   public:
    // Adds 'function' to a sequence.  Note that this can occur at any time
    // the sequence is live -- you can add functions to a sequence that has
    // already started processing.
    //
    // 'function' can be called any time after Add(), and may in fact be
    // called before Add() returns.
    //
    // Ownership of 'function' is transferred to the Sequence, which deletes
    // it after execution or upon cancellation due to shutdown.
    void Add(Function* function);

    // Determines whether the Sequence is currently running, or waiting to run.
    bool IsBusy();

   private:
    // Construct using QueuedWorkerPool::NewSequence().
    Sequence(ThreadSystem* thread_system, QueuedWorkerPool* pool);

    // Free by calling QueuedWorkerPool::FreeSequence().
    ~Sequence();

    // Resets a new or recycled Sequence to its original state.
    void Reset();

    // Waits for any currently active function to complete, deletes
    // any other outstanding functions.  During the shutdown process,
    // the Sequence will simply delete, without running, any function
    // added to it from another thread.
    //
    // This function blocks until shutdown is complete.
    void WaitForShutDown();

    // Puts the Sequence in shutdown mode, but does not block until shutdown
    // is complete.  Return 'true' if the sequence is inactive and thus can
    // be immediately recycled.
    bool InitiateShutDown();

    // Gets the next function in the sequence, and transfers ownership
    // the the caller.
    Function* NextFunction();

    friend class QueuedWorkerPool;
    std::deque<Function*> work_queue_;
    scoped_ptr<ThreadSystem::CondvarCapableMutex> sequence_mutex_;
    QueuedWorkerPool* pool_;
    bool shutdown_;
    bool active_;
    scoped_ptr<ThreadSystem::Condvar> termination_condvar_;

    DISALLOW_COPY_AND_ASSIGN(Sequence);
  };

  // Sequence is owned by the pool, and will be automatically freed when
  // the pool is finally freed (e.g. on server shutdown).  But the sequence
  // does *not* auto-destruct when complete; it must be explicitly freed
  // using FreeSequence().
  Sequence* NewSequence();  // Returns NULL if shutting down.

  // Shuts down a sequence and frees it.  This does *not* block waiting
  // for the Sequence to finish.
  void FreeSequence(Sequence* sequence);

  // Shuts down all Sequences and Worker threads, but does not delete the
  // sequences.  The sequences will be deleted when the pool is destructed.
  void ShutDown();

 private:
  friend class Sequence;
  void Run(Sequence* sequence, QueuedWorker* worker);
  void QueueSequence(Sequence* sequence);
  Sequence* AssignWorkerToNextSequence(QueuedWorker* worker);
  void SequenceNoLongerActive(Sequence* sequence);

  ThreadSystem* thread_system_;
  scoped_ptr<AbstractMutex> mutex_;

  // active_workers_ and available_workers_ are mutually exclusive.
  std::set<QueuedWorker*> active_workers_;
  std::vector<QueuedWorker*> available_workers_;

  // queued_sequences_ and free_sequences_ are mutually exclusive, but
  // all_sequences contains all of them.
  std::vector<Sequence*> all_sequences_;
  std::deque<Sequence*> queued_sequences_;
  std::vector<Sequence*> free_sequences_;

  size_t max_workers_;
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(QueuedWorkerPool);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_QUEUED_WORKER_POOL_H_
