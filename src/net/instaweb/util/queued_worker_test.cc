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
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/worker_test_base.h"

namespace net_instaweb {
namespace {

class QueuedWorkerTest: public WorkerTestBase {
 public:
  QueuedWorkerTest() {
    worker_.reset(new QueuedWorker(thread_runtime_.get()));
  }

  ~QueuedWorkerTest() {
    worker_.reset(NULL);
  }

 protected:
  scoped_ptr<QueuedWorker> worker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueuedWorkerTest);
};

TEST_F(QueuedWorkerTest, BasicOperation) {
  // All the jobs we queued should be run in order
  const int kBound = 42;
  int count = 0;
  SyncPoint sync(thread_runtime_.get());

  ASSERT_TRUE(worker_->Start());
  for (int i = 0; i < kBound; ++i) {
    worker_->RunInWorkThread(new CountClosure(&count));
  }

  worker_->RunInWorkThread(new NotifyRunClosure(&sync));
  sync.Wait();
  EXPECT_EQ(kBound, count);
}

}  // namespace
}  // namespace net_instaweb
