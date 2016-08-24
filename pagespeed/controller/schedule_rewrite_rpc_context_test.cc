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

#include <memory>

#include "base/logging.h"
#include "pagespeed/controller/controller.pb.h"
#include "pagespeed/controller/controller_grpc_mocks.h"
#include "pagespeed/controller/schedule_rewrite_rpc_context.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/proto_matcher.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/message_handler_test_base.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"
#include "pagespeed/kernel/thread/sequence.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/util/grpc.h"
#include "pagespeed/kernel/util/platform.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::HasSubstr;
using testing::Not;
using testing::WithArgs;
using testing::Return;
using testing::SetArgPointee;

namespace net_instaweb {

namespace {

typedef MockReaderWriterT<ScheduleRewriteRequest,
                          ScheduleRewriteResponse>
    MockReaderWriter;

class MockScheduleRewriteCallback : public ScheduleRewriteCallback {
 public:
  MockScheduleRewriteCallback(const GoogleString& key, Sequence* s)
      : ScheduleRewriteCallback(key, s) {
    EXPECT_CALL(*this, RunImpl(_)).Times(0);
    EXPECT_CALL(*this, CancelImpl()).Times(0);;
  }

  MOCK_METHOD1(RunImpl, void(scoped_ptr<ScheduleRewriteContext>* context));
  MOCK_METHOD0(CancelImpl, void());
};

class ScheduleRewriteRpcContextTest : public testing::Test {
 public:
  ScheduleRewriteRpcContextTest()
      : thread_system_(Platform::CreateThreadSystem()),
        worker_(2 /* max_workers */, "schedule_rewrite_operation_test",
                thread_system_.get()),
        sequence_(worker_.NewSequence()),
        stub_(sequence_) {}

  ~ScheduleRewriteRpcContextTest() {
    worker_.FreeSequence(sequence_);
  }

  void StartRpcContext(MockReaderWriter* rw,
                       MockScheduleRewriteCallback* cb) {
    stub_.ExpectAsyncScheduleRewrite(rw);
    // Now start the operation. This cleans up after itself.
    new ScheduleRewriteRpcContext(&stub_, nullptr /* queue */,
                                  thread_system_.get(), &handler_, cb);
  }

  void ScheduleCallSuccessAndDelete(
      scoped_ptr<ScheduleRewriteContext>* ctx) {
    sequence_->Add(
        MakeFunction(this, &ScheduleRewriteRpcContextTest::CallSuccessAndDelete,
                     ctx->release()));
  }

  void CallSuccessAndDelete(ScheduleRewriteContext* context) {
    context->MarkSucceeded();
    delete context;
  }

  void ScheduleCallFailedAndDelete(
      scoped_ptr<ScheduleRewriteContext>* ctx) {
    sequence_->Add(
        MakeFunction(this, &ScheduleRewriteRpcContextTest::CallFailedAndDelete,
                     ctx->release()));
  }

  void CallFailedAndDelete(ScheduleRewriteContext* context) {
    context->MarkFailed();
    delete context;
  }

  void ExpectFinishWithDebugHack(MockReaderWriter* rw, ::grpc::Status status,
                                 WorkerTestBase::SyncPoint* sync) {
#ifndef NDEBUG
    if (!status.ok()) {
      // This is a pretty nasty hack. The code calls LOG(DFATAL) when the
      // error_code != OK. Unfortunately, I can't make either EXPECT_DEATH
      // or EXPECT_DFATAL work properly because threads. So, we hack the tests
      // in debug builds to avoid the DFATAL path.
      LOG(WARNING) << "Squashing gRPC error status " << status.error_code()
                   << " to OK. Consider re-running this test under opt.";
      status = ::grpc::Status(::grpc::StatusCode::OK, status.error_message());
    }
#endif
    if (sync != nullptr) {
      rw->ExpectFinishAndNotify(status, sync);
    } else {
      rw->ExpectFinish(status);
    }
  }

 protected:
  std::unique_ptr<ThreadSystem> thread_system_;
  QueuedWorkerPool worker_;
  QueuedWorkerPool::Sequence* sequence_;
  MockCentralControllerRpcServiceStub stub_;
  TestMessageHandler handler_;
};

TEST_F(ScheduleRewriteRpcContextTest, SuccessfulRequest) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockScheduleRewriteCallback* cb =
      new MockScheduleRewriteCallback("a", sequence_);
  MockReaderWriter* rw = new MockReaderWriter(sequence_);
  {
    ::testing::InSequence s;

    // First, the context writes the initial request which contains the key it
    // wants to rewrite.
    rw->ExpectWrite(EqualsProto("key: \"a\""));

    // Next, the context attempts to read a response back from the server. Here
    // we tell it that it's OK to continue.
    rw->ExpectRead("ok_to_proceed: true");

    // Context was told it was OK to run, so it calls Run on the callback.
    EXPECT_CALL(*cb, RunImpl(_)).Times(1);

    // When the callback completes, write a "Did it!" message back to the
    // server.
    rw->ExpectWrite(EqualsProto("status: SUCCESS"));

    // And now call Finish and wait for the server to tell us it's done.
    rw->ExpectFinishAndNotify(::grpc::Status(), &sync);
  }

