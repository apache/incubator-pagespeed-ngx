// Copyright 2012 Google Inc.
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

#ifndef MOD_SPDY_COMMON_THREAD_POOL_H_
#define MOD_SPDY_COMMON_THREAD_POOL_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "net/spdy/spdy_protocol.h"  // for net::SpdyPriority

namespace net_instaweb { class Function; }

namespace mod_spdy {

class Executor;

// A ThreadPool keeps a pool of threads waiting to perform tasks.  One can
// create any number of Executor objects, using the NewExecutor method, which
// will all share the threads for executing tasks.  If more tasks are queued
// than there are threads in the pool, these executors will respect task
// priorities when deciding which tasks to execute first.
class ThreadPool {
 public:
  // Create a new thread pool that uses at least min_threads threads, and at
  // most max_threads threads, at a time.  min_threads must be no greater than
  // max_threads, and both must be positive.
  ThreadPool(int min_threads, int max_threads);

  // As above, but specify the amount of time after which to kill idle threads,
  // rather than using the default value (this is primarily for testing).
  // max_thread_idle_time must be non-negative.
  ThreadPool(int min_threads, int max_threads,
             base::TimeDelta max_thread_idle_time);

  // The destructor will block until all threads in the pool have shut down.
  // The ThreadPool must not be destroyed until all Executor objects returned
  // from the NewExecutor method have first been deleted.
  ~ThreadPool();

  // Start up the thread pool.  Must be called exactly one before using the
  // thread pool; returns true on success, or false on failure.  If startup
  // fails, the ThreadPool must be immediately deleted.
  bool Start();

  // Return a new Executor object that uses this thread pool to perform tasks.
  // The caller gains ownership of the returned Executor, and the ThreadPool
  // must outlive the returned Executor.
  Executor* NewExecutor();

  // Return the current total number of worker threads.  This is provided for
  // testing purposes only.
  int GetNumWorkersForTest();
  // Return the number of worker threads currently idle.  This is provided for
  // testing purposes only.
  int GetNumIdleWorkersForTest();
  // Return the number of terminated (zombie) threads that have yet to be
  // reaped.  This is provided for testing purposes only.
  int GetNumZombiesForTest();

 private:
  class ThreadPoolExecutor;
  class WorkerThread;

  // A Task is a simple pair of the Function to run, and the executor to which
  // the task was added.
  struct Task {
    Task(net_instaweb::Function* fun, ThreadPoolExecutor* own)
        : function(fun), owner(own) {}
    net_instaweb::Function* function;
    ThreadPoolExecutor* owner;
  };

  typedef std::multimap<net::SpdyPriority, Task> TaskQueue;
  typedef std::map<const ThreadPoolExecutor*, int> OwnerMap;

  // Start a new worker thread if 1) the task queue is larger than the number
  // of currently idle workers, and 2) we have fewer than the maximum number of
  // workers.  Otherwise, do nothing.  Must be holding lock_ when calling this.
  void StartNewWorkerIfNeeded();

  // Join and delete all worker threads in the given set.  This will block
  // until all the threads have terminated and been cleaned up, so don't call
  // this while holding the lock_.
  static void JoinThreads(const std::set<WorkerThread*>& threads);

  // These calls are used to implement the WorkerThread's main function.  Must
  // be holding lock_ when calling any of these.
  bool TryZombifyIdleThread(WorkerThread* thread);
  Task GetNextTask();
  void OnTaskComplete(Task task);

  // The min and max number of threads passed to the constructor.  Although the
  // constructor takes signed ints (for convenience), we store these unsigned
  // to avoid the need for static_casts when comparing against workers_.size().
  const unsigned int min_threads_;
  const unsigned int max_threads_;
  const base::TimeDelta max_thread_idle_time_;
  // This single master lock protects all of the below fields, as well as any
  // mutable data and condition variables in the worker threads and executors.
  // Having just one lock makes everything much easier to understand.
  base::Lock lock_;
  // Workers wait on this condvar when waiting for a new task.  We signal it
  // when a new task becomes available, or when we need to shut down.
  base::ConditionVariable worker_condvar_;
  // The list of running worker threads.  We keep this around so that we can
  // join the threads on shutdown.
  std::set<WorkerThread*> workers_;
  // Worker threads that have shut themselves down (due to being idle), and are
  // awaiting cleanup by the master thread.
  std::set<WorkerThread*> zombies_;
  // How many workers do we have that are actually executing tasks?
  unsigned int num_busy_workers_;
  // We set this to true to tell the worker threads to terminate.
  bool shutting_down_;
  // The priority queue of pending tasks.  Invariant: all Function objects in
  // the queue have neither been started nor cancelled yet.
  TaskQueue task_queue_;
  // This maps executors to the number of currently running tasks for that
  // executor; we increment when we start a task, and decrement when we finish
  // it.  If the number is zero, we remove the entry from the map; thus, as an
  // invariant the map only contains entries for executors with active tasks.
  OwnerMap active_task_counts_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPool);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_THREAD_POOL_H_
