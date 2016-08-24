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

#ifndef PAGESPEED_CONTROLLER_CONTROLLER_GRPC_MOCKS_H_
#define PAGESPEED_CONTROLLER_CONTROLLER_GRPC_MOCKS_H_

#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/controller/controller.pb.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gmock.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/thread/sequence.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/kernel/util/grpc.h"

// Various mocks and related classes that are useful for testing client-side
// CentralController gRPC stuff.

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::WithArgs;
using testing::Return;
using testing::SetArgPointee;

namespace net_instaweb {

// Function wrapper that Notify()s a SyncPoint after running another contained
// Function.

class NotifyFunction : public Function {
 public:
  NotifyFunction(Function* func, WorkerTestBase::SyncPoint* sync)
      : func_(func), sync_(sync) {}

  void Run() override {
    func_->CallRun();
    sync_->Notify();
  }

  void Cancel() override {
    func_->CallCancel();
    sync_->Notify();
  }

 private:
  Function* func_;
  WorkerTestBase::SyncPoint* sync_;
};

// Mock for grpc::ClientAsyncReaderWriterInterface.
// All callbacks are invoked asynchronously through a Sequence to mimic the
// equivalent gRPC behaviour which uses a gRPC CompletionQueue.

template <typename RequestT, typename ResponseT>
class MockReaderWriterT
    : public ::grpc::ClientAsyncReaderWriterInterface<RequestT, ResponseT> {
 public:
  MockReaderWriterT(Sequence* sequence) : sequence_(sequence) {
    EXPECT_CALL(*this, WritesDone(_)).Times(0);
    EXPECT_CALL(*this, ReadInitialMetadata(_)).Times(0);
    EXPECT_CALL(*this, Finish(_, _)).Times(0);
    EXPECT_CALL(*this, Write(_, _)).Times(0);
    EXPECT_CALL(*this, Read(_, _)).Times(0);
  }

  void ExpectRead(const GoogleString& asciiProto) {
    ResponseT resp;
    ASSERT_THAT(ParseTextFormatProtoFromString(asciiProto, &resp), Eq(true));

    EXPECT_CALL(*this, Read(_, _))
        .WillOnce(DoAll(
            SetArgPointee<0>(resp),
            WithArgs<1>(Invoke(this, &MockReaderWriterT::QueueVoidFunction))));
  }

  void ExpectReadFailure() {
    EXPECT_CALL(*this, Read(_, _))
        .WillOnce(WithArgs<1>(
            Invoke(this, &MockReaderWriterT::QueueVoidFunctionForCancel)));
  }

  // Matcher winds up being a complicated type that includes testing::internal
  // stuff, so we just template on it.
  template <typename Matcher>
  void ExpectWrite(const Matcher& matcher) {
    EXPECT_CALL(*this, Write(matcher, _))
        .WillOnce(
            WithArgs<1>(Invoke(this, &MockReaderWriterT::QueueVoidFunction)));
  }

  template <typename Matcher>
  void ExpectWriteFailure(const Matcher& matcher) {
    EXPECT_CALL(*this, Write(matcher, _))
        .WillOnce(WithArgs<1>(
            Invoke(this, &MockReaderWriterT::QueueVoidFunctionForCancel)));
  }

  void ExpectFinish(const ::grpc::Status& status) {
    EXPECT_CALL(*this, Finish(_, _))
        .WillOnce(DoAll(
            SetArgPointee<0>(status),
            WithArgs<1>(Invoke(this, &MockReaderWriterT::QueueVoidFunction))));
  }

  void ExpectFinishAndNotify(const ::grpc::Status& status,
                             WorkerTestBase::SyncPoint* sync) {
    EXPECT_CALL(*this, Finish(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(status),
                        WithArgs<1>(Invoke([this, sync](void* f) {
                          this->QueueVoidFunctionWithNotify(f, sync);
                        }))));
  }

  void ExpectFinishFailure() {
    EXPECT_CALL(*this, Finish(_, _))
        .WillOnce(WithArgs<1>(
            Invoke(this, &MockReaderWriterT::QueueVoidFunctionForCancel)));
  }

  MOCK_METHOD1(WritesDone, void(void* tag));
  MOCK_METHOD1(ReadInitialMetadata, void(void* tag));
  MOCK_METHOD2(Finish, void(::grpc::Status* status, void* tag));
  MOCK_METHOD2_T(Write, void(const RequestT& resp, void* tag));
  MOCK_METHOD2_T(Read, void(ResponseT* resp, void* tag));

 private:
  void QueueVoidFunction(void* fv) {
    Function* f = static_cast<Function*>(fv);
    sequence_->Add(f);
  }

  void QueueVoidFunctionForCancel(void* fv) {
    Function* f = static_cast<Function*>(fv);
    sequence_->Add(MakeFunction(f, &Function::CallCancel));
  }

  void QueueVoidFunctionWithNotify(void* fv, WorkerTestBase::SyncPoint* sync) {
    Function* f = static_cast<Function*>(fv);
    sequence_->Add(new NotifyFunction(f, sync));
  }

  Sequence* sequence_;
};

// Mock for CentralControllerRpcServiceStub. Mostly used just to bootstrap
// a MockReaderWriterT, this also features deferred execution to mimic gRPC.

class MockCentralControllerRpcServiceStub
    : public grpc::CentralControllerRpcService::StubInterface {
 public:
  MockCentralControllerRpcServiceStub(Sequence* sequence)
      : sequence_(sequence) {
    EXPECT_CALL(*this, ScheduleExpensiveOperationRaw(_)).Times(0);
    EXPECT_CALL(*this, ScheduleRewriteRaw(_)).Times(0);
    EXPECT_CALL(*this, AsyncScheduleExpensiveOperationRaw(_, _, _)).Times(0);
    EXPECT_CALL(*this, AsyncScheduleRewriteRaw(_, _, _)).Times(0);
  }

  MOCK_METHOD1(
      ScheduleExpensiveOperationRaw,
      ::grpc::ClientReaderWriterInterface<
          ::net_instaweb::ScheduleExpensiveOperationRequest,
          ::net_instaweb::
              ScheduleExpensiveOperationResponse>*(::grpc::ClientContext*));

  MOCK_METHOD1(
      ScheduleRewriteRaw,
      ::grpc::ClientReaderWriterInterface<
          ::net_instaweb::ScheduleRewriteRequest,
          ::net_instaweb::ScheduleRewriteResponse>*(::grpc::ClientContext*));

  MOCK_METHOD3(
      AsyncScheduleExpensiveOperationRaw,
      ::grpc::ClientAsyncReaderWriterInterface<
          ::net_instaweb::ScheduleExpensiveOperationRequest,
          net_instaweb::
              ScheduleExpensiveOperationResponse>*(::grpc::ClientContext*,
                                                   ::grpc::CompletionQueue*,
                                                   void*));

  MOCK_METHOD3(
      AsyncScheduleRewriteRaw,
      ::grpc::ClientAsyncReaderWriterInterface<
          ::net_instaweb::ScheduleRewriteRequest,
          ::net_instaweb::ScheduleRewriteResponse>*(::grpc::ClientContext*,
                                                    ::grpc::CompletionQueue*,
                                                    void*));

  void ExpectAsyncScheduleExpensiveOperation(
      ::grpc::ClientAsyncReaderWriterInterface<
          ::net_instaweb::ScheduleExpensiveOperationRequest,
          ::net_instaweb::ScheduleExpensiveOperationResponse>* rw) {
    // Configure the stub to invoke the callback and return rw in response to
    // a client initiating a request.
    EXPECT_CALL(*this,
                AsyncScheduleExpensiveOperationRaw(_, nullptr /* queue */, _))
        .WillOnce(DoAll(WithArgs<2>(Invoke([this](void* fv) {
                          sequence_->Add(static_cast<Function*>(fv));
                        })),
                        Return(rw)));
  }

  void ExpectAsyncScheduleExpensiveOperationFailure(
      ::grpc::ClientAsyncReaderWriterInterface<
          ::net_instaweb::ScheduleExpensiveOperationRequest,
          ::net_instaweb::ScheduleExpensiveOperationResponse>* rw) {
    // Configure the stub to invoke the Cancel callback and return rw in
    // response to a client initiating a request.
    EXPECT_CALL(*this,
                AsyncScheduleExpensiveOperationRaw(_, nullptr /* queue */, _))
        .WillOnce(DoAll(WithArgs<2>(Invoke([this](void* fv) {
                          sequence_->Add(
                              MakeFunction(static_cast<Function*>(fv),
                                           &Function::CallCancel));
                        })),
                        Return(rw)));
  }

  void ExpectAsyncScheduleRewrite(
      ::grpc::ClientAsyncReaderWriterInterface<
          ::net_instaweb::ScheduleRewriteRequest,
          ::net_instaweb::ScheduleRewriteResponse>* rw) {
    // Configure the stub to invoke the callback and return rw in response to
    // a client initiating a request.
    EXPECT_CALL(*this,
                AsyncScheduleRewriteRaw(_, nullptr /* queue */, _))
        .WillOnce(DoAll(WithArgs<2>(Invoke([this](void* fv) {
                          sequence_->Add(static_cast<Function*>(fv));
                        })),
                        Return(rw)));
  }

  void ExpectAsyncScheduleRewriteFailure(
      ::grpc::ClientAsyncReaderWriterInterface<
          ::net_instaweb::ScheduleRewriteRequest,
          ::net_instaweb::ScheduleRewriteResponse>* rw) {
    // Configure the stub to invoke the Cancel callback and return rw in
    // response to a client initiating a request.
    EXPECT_CALL(*this,
                AsyncScheduleRewriteRaw(_, nullptr /* queue */, _))
        .WillOnce(DoAll(WithArgs<2>(Invoke([this](void* fv) {
                          sequence_->Add(
                              MakeFunction(static_cast<Function*>(fv),
                                           &Function::CallCancel));
                        })),
                        Return(rw)));
  }

 private:
  Sequence* sequence_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_CONTROLLER_GRPC_MOCKS_H_
