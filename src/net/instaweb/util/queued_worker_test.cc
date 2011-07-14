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

// Unit-test for QueuedWorker

#include "net/instaweb/util/public/queued_worker.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {
namespace {

class QueuedWorkerTest: public WorkerTestBase {
 public:
  QueuedWorkerTest() : worker_(new QueuedWorker(thread_runtime_.get())) {}

 protected:
  scoped_ptr<QueuedWorker> worker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueuedWorkerTest);
};

// A closure that enqueues a new version of itself 'count' times.
class ChainedTask : public Function {
 public:
  ChainedTask(int* count, QueuedWorker* worker)
      : count_(count),
        worker_(worker) {
  }

  virtual void Run() {
    --*count_;
    if (*count_ > 0) {
      worker_->RunInWorkThread(new ChainedTask(count_, worker_));
    }
  }

 private:
  int* count_;
  QueuedWorker* worker_;

  DISALLOW_COPY_AND_ASSIGN(ChainedTask);
};

TEST_F(QueuedWorkerTest, BasicOperation) {
  // All the jobs we queued should be run in order
  const int kBound = 42;
  int count = 0;
  SyncPoint sync(thread_runtime_.get());

  ASSERT_TRUE(worker_->Start());
  for (int i = 0; i < kBound; ++i) {
    worker_->RunInWorkThread(new CountFunction(&count));
  }

  worker_->RunInWorkThread(new NotifyRunFunction(&sync));
  sync.Wait();
  EXPECT_EQ(kBound, count);
}

TEST_F(QueuedWorkerTest, ChainedTasks) {
  // The ChainedTask closure ensures that there is always a task
  // queued until we've executed all 11 tasks in the chain, at which
  // point the 'idle' callback fires and we can complete the test.
  int count = 11;
  SyncPoint sync(thread_runtime_.get());
  worker_->set_idle_callback(new NotifyRunFunction(&sync));
  ASSERT_TRUE(worker_->Start());
  worker_->RunInWorkThread(new ChainedTask(&count, worker_.get()));
  sync.Wait();
  EXPECT_EQ(0, count);
}

TEST_F(QueuedWorkerTest, TestShutDown) {
  // Make sure that shutdown cancels jobs put in after it --- that
  // the job gets deleted (making clean.Wait() return), and doesn't
  // run (which would CHECK(false)).
  SyncPoint clean(thread_runtime_.get());
  ASSERT_TRUE(worker_->Start());
  worker_->ShutDown();
  worker_->RunInWorkThread(new DeleteNotifyFunction(&clean));
  clean.Wait();
}

TEST_F(QueuedWorkerTest, TestIsBusy) {
  ASSERT_TRUE(worker_->Start());
  EXPECT_FALSE(worker_->IsBusy());

  SyncPoint start_sync(thread_runtime_.get());
  worker_->RunInWorkThread(new WaitRunFunction(&start_sync));
  EXPECT_TRUE(worker_->IsBusy());
  start_sync.Notify();
  worker_->ShutDown();
  EXPECT_FALSE(worker_->IsBusy());
}

}  // namespace
}  // namespace net_instaweb
