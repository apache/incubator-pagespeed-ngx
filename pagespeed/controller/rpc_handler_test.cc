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
#include "pagespeed/controller/grpc_test.grpc.pb.h"
#include "pagespeed/controller/grpc_test.pb.h"
#include "pagespeed/controller/rpc_handler.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/proto_matcher.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/util/grpc.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace net_instaweb {

namespace {

class MockRpcHandler : public RpcHandler<grpc::GrpcTestService::AsyncService,
                                         TestRequest, TestResponse> {
 public:
  MockRpcHandler(grpc::GrpcTestService::AsyncService* service,
                 ::grpc::ServerCompletionQueue* cq)
      : RpcHandler<grpc::GrpcTestService::AsyncService, TestRequest,
                   TestResponse>(service, cq) {
    // Default action for HandleRequest is to close the connection with a
    // successful status and not send a reply.
    ON_CALL(*this, HandleRequest(_))
        .WillByDefault(InvokeWithoutArgs(this, &MockRpcHandler::SendFinish));
    ON_CALL(*this, HandleWriteDone())
        .WillByDefault(Return());
    EXPECT_CALL(*this, HandleError()).Times(0);
  }
  virtual ~MockRpcHandler() { }

  MOCK_METHOD1(HandleRequest, void(const TestRequest& req));
  MOCK_METHOD0(HandleWriteDone, void());
  MOCK_METHOD0(HandleError, void());

  void SendResponse() {
    ASSERT_THAT(Write(response_), Eq(true));
  }

  void SendResponseAndIncrement() {
    ASSERT_THAT(Write(response_), Eq(true));
    response_.set_id(response_.id() + 1);
  }

  void SendResponseAndExpectFailure() {
    ASSERT_THAT(Write(response_), Eq(false));
  }

  void SendFinish() {
    ASSERT_THAT(Finish(status_), Eq(true));
  }

  void SendFinishAndExpectFailure() {
    ASSERT_THAT(Finish(status_), Eq(false));
  }

  void SetResponse(const GoogleString& ascii_proto) {
    ASSERT_THAT(ParseTextFormatProtoFromString(ascii_proto, &response_),
                Eq(true));
  }

  void SetFinishStatus(const ::grpc::Status& status) { status_ = status; }

  // This would normally be hidden inside a factory function, but we need to
  // manipulate the mock before calling Start(). Don't call this direcly, use
  // StartOnServerThread instead.
  using RpcHandler::Start;

  void InitResponder(grpc::GrpcTestService::AsyncService* service,
                     ::grpc::ServerContext* ctx, ReaderWriterT* responder,
                     ::grpc::ServerCompletionQueue* cq, void* callback) {
    service->RequestTest(ctx, responder, cq, cq, callback);
  }

  MockRpcHandler* CreateHandler(grpc::GrpcTestService::AsyncService* service,
                                ::grpc::ServerCompletionQueue* cq) override {
    return new MockRpcHandler(service, cq);
  }

 private:
  TestResponse response_;
  ::grpc::Status status_;
};

class RpcHandlerTest : public GrpcServerTest {
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
  void StartOnServerThread(MockRpcHandler* handler) {
    QueueFunctionForServerThread(
        MakeFunction<MockRpcHandler>(handler, &MockRpcHandler::Start));
  }

 protected:
  class ClientConnection : public BaseClientConnection {
   public:
    explicit ClientConnection(const GoogleString& address)
        : BaseClientConnection(address),
          stub_(grpc::GrpcTestService::NewStub(channel_)),
          reader_writer_(stub_->Test(&client_ctx_)) {
    }

    std::unique_ptr<grpc::GrpcTestService::Stub> stub_;
    std::unique_ptr<::grpc::ClientReaderWriter<TestRequest, TestResponse>>
        reader_writer_;
  };

  grpc::GrpcTestService::AsyncService service_;
  std::unique_ptr<ClientConnection> client_;
};

// Test HandleRequest correctly Finish()es when a message is not returned.
TEST_F(RpcHandlerTest, ReadOneWriteStatusOnly) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  EXPECT_CALL(*handler, HandleRequest(EqualsProto("id: 1")))
      .WillOnce(InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish));
  StartOnServerThread(handler);

  TestRequest req;
  req.set_id(1);
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}

