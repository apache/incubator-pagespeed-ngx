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
#include "pagespeed/controller/expensive_operation_rpc_handler.h"
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

class MockExpensiveOperationController : public ExpensiveOperationController {
 public:
  MockExpensiveOperationController() {
    EXPECT_CALL(*this, ScheduleExpensiveOperation(_)).Times(0);
    EXPECT_CALL(*this, NotifyExpensiveOperationComplete()).Times(0);
  }
  virtual ~MockExpensiveOperationController() { }

  MOCK_METHOD1(ScheduleExpensiveOperation, void(Function* cb));
  MOCK_METHOD0(NotifyExpensiveOperationComplete, void());

  void SaveFunction(Function* f) { saved_function_ = f; }

  Function* saved_function_;
};

}  // namespace

class ExpensiveOperationRpcHandlerTest : public GrpcServerTest {
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
  void StartOnServerThread(ExpensiveOperationRpcHandler* handler) {
    QueueFunctionForServerThread(MakeFunction<ExpensiveOperationRpcHandler>(
        handler, &ExpensiveOperationRpcHandler::Start));
  }

  void StartHandler() {
    StartOnServerThread(new ExpensiveOperationRpcHandler(
        &service_, queue_.get(), &mock_controller_));
  }

 protected:
  class ClientConnection : public BaseClientConnection {
   public:
    explicit ClientConnection(const GoogleString& address)
        : BaseClientConnection(address),
          stub_(grpc::CentralControllerRpcService::NewStub(channel_)),
          reader_writer_(stub_->ScheduleExpensiveOperation(&client_ctx_)) {
    }

    std::unique_ptr<grpc::CentralControllerRpcService::Stub> stub_;
    std::unique_ptr<::grpc::ClientReaderWriter<
        ScheduleExpensiveOperationRequest, ScheduleExpensiveOperationResponse>>
        reader_writer_;
  };

  void SendScheduleRequest() {
    ScheduleExpensiveOperationRequest req;
    ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));
  }

  void ExpectProceedPermission(bool expected_ok) {
    ScheduleExpensiveOperationResponse resp;
    ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
    EXPECT_THAT(resp.ok_to_proceed(), Eq(expected_ok));
  }

  void ExpectFinalStatus(const ::grpc::StatusCode& expected_code) {
    ::grpc::Status status = client_->reader_writer_->Finish();
    EXPECT_THAT(status.error_code(), Eq(expected_code));
  }

  grpc::CentralControllerRpcService::AsyncService service_;
  std::unique_ptr<ClientConnection> client_;
  MockExpensiveOperationController mock_controller_;
};

namespace {

TEST_F(ExpensiveOperationRpcHandlerTest, ImmediateDeny) {
  EXPECT_CALL(mock_controller_, ScheduleExpensiveOperation(_))
      .WillOnce(WithArgs<0>(Invoke(&CancelFunction)));
  StartHandler();

  SendScheduleRequest();
  ExpectProceedPermission(false);
  ExpectFinalStatus(::grpc::StatusCode::OK);
}

TEST_F(ExpensiveOperationRpcHandlerTest, ImmediateAllow) {
  EXPECT_CALL(mock_controller_, ScheduleExpensiveOperation(_))
      .WillOnce(WithArgs<0>(Invoke(&RunFunction)));
  EXPECT_CALL(mock_controller_, NotifyExpensiveOperationComplete())
      .Times(1);
  StartHandler();

  SendScheduleRequest();
  ExpectProceedPermission(true);
  SendScheduleRequest();
  ExpectFinalStatus(::grpc::StatusCode::OK);
}

TEST_F(ExpensiveOperationRpcHandlerTest, AllowRewriteCallbackDelayed) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  EXPECT_CALL(mock_controller_, ScheduleExpensiveOperation(_))
      .WillOnce(DoAll(
          WithArgs<0>(Invoke(&mock_controller_,
                             &MockExpensiveOperationController::SaveFunction)),
          InvokeWithoutArgs(&sync, &WorkerTestBase::SyncPoint::Notify)));
  EXPECT_CALL(mock_controller_, NotifyExpensiveOperationComplete()).Times(1);
  StartHandler();

  SendScheduleRequest();

  // Wait for server to finish processing ScheduleExpensiveOperation call, then
  // invoke the callback.
  sync.Wait();
  QueueFunctionForServerThread(mock_controller_.saved_function_);

