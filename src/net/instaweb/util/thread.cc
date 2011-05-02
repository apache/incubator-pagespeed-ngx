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
// Implementation of the Thread class, which routes things to an underlying
// ThreadImpl.

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/thread.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

ThreadSystem::Thread::Thread(ThreadSystem* runtime, ThreadFlags flags)
    : impl_(runtime->NewThreadImpl(this, flags)) {
}

ThreadSystem::Thread::~Thread() {
}

bool ThreadSystem::Thread::Start() {
  return impl_->StartImpl();
}

void ThreadSystem::Thread::Join() {
  impl_->JoinImpl();
}

}  // namespace net_instaweb
