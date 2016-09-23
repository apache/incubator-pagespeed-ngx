// Copyright 2016 Google Inc.
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
// Author: cheesy@google.com (Steve Hill)

#include <unistd.h>
#include <memory>
#include <vector>

#include "pagespeed/controller/context_registry.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"
#include "pagespeed/kernel/util/platform.h"

using testing::AtMost;
using testing::Eq;
using testing::Gt;
using testing::Invoke;
using testing::Lt;

namespace net_instaweb {

namespace {

// Trivial mock object that can be put into a ContextRegistry.
class MockContext {
 public:
  MockContext() {
    EXPECT_CALL(*this, TryCancel()).Times(0);
  }

  MOCK_METHOD0(TryCancel, void());
};

class ContextRegistryTest : public testing::Test {
 public:
  ContextRegistryTest()
      : thread_system_(Platform::CreateThreadSystem()),
        worker_(1 /* max_workers */, "context_registry_test",
                thread_system_.get()),
        sequence_(worker_.NewSequence()),
        registry_(thread_system_.get()) {}

  ~ContextRegistryTest() override { worker_.FreeSequence(sequence_); }

  // MakeFunction doesn't know how to deal with protected, grumble.
  void RemoveContexts(std::vector<MockContext>* contexts,
                      WorkerTestBase::SyncPoint* sync) {
    sync->Notify();
    for (MockContext& context : *contexts) {
      registry_.RemoveContext(&context);
      usleep(1);  // yield.
    }
  }

 protected:
  typedef ContextRegistry<MockContext> MockRegistry;

  std::unique_ptr<ThreadSystem> thread_system_;
  QueuedWorkerPool worker_;
  QueuedWorkerPool::Sequence* sequence_;

  MockRegistry registry_;
};

TEST_F(ContextRegistryTest, CancelEmptyDoesntBlock) {
  EXPECT_THAT(registry_.Empty(), Eq(true));
  EXPECT_THAT(registry_.IsShutdown(), Eq(false));
  registry_.CancelAllActiveAndWait();
  // We're verifying that Cancel doesn't wait indefinitely when empty. If that
  // happens the test will hang and require ^C or some other timeout.
  EXPECT_THAT(registry_.Empty(), Eq(true));
  EXPECT_THAT(registry_.IsShutdown(), Eq(true));
}

TEST_F(ContextRegistryTest, CantRegisterAfterShutdown) {
  MockContext ctx;
  registry_.CancelAllActive();
  EXPECT_THAT(registry_.TryRegisterContext(&ctx), Eq(false));
  EXPECT_THAT(registry_.Empty(), Eq(true));
}

TEST_F(ContextRegistryTest, DoesntCancelOldEntries) {
  MockContext ctx;
  EXPECT_CALL(ctx, TryCancel()).Times(0);

  EXPECT_THAT(registry_.TryRegisterContext(&ctx), Eq(true));
  EXPECT_THAT(registry_.Empty(), Eq(false));

  registry_.RemoveContext(&ctx);
  EXPECT_THAT(registry_.Empty(), Eq(true));

  // Should do precisely nothing.
  registry_.CancelAllActiveAndWait();
}

TEST_F(ContextRegistryTest, CancelsContainedItems) {
  MockContext ctx;
  // CancelAllActiveAndWait blocks until the registry is empty, so we need to
  // call RemoveContext on another thread. Here we setup "ctx" so that it
  // expects TryCancel to be called once. Once that happens, it will queue a
  // task on sequence_ that removes ctx from the registry.
  EXPECT_CALL(ctx, TryCancel()).WillOnce(Invoke([this, &ctx]() {
    sequence_->Add(
        MakeFunction(&registry_, &MockRegistry::RemoveContext, &ctx));
  }));

  EXPECT_THAT(registry_.TryRegisterContext(&ctx), Eq(true));
  EXPECT_THAT(registry_.Empty(), Eq(false));

  registry_.CancelAllActiveAndWait();  // Should block.
  EXPECT_THAT(registry_.Empty(), Eq(true));
}

TEST_F(ContextRegistryTest, NoWaitDoesnWait) {
  MockContext ctx;
  EXPECT_CALL(ctx, TryCancel());

  EXPECT_THAT(registry_.TryRegisterContext(&ctx), Eq(true));
  EXPECT_THAT(registry_.Empty(), Eq(false));

  registry_.CancelAllActive();  // Must not block, even though not empty.
  EXPECT_THAT(registry_.Empty(), Eq(false));

  registry_.RemoveContext(&ctx);
  EXPECT_THAT(registry_.Empty(), Eq(true));
}

TEST_F(ContextRegistryTest, RemovalsWhileCanceling) {
  // We want to verify that nothing bad happens if a context is removed by
  // another thread while CancelAllActiveAndWait() is running. So, we put a
  // bunch of things in the registry, start a thread that removes them and call
  // CancelAllActiveAndWait() in parallel. It would be nice to force
  // synchronization here, but that's tricky because we can't call back into the
  // registry from TryCancel() because the lock is held.
  static const int kNumContexts = 1000;

  int num_canceled = 0;

  std::vector<MockContext> contexts(kNumContexts);
  for (MockContext& context : contexts) {
    EXPECT_CALL(context, TryCancel())
        .Times(AtMost(1))
        // Calls to TryCancel are in series via this thread, so there's no
        // concurrency issue on num_canceled.
        .WillOnce(Invoke([&num_canceled]() { ++num_canceled; }));
    ASSERT_THAT(registry_.TryRegisterContext(&context), Eq(true));
  }

  // Queue the task that unregisters everything from the registry.
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  sequence_->Add(MakeFunction<ContextRegistryTest>(
      this, &ContextRegistryTest::RemoveContexts, &contexts, &sync));

  // Wait for the task to start, then start notifying everything in the
  // registry.
  sync.Wait();
  registry_.CancelAllActiveAndWait();

  EXPECT_THAT(registry_.Empty(), Eq(true));

  // Verify that some contexts were canceled (> 0) and some were removed
  // (< kNumContexts).
  EXPECT_THAT(num_canceled, Gt(0));
  EXPECT_THAT(num_canceled, Lt(kNumContexts));
}

}  // namespace

}  // namespace net_instaweb