  ExpectProceedPermission(true);
  SendScheduleRequest();
  ExpectFinalStatus(::grpc::StatusCode::OK);
}

TEST_F(ExpensiveOperationRpcHandlerTest,
       ClientDisconnectDuringOperation) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  EXPECT_CALL(mock_controller_, ScheduleExpensiveOperation(_))
      .WillOnce(WithArgs<0>(Invoke(&RunFunction)));
  EXPECT_CALL(mock_controller_, NotifyExpensiveOperationComplete())
      .WillOnce(InvokeWithoutArgs(&sync, &WorkerTestBase::SyncPoint::Notify));
  StartHandler();

  SendScheduleRequest();
  ExpectProceedPermission(true);
  client_.reset();

  // Wait for the server to actually run the function before finishing.
  sync.Wait();
}

TEST_F(ExpensiveOperationRpcHandlerTest, ClientDisconnectWhileWaiting) {
  WorkerTestBase::SyncPoint func_saved(thread_system_.get());
  WorkerTestBase::SyncPoint func_run(thread_system_.get());
  {
    // We need to make sure the Complete notification doesn't happen until after
    // the server wakes up.
    ::testing::InSequence s;

    EXPECT_CALL(mock_controller_, ScheduleExpensiveOperation(_))
        .WillOnce(DoAll(WithArgs<0>(Invoke(
                            &mock_controller_,
                            &MockExpensiveOperationController::SaveFunction)),
                        InvokeWithoutArgs(&func_saved,
                                          &WorkerTestBase::SyncPoint::Notify)));
    EXPECT_CALL(mock_controller_, NotifyExpensiveOperationComplete())
        .WillOnce(
            InvokeWithoutArgs(&func_run, &WorkerTestBase::SyncPoint::Notify));
  }
  StartHandler();

  SendScheduleRequest();

  // Wait for the server to process the request, then drop the client.
  func_saved.Wait();
  client_.reset();

  // Now "wake up" the server. This should call
  // NotifyExpensiveOperationComplete.
  QueueFunctionForServerThread(mock_controller_.saved_function_);

  // Wait for the server to actually run the function before finishing.
  func_run.Wait();
}

TEST_F(ExpensiveOperationRpcHandlerTest, ClientDisconnectWhileWaitingForDeny) {
  WorkerTestBase::SyncPoint func_saved(thread_system_.get());
  WorkerTestBase::SyncPoint func_run(thread_system_.get());

  EXPECT_CALL(mock_controller_, ScheduleExpensiveOperation(_))
      .WillOnce(DoAll(
          WithArgs<0>(Invoke(&mock_controller_,
                             &MockExpensiveOperationController::SaveFunction)),
          InvokeWithoutArgs(&func_saved, &WorkerTestBase::SyncPoint::Notify)));
  EXPECT_CALL(mock_controller_, NotifyExpensiveOperationComplete()).Times(0);
  StartHandler();

  SendScheduleRequest();

  // Wait for the server to process the request, then drop the client.
  func_saved.Wait();
  client_.reset();

  // Now "wake up" the server and have it deny the operation. This should call
  // NotifyClient(false) which must not call NotifyExpensiveOperationComplete.
  QueueFunctionForServerThread(
      MakeFunction(mock_controller_.saved_function_, &Function::CallCancel));
  // Queue another event to notify once NotifyClient has been invoked.
  QueueFunctionForServerThread(
      MakeFunction(&func_run, &WorkerTestBase::SyncPoint::Notify));

  // Wait for the server to actually run the function before finishing.
  func_run.Wait();
}

TEST_F(ExpensiveOperationRpcHandlerTest, RequestWhileWaiting) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  EXPECT_CALL(mock_controller_, ScheduleExpensiveOperation(_))
      .WillOnce(WithArgs<0>(Invoke(
          &mock_controller_, &MockExpensiveOperationController::SaveFunction)));
  EXPECT_CALL(mock_controller_, NotifyExpensiveOperationComplete())
      .WillOnce(
          InvokeWithoutArgs(&sync, &WorkerTestBase::SyncPoint::Notify));
  StartHandler();

  SendScheduleRequest();
  SendScheduleRequest();

  ExpectFinalStatus(::grpc::StatusCode::ABORTED);
  QueueFunctionForServerThread(mock_controller_.saved_function_);
  sync.Wait();
}

}  // namespace

}  // namespace net_instaweb