// Test HandleRequest correctly Write()s to the client when not returning a
// status.
TEST_F(RpcHandlerTest, WriteWithoutStatus) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  handler->SetResponse("id: 2");
  EXPECT_CALL(*handler, HandleRequest(EqualsProto("id: 1")))
      .WillOnce(InvokeWithoutArgs(handler, &MockRpcHandler::SendResponse))
      .WillOnce(InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish));
  StartOnServerThread(handler);

  TestRequest req;
  req.set_id(1);
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  TestResponse resp;
  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 2"));

  // Write one and read back a status, just to finish up cleanly.
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}

// Test HandleRequest correctly Write()s and Finish()es if both a message
// and a status are requested.
TEST_F(RpcHandlerTest, ReadOneWriteResultAndStatus) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  handler->SetResponse("id: 2");
  EXPECT_CALL(*handler, HandleRequest(EqualsProto("id: 1")))
      .WillOnce(DoAll(InvokeWithoutArgs(handler, &MockRpcHandler::SendResponse),
                      InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish)));
  StartOnServerThread(handler);

  TestRequest req;
  req.set_id(1);
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  TestResponse resp;
  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 2"));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}

// Write() fails for two outstanding messages.
TEST_F(RpcHandlerTest, WriteTwoResultsFail) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  handler->SetResponse("id: 2");
  EXPECT_CALL(*handler, HandleRequest(EqualsProto("id: 1")))
      .WillOnce(DoAll(
          // It is not possible for the write to complete between these calls;
          // That would require the server's event loop handle the Write event,
          // but it's busy dealing with HandleRequest.
          InvokeWithoutArgs(handler, &MockRpcHandler::SendResponseAndIncrement),
          InvokeWithoutArgs(handler,
                            &MockRpcHandler::SendResponseAndExpectFailure),
          InvokeWithoutArgs(&sync, &WorkerTestBase::SyncPoint::Notify),
          InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish)));
  StartOnServerThread(handler);

  TestRequest req;
  req.set_id(1);
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  sync.Wait();

  TestResponse resp;
  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 2"));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}

// We can Write() two messages, so long as we wait for WriteDone.
TEST_F(RpcHandlerTest, CallWriteFromWriteDone) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  handler->SetResponse("id: 2");
  EXPECT_CALL(*handler, HandleRequest(_))
      .WillOnce(InvokeWithoutArgs(handler,
                                  &MockRpcHandler::SendResponseAndIncrement))
      .WillOnce(InvokeWithoutArgs(handler,
                                  &MockRpcHandler::SendFinish));
  EXPECT_CALL(*handler, HandleWriteDone())
      .WillOnce(InvokeWithoutArgs(handler,
                                  &MockRpcHandler::SendResponseAndIncrement))
      .WillOnce(Return());
  StartOnServerThread(handler);

  TestRequest req;
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  TestResponse resp;
  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 2"));

  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 3"));

  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}

// It's OK to call Finish from the WriteDone handler.
TEST_F(RpcHandlerTest, CallFinishFromWriteDone) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  handler->SetResponse("id: 2");
  EXPECT_CALL(*handler, HandleRequest(_))
      .WillOnce(InvokeWithoutArgs(handler,
                                  &MockRpcHandler::SendResponseAndIncrement));
  EXPECT_CALL(*handler, HandleWriteDone())
      .WillOnce(InvokeWithoutArgs(handler,
                                  &MockRpcHandler::SendFinish));
  StartOnServerThread(handler);

  TestRequest req;
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  TestResponse resp;
  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 2"));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}

// It's OK to call Write and Finish from the WriteDone handler.
TEST_F(RpcHandlerTest, CallWriteAndFinishFromWriteDone) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  handler->SetResponse("id: 2");
  EXPECT_CALL(*handler, HandleRequest(_))
      .WillOnce(InvokeWithoutArgs(handler,
                                  &MockRpcHandler::SendResponseAndIncrement));
  EXPECT_CALL(*handler, HandleWriteDone())
      .WillOnce(DoAll(
          InvokeWithoutArgs(handler, &MockRpcHandler::SendResponseAndIncrement),
          InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish)))
      .WillOnce(Return());
  StartOnServerThread(handler);

  TestRequest req;
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  TestResponse resp;
  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 2"));

  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 3"));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}


