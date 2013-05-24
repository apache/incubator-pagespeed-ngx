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

// This contains things that are common between unit tests for Worker and its
// subclasses, such as runtime creation and various closures.

#include "pagespeed/kernel/thread/worker_test_base.h"

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

WorkerTestBase::WorkerTestBase() {
  thread_runtime_.reset(Platform::CreateThreadSystem());
}

WorkerTestBase::~WorkerTestBase() {
}

WorkerTestBase::SyncPoint::SyncPoint(ThreadSystem* thread_system)
    : done_(false),
      mutex_(thread_system->NewMutex()),
      notify_(mutex_->NewCondvar()) {}

void WorkerTestBase::SyncPoint::Wait() {
  ScopedMutex lock(mutex_.get());
  while (!done_) {
    notify_->Wait();
  }
}

void WorkerTestBase::SyncPoint::Notify() {
  ScopedMutex lock(mutex_.get());
  done_ = true;
  notify_->Signal();
}

WorkerTestBase::NotifyRunFunction::NotifyRunFunction(SyncPoint* sync)
    : sync_(sync) {}

void WorkerTestBase::NotifyRunFunction::Run() {
  sync_->Notify();
}

WorkerTestBase::WaitRunFunction::WaitRunFunction(SyncPoint* sync)
    : sync_(sync) {}

void WorkerTestBase::WaitRunFunction::Run() {
  sync_->Wait();
}

}  // namespace net_instaweb
