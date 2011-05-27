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

// Unit-test for SlowWorker

#include "net/instaweb/util/public/slow_worker.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/worker.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {
namespace {

class SlowWorkerTest: public WorkerTestBase {
 public:
  SlowWorkerTest() {
    worker_.reset(new SlowWorker(thread_runtime_.get()));
  }

  ~SlowWorkerTest() {
    worker_.reset(NULL);
  }

 protected:
  scoped_ptr<SlowWorker> worker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SlowWorkerTest);
};

// Closure that signals on destruction and check fails when run.
class DeleteNotifyClosure : public Worker::Closure {
 public:
  explicit DeleteNotifyClosure(WorkerTestBase::SyncPoint* sync) : sync_(sync) {}
  virtual ~DeleteNotifyClosure() {
    sync_->Notify();
  }

  virtual void Run() {
    CHECK(false) << "SlowWorker ran a job while an another is active!?";
  }

 private:
  WorkerTestBase::SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(DeleteNotifyClosure);
};

TEST_F(SlowWorkerTest, BasicOperation) {
  // Add in a job that waits for our OK before finishing Run() and an another
  // job that fails if Run. Since we don't let the first one proceed, the second
  // one should be deleted immediately.
  SyncPoint start_sync(thread_runtime_.get());
  SyncPoint delete_sync(thread_runtime_.get());

  ASSERT_TRUE(worker_->Start());
  worker_->RunIfNotBusy(new WaitRunClosure(&start_sync));
  worker_->RunIfNotBusy(new DeleteNotifyClosure(&delete_sync));
  delete_sync.Wait();
  start_sync.Notify();

  // Make sure we kill the thread now, so we don't get it accessing
  // deleted start_sync after we exit if it's slow to start
  worker_.reset(NULL);
}

class WaitCancelClosure : public Worker::Closure {
 public:
  explicit WaitCancelClosure(WorkerTestBase::SyncPoint* sync) : sync_(sync) {}

  virtual void Run() {
    sync_->Notify();
    while (!quit_requested()) {
      usleep(10);
    }
  }

 private:
  WorkerTestBase::SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(WaitCancelClosure);
};

TEST_F(SlowWorkerTest, Cancellation) {
  // Used to wait for thread to actually start, so we don't see it
  // being deleted w/o being started as confirming that exit is working
  SyncPoint start_sync(thread_runtime_.get());

  ASSERT_TRUE(worker_->Start());
  worker_->RunIfNotBusy(new WaitCancelClosure(&start_sync));

  // Wait for the thread to start...
  start_sync.Wait();

  // Ask for exit and block on that.
  worker_.reset(NULL);
}

}  // namespace
}  // namespace net_instaweb
