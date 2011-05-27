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
// This contains QueuedWorker, which runs tasks in a background thread
// in FIFO order.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_QUEUED_WORKER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_QUEUED_WORKER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/worker.h"

namespace net_instaweb {

class ThreadSystem;

// See file comment.
class QueuedWorker : public Worker {
 public:
  // Initializes the worker. You still need to call ->Start to actually
  // start the thread, however. (Note: start can return false on failure).
  explicit QueuedWorker(ThreadSystem* runtime);

  // This waits for the running task to terminate.
  virtual ~QueuedWorker();

  // Runs the given closure in the work thread. Not that it's possible for the
  // closure to be deleted without running in case where the system is shutting
  // down.
  //
  // Takes ownership of the closure.
  void RunInWorkThread(Closure* closure);

 private:
  virtual bool IsPermitted(Closure* closure);

  DISALLOW_COPY_AND_ASSIGN(QueuedWorker);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_QUEUED_WORKER_H_
