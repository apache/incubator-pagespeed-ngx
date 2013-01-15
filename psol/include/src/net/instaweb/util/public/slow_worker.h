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
// This contains SlowWorker, which runs (expensive) tasks in a background thread
// when it's not otherwise busy.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_SLOW_WORKER_H_
#define NET_INSTAWEB_UTIL_PUBLIC_SLOW_WORKER_H_

#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/worker.h"

namespace net_instaweb {

class Function;
class ThreadSystem;

// See file comment.
class SlowWorker : public Worker {
 public:
  // Initializes the worker. You still need to call ->Start to actually
  // start the thread, however. (Note: start can return false on failure).
  explicit SlowWorker(ThreadSystem* runtime);

  // This waits for the running task to terminate.
  virtual ~SlowWorker();

  // If this SlowWorker's thread is currently idle, it will run the closure.
  // Otherwise, the closure will simply be deleted.
  //
  // Takes ownership of the closure.
  void RunIfNotBusy(Function* closure);

 private:
  virtual bool IsPermitted(Function* closure);

  DISALLOW_COPY_AND_ASSIGN(SlowWorker);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_SLOW_WORKER_H_
