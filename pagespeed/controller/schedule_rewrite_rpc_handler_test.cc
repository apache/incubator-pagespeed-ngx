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

#include "pagespeed/controller/grpc_server_test.h"
#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/controller/controller.pb.h"
#include "pagespeed/controller/schedule_rewrite_rpc_handler.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/util/grpc.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::WithArgs;

namespace net_instaweb {

namespace {

// Free functions to allow use of WithArgs<N>(Invoke(, because gMock doesn't
// understand our Functions.
void RunFunction(Function* f) {
  f->CallRun();
}

void CancelFunction(Function* f) {
  f->CallCancel();
}

class MockScheduleRewriteController : public ScheduleRewriteController {
 public:
  MockScheduleRewriteController() {
    EXPECT_CALL(*this, ScheduleRewrite(_, _)).Times(0);
    EXPECT_CALL(*this, NotifyRewriteComplete(_)).Times(0);
    EXPECT_CALL(*this, NotifyRewriteFailed(_)).Times(0);
  }
  virtual ~MockScheduleRewriteController() { }

  MOCK_METHOD2(ScheduleRewrite, void(const GoogleString& key, Function* cb));
  MOCK_METHOD1(NotifyRewriteComplete, void(const GoogleString& key));
  MOCK_METHOD1(NotifyRewriteFailed, void(const GoogleString& key));

  void SaveFunction(Function* f) { saved_function_ = f; }

  Function* saved_function_;
};

}  // namespace

class ScheduleRewriteRpcHandlerTest : public GrpcServerTest {
 public:
  void SetUp() override {
    GrpcServerTest::SetUp();
    client_.reset(new ClientConnection(ServerAddress()));
  }

  void RegisterServices(::grpc::ServerBuilder* builder) override {
    builder->RegisterService(&service_);
  }

  // gRPC functions can only safely be called from the server thread. Since
  // Start() is one of those, provide a wrapper for that.
  void StartOnServerThread(ScheduleRewriteRpcHandler* handler) {
    QueueFunctionForServerThread(MakeFunction<ScheduleRewriteRpcHandler>(
        handler, &ScheduleRewriteRpcHandler::Start));
  }

  void StartHandler() {
    StartOnServerThread(new ScheduleRewriteRpcHandler(&service_, queue_.get(),
                                                      &mock_controller_));
  }

 protected:
  class ClientConnection : public BaseClientConnection {
   public:
    explicit ClientConnection(const GoogleString& address)
        : BaseClientConnection(address),
          stub_(grpc::CentralControllerRpcService::NewStub(channel_)),
          reader_writer_(stub_->ScheduleRewrite(&client_ctx_)) {
    }

    std::unique_ptr<grpc::CentralControllerRpcService::Stub> stub_;
    std::unique_ptr<::grpc::ClientReaderWriter<ScheduleRewriteRequest,
                                               ScheduleRewriteResponse>>
        reader_writer_;
  };

  void SendStartRewrite(const GoogleString& key) {
    ScheduleRewriteRequest req;
    req.set_key(key);
    ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));
  }

  void SendRewriteStatus(ScheduleRewriteRequest::RewriteStatus status) {
    ScheduleRewriteRequest req;
    req.set_status(status);
    ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));
  }

  void ExpectRewritePermission(bool expected_ok) {
    ScheduleRewriteResponse resp;
    ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
    EXPECT_THAT(resp.ok_to_proceed(), Eq(expected_ok));
  }

  void ExpectFinalStatus(const ::grpc::StatusCode& expected_code) {
    ::grpc::Status status = client_->reader_writer_->Finish();
    EXPECT_THAT(status.error_code(), Eq(expected_code));
  }

  grpc::CentralControllerRpcService::AsyncService service_;
  std::unique_ptr<ClientConnection> client_;
  MockScheduleRewriteController mock_controller_;
};

namespace {

TEST_F(ScheduleRewriteRpcHandlerTest, ImmediateDeny) {
  EXPECT_CALL(mock_controller_, ScheduleRewrite("test", _))
      .WillOnce(WithArgs<1>(Invoke(&CancelFunction)));
  StartHandler();

  SendStartRewrite("test");
  ExpectRewritePermission(false);
  ExpectFinalStatus(::grpc::StatusCode::OK);
}

TEST_F(ScheduleRewriteRpcHandlerTest, AllowRewriteThatSucceeds) {
  EXPECT_CALL(mock_controller_, ScheduleRewrite("anotherkey", _))
      .WillOnce(WithArgs<1>(Invoke(&RunFunction)));
  EXPECT_CALL(mock_controller_, NotifyRewriteComplete("anotherkey")).Times(1);
  StartHandler();

  SendStartRewrite("anotherkey");
  ExpectRewritePermission(true);
  SendRewriteStatus(ScheduleRewriteRequest::SUCCESS);
  ExpectFinalStatus(::grpc::StatusCode::OK);
}

TEST_F(ScheduleRewriteRpcHandlerTest, AllowRewriteThatFails) {
  EXPECT_CALL(mock_controller_, ScheduleRewrite("test", _))
      .WillOnce(WithArgs<1>(Invoke(&RunFunction)));
  EXPECT_CALL(mock_controller_, NotifyRewriteFailed("test")).Times(1);
  StartHandler();

  SendStartRewrite("test");
  ExpectRewritePermission(true);
  SendRewriteStatus(ScheduleRewriteRequest::FAILED);
  ExpectFinalStatus(::grpc::StatusCode::OK);
}

TEST_F(ScheduleRewriteRpcHandlerTest, AllowRewriteCallbackDelayed) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  EXPECT_CALL(mock_controller_, ScheduleRewrite("anotherkey", _))
      .WillOnce(DoAll(
          WithArgs<1>(Invoke(&mock_controller_,
                             &MockScheduleRewriteController::SaveFunction)),
          InvokeWithoutArgs(&sync, &WorkerTestBase::SyncPoint::Notify)));
  EXPECT_CALL(mock_controller_, NotifyRewriteComplete("anotherkey")).Times(1);
  StartHandler();