  StartRpcContext(rw, cb);
  sync.Wait();
}

TEST_F(ScheduleRewriteRpcContextTest, SuccessfulRequestWithExplicitSuccess) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockScheduleRewriteCallback* cb =
      new MockScheduleRewriteCallback("b", sequence_);
  MockReaderWriter* rw = new MockReaderWriter(sequence_);
  {
    ::testing::InSequence s;

    // First, the context writes the initial request which contains the key it
    // wants to rewrite.
    rw->ExpectWrite(EqualsProto("key: \"b\""));

    // Next, the context attempts to read a response back from the server. Here
    // we tell it that it's OK to continue.
    rw->ExpectRead("ok_to_proceed: true");

    // Context was told it was OK to run, so it calls Run on the callback.
    // We detach the context supplied to Run and schedule it to be marked
    // Done() via a subsequent callback.
    EXPECT_CALL(*cb, RunImpl(_))
        .WillOnce(Invoke(
            this,
            &ScheduleRewriteRpcContextTest::ScheduleCallSuccessAndDelete));

    // When the callback completes, write a "Did it!" message back to the
    // server.
    rw->ExpectWrite(EqualsProto("status: SUCCESS"));

    // And now call Finish and wait for the server to tell us it's done.
    rw->ExpectFinishAndNotify(::grpc::Status(), &sync);
  }

  StartRpcContext(rw, cb);
  sync.Wait();
}

TEST_F(ScheduleRewriteRpcContextTest, SuccessfulRequestWithExplicitFailure) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockScheduleRewriteCallback* cb =
      new MockScheduleRewriteCallback("b", sequence_);
  MockReaderWriter* rw = new MockReaderWriter(sequence_);
  {
    ::testing::InSequence s;

    // First, the context writes the initial request which contains the key it
    // wants to rewrite.
    rw->ExpectWrite(EqualsProto("key: \"b\""));

    // Next, the context attempts to read a response back from the server. Here
    // we tell it that it's OK to continue.
    rw->ExpectRead("ok_to_proceed: true");

    // Context was told it was OK to run, so it calls Run on the callback.
    // We detach the context supplied to Run and schedule it to be marked
    // Done() via a subsequent callback.
    EXPECT_CALL(*cb, RunImpl(_))
        .WillOnce(Invoke(
            this, &ScheduleRewriteRpcContextTest::ScheduleCallFailedAndDelete));

    // When the callback completes, inform the controller that the rewrite
    // failed.
    rw->ExpectWrite(EqualsProto("status: FAILED"));

    // And now call Finish and wait for the server to tell us it's done.
    rw->ExpectFinishAndNotify(::grpc::Status(), &sync);
  }

  StartRpcContext(rw, cb);
  sync.Wait();
}

TEST_F(ScheduleRewriteRpcContextTest, UnsuccessfulRequest) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockScheduleRewriteCallback* cb =
      new MockScheduleRewriteCallback("hello", sequence_);
  MockReaderWriter* rw = new MockReaderWriter(sequence_);
  {
    ::testing::InSequence s;

    // First, the context writes the initial request which contains the key it
    // wants to rewrite.
    rw->ExpectWrite(EqualsProto("key: \"hello\""));

    // Next, the context attempts to read a response back from the server. Here
    // we tell it that it's not OK to continue.
    rw->ExpectRead("ok_to_proceed: false");

    // Context was told it was not OK to run, so it calls Cancel on the
    // callback.
    EXPECT_CALL(*cb, CancelImpl())
        .WillOnce(Invoke(&sync, &WorkerTestBase::SyncPoint::Notify));

    // If the server returns not OK, we don't actually call Finish, just close
    // the connection. So no EXPECT_CALL for Finish here.
  }

  StartRpcContext(rw, cb);
  sync.Wait();
}

TEST_F(ScheduleRewriteRpcContextTest, InitFailed) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockScheduleRewriteCallback* cb =
      new MockScheduleRewriteCallback("key", sequence_);
  MockReaderWriter* rw = new MockReaderWriter(sequence_);

  // Configure the stub to return rw in response to the RpcContext
  // initiating a request, but then indicate that the gRPC init failed.
  stub_.ExpectAsyncScheduleRewriteFailure(rw);

  ExpectFinishWithDebugHack(
      rw, ::grpc::Status(::grpc::StatusCode::ABORTED, "hangup"),
      nullptr /* notify */);

  // Controller is AFK so we expect Cancel on the callback.
  EXPECT_CALL(*cb, CancelImpl())
      .WillOnce(Invoke(&sync, &WorkerTestBase::SyncPoint::Notify));

  // Start the operation. This cleans up after itself.
  new ScheduleRewriteRpcContext(&stub_, nullptr /* queue */,
                                   thread_system_.get(), &handler_, cb);

  sync.Wait();

  ASSERT_THAT(handler_.messages(), Not(IsEmpty()));