// Write() works when called outside of an event handler.
TEST_F(RpcHandlerTest, WriteResultOutsideHandleRequest) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  handler->SetResponse("id: 2");
  EXPECT_CALL(*handler, HandleRequest(EqualsProto("id: 1")))
      .WillOnce(
          InvokeWithoutArgs(handler, &MockRpcHandler::SendResponseAndIncrement))
      .WillOnce(InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish));
  EXPECT_CALL(*handler, HandleWriteDone())
      .WillOnce(InvokeWithoutArgs(&sync, &WorkerTestBase::SyncPoint::Notify))
      .WillOnce(Return());
  StartOnServerThread(handler);

  TestRequest req;
  req.set_id(1);
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  TestResponse resp;
  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 2"));

  // Wait for WriteDone and then schedule another Write.
  sync.Wait();
  QueueFunctionForServerThread(
      MakeFunction(handler, &MockRpcHandler::SendResponseAndIncrement));

  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 3"));

  // Write again, server should now finish.
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}

// Test that HandleError is invoked if the client disconnects after the
// handler Write()s.
TEST_F(RpcHandlerTest, ClientAbortAfterWrite) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  EXPECT_CALL(*handler, HandleRequest(_))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(handler, &MockRpcHandler::SendResponse));
  EXPECT_CALL(*handler, HandleError()).Times(1)
      .WillOnce(InvokeWithoutArgs(&sync, &WorkerTestBase::SyncPoint::Notify));
  StartOnServerThread(handler);

  TestRequest req;
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));
  TestResponse resp;
  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));

  // Disconnects from server.
  client_.reset();

  // Make sure the server sees the error and finishes up before we destory it.
  sync.Wait();
}

// Test that HandleError is not invoked if the client disconnects after the
// handler has Finish()ed.
TEST_F(RpcHandlerTest, ClientAbortAfterFinish) {
  WorkerTestBase::SyncPoint sync(thread_system_.get());
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  EXPECT_CALL(*handler, HandleRequest(_))
      .Times(1)
      .WillOnce(
          DoAll(InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish),
                InvokeWithoutArgs(&sync, &WorkerTestBase::SyncPoint::Notify)));
  // We do not override the default implementation of HandleError; An
  // abort would be fine.
  EXPECT_CALL(*handler, HandleError()).Times(0);
  StartOnServerThread(handler);

  TestRequest req;
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  // I'd like to use WritesDone(), but that only gurantees that the messages
  // were flushed to the server, not that the server actually processed them.
  // Hence the barrier.
  sync.Wait();

  // Disconnects from server.
  client_.reset();
}

// Test that gRPC statuses other than the default are correctly propagated
// to the client.
TEST_F(RpcHandlerTest, NonDefaultStatus) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  handler->SetFinishStatus(
      ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "bad"));
  EXPECT_CALL(*handler, HandleRequest(_))
      .WillOnce(InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish));
  StartOnServerThread(handler);

  TestRequest req;
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.error_code(), Eq(::grpc::StatusCode::FAILED_PRECONDITION));
  EXPECT_THAT(status.error_message(), Eq("bad"));
}

// Calling Finish works outside of HandleRequest.
TEST_F(RpcHandlerTest, FinishOutsideHandleRequest) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  handler->SetFinishStatus(
      ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "awful"));
  handler->SetResponse("id: 2");
  EXPECT_CALL(*handler, HandleRequest(_))
      .WillOnce(InvokeWithoutArgs(handler, &MockRpcHandler::SendResponse));
  StartOnServerThread(handler);

  TestRequest req;
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  // Do a blocking read to ensure the handler is done.
  TestResponse resp;
  ASSERT_THAT(client_->reader_writer_->Read(&resp), Eq(true));
  EXPECT_THAT(resp, EqualsProto("id: 2"));

  // This won't run immediately, but Finish is blocking so that's fine.
  QueueFunctionForServerThread(
      MakeFunction(handler, &MockRpcHandler::SendFinish));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.error_code(), Eq(::grpc::StatusCode::FAILED_PRECONDITION));
  EXPECT_THAT(status.error_message(), Eq("awful"));
}