  SendStartRewrite("anotherkey");

  // Wait for server to finish processing ScheduleRewrite call, then invoke the
  // callback.
  sync.Wait();
  QueueFunctionForServerThread(mock_controller_.saved_function_);

  ExpectRewritePermission(true);
  SendRewriteStatus(ScheduleRewriteRequest::SUCCESS);
  ExpectFinalStatus(::grpc::StatusCode::OK);
}

TEST_F(ScheduleRewriteRpcHandlerTest, ClientDisconnectDuringRewrite) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  EXPECT_CALL(mock_controller_, ScheduleRewrite("broken", _))
      .WillOnce(WithArgs<1>(Invoke(&RunFunction)));
  EXPECT_CALL(mock_controller_, NotifyRewriteFailed("broken"))
      .WillOnce(InvokeWithoutArgs(&sync, &WorkerTestBase::SyncPoint::Notify));
  StartHandler();

  SendStartRewrite("broken");
  ExpectRewritePermission(true);
  client_.reset();

  // Wait for the server to actually run the function before finishing.
  sync.Wait();
}

TEST_F(ScheduleRewriteRpcHandlerTest, ClientDisconnectWhileWaiting) {
  WorkerTestBase::SyncPoint func_saved(thread_system_.get());
  WorkerTestBase::SyncPoint func_run(thread_system_.get());
  {
    // We need to make sure the Failed notification doesn't happen until after
    // the server wakes up.
    ::testing::InSequence s;

    EXPECT_CALL(mock_controller_, ScheduleRewrite("broken", _))
        .WillOnce(DoAll(
            WithArgs<1>(Invoke(&mock_controller_,
                               &MockScheduleRewriteController::SaveFunction)),
            InvokeWithoutArgs(&func_saved,
                              &WorkerTestBase::SyncPoint::Notify)));
    EXPECT_CALL(mock_controller_, NotifyRewriteFailed("broken"))
        .WillOnce(
            InvokeWithoutArgs(&func_run, &WorkerTestBase::SyncPoint::Notify));
  }
  StartHandler();

  SendStartRewrite("broken");

  // Wait for the server to process the request, then drop the client.
  func_saved.Wait();
  client_.reset();

  // Now "wake up" the server. This should call NotifyRewriteFailed.
  QueueFunctionForServerThread(mock_controller_.saved_function_);

  // Wait for the server to actually run the function before finishing.
  func_run.Wait();
}

TEST_F(ScheduleRewriteRpcHandlerTest, ClientDisconnectWhileWaitingForDeny) {
  WorkerTestBase::SyncPoint func_saved(thread_system_.get());
  WorkerTestBase::SyncPoint func_run(thread_system_.get());

  EXPECT_CALL(mock_controller_, ScheduleRewrite("broken", _))
      .WillOnce(DoAll(
          WithArgs<1>(Invoke(&mock_controller_,
                             &MockScheduleRewriteController::SaveFunction)),
          InvokeWithoutArgs(&func_saved, &WorkerTestBase::SyncPoint::Notify)));
  EXPECT_CALL(mock_controller_, NotifyRewriteFailed("broken")).Times(0);
  StartHandler();

  SendStartRewrite("broken");

  // Wait for the server to process the request, then drop the client.
  func_saved.Wait();
  client_.reset();

  // Now "wake up" the server and have it deny the rewrite request. This
  // should call NotifyClient(false) which must not call NotifyRewriteFailed.
  QueueFunctionForServerThread(
      MakeFunction(mock_controller_.saved_function_, &Function::CallCancel));
  // Queue another event to notify once NotifyClient has been invoked.
  QueueFunctionForServerThread(
      MakeFunction(&func_run, &WorkerTestBase::SyncPoint::Notify));

  // Wait for the server to actually run the function before finishing.
  func_run.Wait();
}

TEST_F(ScheduleRewriteRpcHandlerTest, InitNoKey) {
  StartHandler();

  SendStartRewrite("");
  ExpectFinalStatus(::grpc::StatusCode::ABORTED);
}

TEST_F(ScheduleRewriteRpcHandlerTest, InitWithStatus) {
  StartHandler();

  ScheduleRewriteRequest req;
  req.set_status(ScheduleRewriteRequest::SUCCESS);
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  ExpectFinalStatus(::grpc::StatusCode::ABORTED);
}

TEST_F(ScheduleRewriteRpcHandlerTest, StatusWhileWaiting) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  EXPECT_CALL(mock_controller_, ScheduleRewrite("hello", _))
      .WillOnce(WithArgs<1>(Invoke(
          &mock_controller_, &MockScheduleRewriteController::SaveFunction)));
  EXPECT_CALL(mock_controller_, NotifyRewriteFailed("hello"))
      .WillOnce(
          InvokeWithoutArgs(&sync, &WorkerTestBase::SyncPoint::Notify));
  StartHandler();

  SendStartRewrite("hello");
  SendStartRewrite("goodbye");

  ExpectFinalStatus(::grpc::StatusCode::ABORTED);
  QueueFunctionForServerThread(mock_controller_.saved_function_);
  sync.Wait();
}

}  // namespace

}  // namespace net_instaweb