#ifdef NDEBUG
  EXPECT_THAT(handler_.messages().back(), HasSubstr("hangup"));
#endif
}

TEST_F(ScheduleRewriteRpcContextTest, FinishFailed) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockScheduleRewriteCallback* cb =
      new MockScheduleRewriteCallback("key", sequence_);
  MockReaderWriter* rw = new MockReaderWriter(sequence_);

  // Configure the stub to return rw in response to the RpcContext
  // initiating a request, but then indicate that the gRPC init failed.
  stub_.ExpectAsyncScheduleRewriteFailure(rw);

  // What we're actually testing; Make Finish() itself fail.
  // Not clear how that would actually happen in practice.
  rw->ExpectFinishFailure();

  // Controller is AFK so we expect Cancel on the callback.
  EXPECT_CALL(*cb, CancelImpl())
      .WillOnce(Invoke(&sync, &WorkerTestBase::SyncPoint::Notify));

  // Start the operation. This cleans up after itself.
  new ScheduleRewriteRpcContext(&stub_, nullptr /* queue */,
                                   thread_system_.get(), &handler_, cb);

  sync.Wait();

  ASSERT_THAT(handler_.messages(), Not(IsEmpty()));
  EXPECT_THAT(handler_.messages().back(), HasSubstr("Finish failed"));
}


TEST_F(ScheduleRewriteRpcContextTest, FirstWriteFailed) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockScheduleRewriteCallback* cb =
      new MockScheduleRewriteCallback("key", sequence_);
  MockReaderWriter* rw = new MockReaderWriter(sequence_);
  {
    ::testing::InSequence s;

    // Pretend that the initial Write failed.
    rw->ExpectWriteFailure(EqualsProto("key: \"key\""));

    ExpectFinishWithDebugHack(
        rw, ::grpc::Status(::grpc::StatusCode::ABORTED, "hangup"),
        nullptr /* notify */);

    // Controller is AFK so we expect Cancel on the callback.
    EXPECT_CALL(*cb, CancelImpl())
        .WillOnce(Invoke(&sync, &WorkerTestBase::SyncPoint::Notify));
  }

  StartRpcContext(rw, cb);
  sync.Wait();

  ASSERT_THAT(handler_.messages(), Not(IsEmpty()));
#ifdef NDEBUG
  EXPECT_THAT(handler_.messages().back(), HasSubstr("hangup"));
#endif
}

TEST_F(ScheduleRewriteRpcContextTest, ReadFailed) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockScheduleRewriteCallback* cb =
      new MockScheduleRewriteCallback("a", sequence_);
  MockReaderWriter* rw = new MockReaderWriter(sequence_);
  {
    ::testing::InSequence s;

    // First, the context writes the initial request which contains the key it
    // wants to rewrite.
    rw->ExpectWrite(EqualsProto("key: \"a\""));

    // Now pretend that the gRPC call to Read failed.
    rw->ExpectReadFailure();

    ExpectFinishWithDebugHack(
        rw, ::grpc::Status(::grpc::StatusCode::ABORTED, "hangup"),
        nullptr /* notify */);

    // Controller is AFK so we expect Cancel on the callback.
    EXPECT_CALL(*cb, CancelImpl())
        .WillOnce(Invoke(&sync, &WorkerTestBase::SyncPoint::Notify));
  }

  StartRpcContext(rw, cb);
  sync.Wait();

  ASSERT_THAT(handler_.messages(), Not(IsEmpty()));
#ifdef NDEBUG
  EXPECT_THAT(handler_.messages().back(), HasSubstr("hangup"));
#endif
}

TEST_F(ScheduleRewriteRpcContextTest, SecondWriteFailed) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockScheduleRewriteCallback* cb =
      new MockScheduleRewriteCallback("a", sequence_);
  MockReaderWriter* rw = new MockReaderWriter(sequence_);
  {
    ::testing::InSequence s;

    // First, the context writes the initial request which contains the key it
    // wants to rewrite.
    rw->ExpectWrite(EqualsProto("key: \"a\""));

    // Next, the context attempts to read a response back from the server. Here
    // we tell it that it's OK to continue.
    rw->ExpectRead("ok_to_proceed: true");

    // Context was told it was OK to run, so it calls Run on the callback.
    EXPECT_CALL(*cb, RunImpl(_)).Times(1);

    // When the callback completes, try to write the "Did it!" message back to
    // the server, but pretend it failed.
    rw->ExpectWriteFailure(EqualsProto("status: SUCCESS"));

    ExpectFinishWithDebugHack(
        rw, ::grpc::Status(::grpc::StatusCode::ABORTED, "hangup"), &sync);
  }

  StartRpcContext(rw, cb);
  sync.Wait();

  ASSERT_THAT(handler_.messages(), Not(IsEmpty()));
#ifdef NDEBUG
  EXPECT_THAT(handler_.messages().back(), HasSubstr("hangup"));
#endif
}

}  // namespace

}  // namespace net_instaweb
