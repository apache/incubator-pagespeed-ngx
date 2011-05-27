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

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/condvar.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/worker.h"

#ifndef NET_INSTAWEB_UTIL_WORKER_TEST_BASE_H_
#define NET_INSTAWEB_UTIL_WORKER_TEST_BASE_H_

namespace net_instaweb {

class WorkerTestBase : public ::testing::Test {
 public:
  class CountClosure;
  class SyncPoint;
  class NotifyRunClosure;
  class WaitRunClosure;
  class FailureClosure;

  WorkerTestBase();
  ~WorkerTestBase();

 protected:
  scoped_ptr<ThreadSystem> thread_runtime_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerTestBase);
};

// A closure that increments a variable on running.
class WorkerTestBase::CountClosure : public Worker::Closure {
 public:
  explicit CountClosure(int* variable) : variable_(variable) {}

  virtual void Run() {
    ++*variable_;
  }

 private:
  int* variable_;
  DISALLOW_COPY_AND_ASSIGN(CountClosure);
};

// A way for one thread to wait for another.
class WorkerTestBase::SyncPoint {
 public:
  explicit SyncPoint(ThreadSystem* thread_system);

  void Wait();
  void Notify();

 private:
  bool done_;
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> notify_;
  DISALLOW_COPY_AND_ASSIGN(SyncPoint);
};

// Notifies of itself having run on a given SyncPoint.
class WorkerTestBase::NotifyRunClosure : public Worker::Closure {
 public:
  explicit NotifyRunClosure(SyncPoint* sync);
  virtual void Run();

 private:
  SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(NotifyRunClosure);
};

// Waits on a given SyncPoint before completing Run()
class WorkerTestBase::WaitRunClosure : public Worker::Closure {
 public:
  explicit WaitRunClosure(SyncPoint* sync);
  virtual void Run();

 private:
  SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(WaitRunClosure);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_WORKER_TEST_BASE_H_
