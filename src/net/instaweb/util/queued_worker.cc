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
// Implements QueuedWorker, which runs tasks in a background thread.

#include "net/instaweb/util/public/queued_worker.h"

#include "base/logging.h"

namespace net_instaweb {

class Function;

QueuedWorker::QueuedWorker(ThreadSystem* runtime)
    : Worker(runtime) {
}

QueuedWorker::~QueuedWorker() {
}

void QueuedWorker::RunInWorkThread(Function* closure) {
  bool ok = QueueIfPermitted(closure);
  CHECK(ok);
}

bool QueuedWorker::IsPermitted(Function* closure) {
  return true;
}

}  // namespace net_instaweb
