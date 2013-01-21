// Copyright 2011 Google Inc.
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

#ifndef MOD_SPDY_COMMON_EXECUTOR_H_
#define MOD_SPDY_COMMON_EXECUTOR_H_

#include "base/basictypes.h"
#include "net/spdy/spdy_protocol.h"

namespace net_instaweb { class Function; }

namespace mod_spdy {

// An interface for a service that can execute tasks.  A thread pool (using
// net_instaweb::QueuedWorkerPool or an apr_thread_pool_t) would be one obvious
// implementation.  In the future we may want to adjust this interface for use
// in an event-driven environment (e.g. Nginx).
class Executor {
 public:
  Executor();
  virtual ~Executor();

  // Add a new task to be run; the executor takes ownership of the task.  The
  // priority argument hints at how important this task is to get done, but the
  // executor is free to ignore it.  If Stop has already been called, the
  // executor may immediately cancel the task rather than running it.
  virtual void AddTask(net_instaweb::Function* task,
                       net::SpdyPriority priority) = 0;

  // Stop the executor.  Cancel all tasks that were pushed onto this executor
  // but that have not yet begun to run.  Tasks that were already running will
  // continue to run, and this function must block until they have completed.
  // It must be safe to call this method more than once.
  virtual void Stop() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Executor);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_EXECUTOR_H_
