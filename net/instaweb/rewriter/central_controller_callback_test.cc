/*
 * Copyright 2015 Google Inc.
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
// Author: cheesy@google.com (Steve Hill)

#include "net/instaweb/rewriter/public/central_controller_callback.h"
#include "net/instaweb/rewriter/public/central_controller_interface.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"

namespace net_instaweb {

namespace {

class MockCallbackHandle;

struct CallCounts {
  explicit CallCounts(ThreadSystem* ts)
      : run_called(0), cancel_called(0), delete_called(0), sync(ts) {}

  int run_called;
  int cancel_called;
  int delete_called;
  WorkerTestBase::SyncPoint sync;
  scoped_ptr<MockCallbackHandle> handle;
};

class MockCallbackHandle {
 public:
  explicit MockCallbackHandle(CallCounts* counts) : counts_(counts) {
  }

  ~MockCallbackHandle() { ++counts_->delete_called; }

  struct CallCounts* counts_;
};

class MockCentralControllerCallback
    : public CentralControllerCallback<MockCallbackHandle> {
 public:
  MockCentralControllerCallback(QueuedWorkerPool::Sequence* sequence,
                                struct CallCounts* counts)
      : CentralControllerCallback<
            MockCallbackHandle>::CentralControllerCallback(sequence),
        counts_(counts), steal_pointer_(false) {}

  virtual ~MockCentralControllerCallback() {}

  MockCallbackHandle* CreateTransactionContext(
      net_instaweb::CentralControllerInterface* interface) {
    return new MockCallbackHandle(counts_);
  }

  virtual void RunImpl(scoped_ptr<MockCallbackHandle>* handle) {
    ++counts_->run_called;
    if (steal_pointer_) {
      counts_->handle.reset(handle->release());
    }
    counts_->sync.Notify();
  }

  virtual void CancelImpl() {
    ++counts_->cancel_called;
    counts_->sync.Notify();
  }

  void SetStealPointer(bool steal) { steal_pointer_ = steal; }

 private:
  struct CallCounts* counts_;
  bool steal_pointer_;
};

// Inheriting from WorkerTestBase since it has useful stuff like SyncPoint.
class CentralControllerCallbackTest : public WorkerTestBase {
 public:
  CentralControllerCallbackTest()
      : worker_(new QueuedWorkerPool(2, "central_controller_test1",
                                     thread_runtime_.get())) {}

 protected:
  scoped_ptr<QueuedWorkerPool> worker_;

  void WaitUntilSequenceCompletes(QueuedWorkerPool::Sequence* sequence) {
    SyncPoint done(thread_runtime_.get());
    sequence->Add(new NotifyRunFunction(&done));
    done.Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CentralControllerCallbackTest);
};

TEST_F(CentralControllerCallbackTest, RegularRun) {
  struct CallCounts counts(thread_runtime_.get());
  QueuedWorkerPool::Sequence* sequence = worker_->NewSequence();
  sequence->Add(new MockCentralControllerCallback(sequence, &counts));

  counts.sync.Wait();
  WaitUntilSequenceCompletes(sequence);

  EXPECT_EQ(1, counts.run_called);
  EXPECT_EQ(1, counts.delete_called);
  EXPECT_EQ(0, counts.cancel_called);

  worker_->FreeSequence(sequence);
}

TEST_F(CentralControllerCallbackTest, CancelImmediately) {
  struct CallCounts counts(thread_runtime_.get());
  QueuedWorkerPool::Sequence* sequence = worker_->NewSequence();
  sequence->Add(
      MakeFunction(static_cast<Function*>(
                       new MockCentralControllerCallback(sequence, &counts)),
                   &Function::CallCancel));

  counts.sync.Wait();
  WaitUntilSequenceCompletes(sequence);

  EXPECT_EQ(0, counts.run_called);
  EXPECT_EQ(0, counts.delete_called);
  EXPECT_EQ(1, counts.cancel_called);

  worker_->FreeSequence(sequence);
}

TEST_F(CentralControllerCallbackTest, CancelAfterRunRequeue) {
  struct CallCounts counts(thread_runtime_.get());
  QueuedWorkerPool::Sequence* sequence = worker_->NewSequence();

  // Create another worker pool/sequence that are shutdown. When the callback
  // is enqued onto this sequence, its Cancel should be immediately called.
  scoped_ptr<QueuedWorkerPool> worker2(new QueuedWorkerPool(
      2, "central_controller_test2", thread_runtime_.get()));
  QueuedWorkerPool::Sequence* sequence2 = worker2->NewSequence();
  worker2->ShutDown();

  sequence->Add(new MockCentralControllerCallback(sequence2, &counts));

  counts.sync.Wait();
  // It's especially important with this test to make sure all the cleanup
  // has completed from the first sequence. Otherwise the MockCallbackHandle
  // destructor won't yet have been run.
  WaitUntilSequenceCompletes(sequence);

  EXPECT_EQ(0, counts.run_called);
  EXPECT_EQ(1, counts.delete_called);
  EXPECT_EQ(1, counts.cancel_called);

  worker_->FreeSequence(sequence);
  worker2->FreeSequence(sequence2);
}

TEST_F(CentralControllerCallbackTest, CancelAfterCancelRequeue) {
  struct CallCounts counts(thread_runtime_.get());
  QueuedWorkerPool::Sequence* sequence = worker_->NewSequence();

  // Create another worker pool/sequence that are shutdown. When the callback
  // is enqued onto this sequence, its Cancel should be immediately called.
  scoped_ptr<QueuedWorkerPool> worker2(new QueuedWorkerPool(
      2, "central_controller_test2", thread_runtime_.get()));
  QueuedWorkerPool::Sequence* sequence2 = worker2->NewSequence();
  worker2->ShutDown();

  // Call Cancel() on the callback instead of Run in CancelAfterRunRequeue.
  sequence->Add(
      MakeFunction(static_cast<Function*>(
                       new MockCentralControllerCallback(sequence2, &counts)),
                   &Function::CallCancel));

  counts.sync.Wait();
  // It's especially important with this test to make sure all the cleanup
  // has completed from the first sequence. Otherwise the MockCallbackHandle
  // destructor won't yet have been run.
  WaitUntilSequenceCompletes(sequence);

  EXPECT_EQ(0, counts.run_called);
  EXPECT_EQ(0, counts.delete_called);  // The context shouldn't be created.
  EXPECT_EQ(1, counts.cancel_called);

  worker_->FreeSequence(sequence);
  worker2->FreeSequence(sequence2);
}

TEST_F(CentralControllerCallbackTest, RegularRunWithPointerSteal) {
  struct CallCounts counts(thread_runtime_.get());
  QueuedWorkerPool::Sequence* sequence = worker_->NewSequence();
  MockCentralControllerCallback* callback =
      new MockCentralControllerCallback(sequence, &counts);
  callback->SetStealPointer(true);
  sequence->Add(callback);

  counts.sync.Wait();
  WaitUntilSequenceCompletes(sequence);

  EXPECT_EQ(1, counts.run_called);
  EXPECT_EQ(0, counts.delete_called);
  EXPECT_EQ(0, counts.cancel_called);

  worker_->FreeSequence(sequence);

  counts.handle.reset();
  EXPECT_EQ(1, counts.delete_called);
}

}  // namespace

}  // namespace net_instaweb