// Test Write fails if called after Finish.
TEST_F(RpcHandlerTest, WriteAfterFinish) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  EXPECT_CALL(*handler, HandleRequest(_))
      .WillOnce(
          DoAll(InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish),
                InvokeWithoutArgs(
                    handler, &MockRpcHandler::SendResponseAndExpectFailure)));
  StartOnServerThread(handler);

  TestRequest req;
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}

// Test Finish fails if called a second time.
TEST_F(RpcHandlerTest, FinishAfterFinish) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  handler->SetResponse("id: 2");
  EXPECT_CALL(*handler, HandleRequest(_))
      .WillOnce(
          DoAll(InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish),
                InvokeWithoutArgs(
                    handler, &MockRpcHandler::SendFinishAndExpectFailure)));
  StartOnServerThread(handler);

  TestRequest req;
  ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

  ::grpc::Status status = client_->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}

// Check we can call Write on a handler refcount after it has finished.
TEST_F(RpcHandlerTest, WriteAfterCompletion) {
  RefCountedPtr<MockRpcHandler> ref;
  {
    MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
    ref.reset(handler);

    // Contrive handler (and ref) to be in FINISHED state.
    EXPECT_CALL(*handler, HandleRequest(_))
        .WillOnce(InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish));
    StartOnServerThread(handler);

    TestRequest req;
    ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

    ::grpc::Status status = client_->reader_writer_->Finish();
    EXPECT_THAT(status.ok(), Eq(true));

    StopServer();  // Blocks until server stop.
  }

  ASSERT_THAT(ref->HasOneRef(), Eq(true));
  ref->SendResponseAndExpectFailure();
}

// Check we can call Finish on a handler refcount after it has finished.
TEST_F(RpcHandlerTest, FinishAfterCompletion) {
  RefCountedPtr<MockRpcHandler> ref;
  {
    MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
    ref.reset(handler);

    // Contrive handler (and ref) to be in FINISHED state.
    EXPECT_CALL(*handler, HandleRequest(_))
        .WillOnce(InvokeWithoutArgs(handler, &MockRpcHandler::SendFinish));
    StartOnServerThread(handler);

    TestRequest req;
    ASSERT_THAT(client_->reader_writer_->Write(req), Eq(true));

    ::grpc::Status status = client_->reader_writer_->Finish();
    EXPECT_THAT(status.ok(), Eq(true));

    StopServer();  // Blocks until server stop.
  }

  ASSERT_THAT(ref->HasOneRef(), Eq(true));
  ref->SendFinishAndExpectFailure();
}

// Check nothing bad happens (leaks or stalls) if Finish is called before the
// first call to HandleRequest.
TEST_F(RpcHandlerTest, FinishBeforeStart) {
  MockRpcHandler* handler = new MockRpcHandler(&service_, queue_.get());
  EXPECT_CALL(*handler, HandleRequest(_)).Times(0);
  handler->SetFinishStatus(
      ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "terrible"));
  // We queue the call to SendFinish on the server thread in case it actually
  // tickles the event loop. It shouldn't, but this is a test after all.
  // SendFinish will verify that Finish returned true.
  QueueFunctionForServerThread(
      MakeFunction(handler, &MockRpcHandler::SendFinish));
  StartOnServerThread(handler);

  TestRequest req;
  req.set_id(7);
  // Surprisingly, Write seems to return true here, even if the RpcHandler has
  // explicitly called responder_->Finish() and then been deleted.
  // I don't actually care about the return value, I just want to verify that
  // HandleRequest isn't called.
  EXPECT_THAT(client_->reader_writer_->Write(req), Eq(true));

  ::grpc::Status status = client_->reader_writer_->Finish();
  // CANCELLED is generated by the gRPC library. There's no code in RpcHandler
  // to propagate the status set above in this case, but that doesn't seem
  // like a big problem.
  EXPECT_THAT(status.error_code(), Eq(::grpc::StatusCode::CANCELLED));

  // Create a new client and poke the server with it just to make sure that
  // everything works.
  std::unique_ptr<ClientConnection> client2(
      new ClientConnection(ServerAddress()));

  req.set_id(9);
  ASSERT_THAT(client2->reader_writer_->Write(req), Eq(true));

  // It would be nice to perform a Read() here, but we don't have access to the
  // new handler to set up expectations.

  status = client2->reader_writer_->Finish();
  EXPECT_THAT(status.ok(), Eq(true));
}

}  // namespace

}  // namespace net_instaweb
