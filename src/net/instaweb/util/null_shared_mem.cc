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
//
// Author: morlovich@google.com (Maksim Orlovich)
//
// A stub implementation of shared memory for systems where unavailable.
// Fails all the operations.

#include "net/instaweb/util/public/null_shared_mem.h"

#include "base/logging.h"
#include "net/instaweb/util/public/message_handler.h"

namespace net_instaweb {

NullSharedMem::NullSharedMem() {
}

NullSharedMem::~NullSharedMem() {
}

size_t NullSharedMem::SharedMutexSize() const {
  return 1;
}

AbstractSharedMemSegment* NullSharedMem::CreateSegment(
    const GoogleString& name, size_t size, MessageHandler* handler) {
  handler->Message(kWarning, "Using null shared memory runtime.");
  return NULL;
}

AbstractSharedMemSegment* NullSharedMem::AttachToSegment(
    const GoogleString& name, size_t size, MessageHandler* handler) {
  return NULL;
}

void NullSharedMem::DestroySegment(const GoogleString& name,
                                   MessageHandler* handler) {
  // Bug-free client code should never call our DestroySegment since
  // Create/Attach will fail.
  LOG(DFATAL) << "Trying to destroy a segment that was never allocated:"
              << name;
}

}  // namespace net_instaweb
