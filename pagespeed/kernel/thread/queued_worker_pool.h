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

#ifndef PAGESPEED_KERNEL_THREAD_QUEUED_WORKER_POOL_H_
#define PAGESPEED_KERNEL_THREAD_QUEUED_WORKER_POOL_H_

#include <cstddef>  // for size_t
#include <deque>
#include <set>
#include <vector>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/thread/sequence.h"

namespace net_instaweb {

class QueuedWorker;
class Waveform;

// Maintains a predefined number of worker threads, and dispatches any
// number of groups of sequential tasks to those threads.
class QueuedWorkerPool {
 public:
  static const int kNoLoadShedding = -1;

  QueuedWorkerPool(int max_workers, StringPiece thread_name_base,
                   ThreadSystem* thread_system);
  ~QueuedWorkerPool();

  // Functions added to a Sequence will be run sequentially, though not
  // necessarily always from the same worker thread.  The scheduler will
  // continue to schedule new work added to the sequence until
  // FreeSequence is called.
  //
  // TODO(jmarantz): Make this subclass private (or just move it to the .cc
  // file) and change NewSequence to return a net_instaweb::Sequence*.
  class Sequence : public net_instaweb::Sequence {
   public:
    // AddFunction is a callback that when invoked queues another callback on
    // the given sequence, and when canceled queues a cancel call to the
    // sequence instead.  The cancellation behavior is what makes this different
    // from a simple call to MakeFunction(sequence, &Sequence::Add, callback).
    class AddFunction : public Function {
     public:
      AddFunction(net_instaweb::Sequence* sequence, Function* callback)
          : sequence_(sequence), callback_(callback) { }
      virtual ~AddFunction();

     protected:
      virtual void Run() {
        sequence_->Add(callback_);
      }
      virtual void Cancel() {
        sequence_->Add(MakeFunction(callback_, &Function::CallCancel));
      }

     private:
      net_instaweb::Sequence* sequence_;
      Function* callback_;
      DISALLOW_COPY_AND_ASSIGN(AddFunction);
    };

    // Adds 'function' to a sequence.  Note that this can occur at any time
    // the sequence is live -- you can add functions to a sequence that has
    // already started processing.
    //
    // 'function' can be called any time after Add(), and may in fact be
    // called before Add() returns.
    //
    // Ownership of 'function' is transferred to the Sequence, which deletes
    // it after execution or upon cancellation due to shutdown.
    //
    // If the pool is being shut down at the time Add is being called,
    // this method will call function->Cancel().
    void Add(Function* function) LOCKS_EXCLUDED(sequence_mutex_);

    void set_queue_size_stat(Waveform* x) { queue_size_ = x; }

    // Sets the maximum number of functions that can be enqueued to a sequence.
    // By default, sequences are unbounded.  When a bound is reached, the oldest
    // functions are retired by calling Cancel() on them.
    void set_max_queue_size(size_t x) { max_queue_size_ = x; }

    // Calls Cancel on all pending functions in the queue.
    void CancelPendingFunctions() LOCKS_EXCLUDED(sequence_mutex_);

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
    void WaitForShutDown() LOCKS_EXCLUDED(sequence_mutex_);

    // Puts the Sequence in shutdown mode, but does not block until shutdown
    // is complete.  Return 'true' if the sequence is inactive and thus can
    // be immediately recycled.
    bool InitiateShutDown() LOCKS_EXCLUDED(sequence_mutex_);

    // Gets the next function in the sequence, and transfers ownership
    // the the caller.
    Function* NextFunction() LOCKS_EXCLUDED(sequence_mutex_);

    bool IsBusy() EXCLUSIVE_LOCKS_REQUIRED(sequence_mutex_);

    // Returns number of tasks that were canceled.
    int CancelTasksOnWorkQueue() EXCLUSIVE_LOCKS_REQUIRED(sequence_mutex_);

    // Cancels all pending tasks (and updates stats appropriately).
    void Cancel() LOCKS_EXCLUDED(sequence_mutex_);

    friend class QueuedWorkerPool;
    std::deque<Function*> work_queue_ GUARDED_BY(sequence_mutex_);
    scoped_ptr<ThreadSystem::CondvarCapableMutex> sequence_mutex_;
    QueuedWorkerPool* pool_;
    bool shutdown_ GUARDED_BY(sequence_mutex_);
    bool active_ GUARDED_BY(sequence_mutex_);
    scoped_ptr<ThreadSystem::Condvar> termination_condvar_
        GUARDED_BY(sequence_mutex_);
    Waveform* queue_size_;
    size_t max_queue_size_;

    DISALLOW_COPY_AND_ASSIGN(Sequence);
  };

  typedef std::set<Sequence*> SequenceSet;

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
  //
  // Equivalent to "InitiateShutDown(); WaitForShutDownComplete();"
  void ShutDown();

  // Starts the shutdown process, preventing further tasks from being queued.
  // Does not wait for any active tasks to be completed.  This must be followed
  // by WaitForShutDownComplete.  It is invalid to call InitiateShutDown() twice
  // in a row.
  void InitiateShutDown();

  // Blocks waiting for all outstanding tasks to be completed.  Must be preceded
  // by InitiateShutDown().
  void WaitForShutDownComplete();

  // Returns true if any of the given sequences is busy. Note that multiple
  // sequences are checked atomically; otherwise we could end up missing
  // work. For example, consider if we had a sequence for main rewrite work,
  // and an another one for expensive work.
  // In this case, if we tried to check their busyness independently, the
  // following could happen:
  // 1) First portion of inexpensive work is done, so we queue up
  //    some on expensive work thread.
  // 2) We check whether inexpensive work sequence is busy. It's not.
  // 3) The expensive work runs, finishes, and queues up more inexpensive
  //    work.
  // 4) We check whether expensive sequence is busy. It's not, so we would
  //    conclude we quiesced --- while there was still work in the inexpensive
  //    queue.
  static bool AreBusy(const SequenceSet& sequences);

  // If x == kNoLoadShedding disables load-shedding.
  // Otherwise, if more than x sequences are queued waiting to run,
  // sequences will start getting dropped and canceled, with oldest
  // sequences canceled first.
  //
  // Precondition: x > 0 || x == kNoLoadShedding
  // x = kNoLoadShedding (the default) disables the limit.
  //
  // Should be called before starting any work.
  void SetLoadSheddingThreshold(int x);

  // Sets up a timed-variable statistic indicating the current queue depth.
  //
  // This must be called prior to creating sequences.
  void set_queue_size_stat(Waveform* x) { queue_size_ = x; }

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

  GoogleString thread_name_base_;

  size_t max_workers_;
  bool shutdown_;

  Waveform* queue_size_;
  int load_shedding_threshold_;

  DISALLOW_COPY_AND_ASSIGN(QueuedWorkerPool);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_THREAD_QUEUED_WORKER_POOL_H_
