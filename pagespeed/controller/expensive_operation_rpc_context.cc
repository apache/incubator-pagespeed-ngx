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

#include "pagespeed/controller/expensive_operation_rpc_context.h"

#include <memory>

#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/controller/controller.pb.h"
#include "pagespeed/controller/expensive_operation_callback.h"
#include "pagespeed/controller/request_result_rpc_client.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/grpc.h"

namespace net_instaweb {

class ExpensiveOperationRpcContext::ExpensiveOperationRequestResultRpcClient
    : public RequestResultRpcClient<ScheduleExpensiveOperationRequest,
                                    ScheduleExpensiveOperationResponse,
                                    ExpensiveOperationCallback> {
 public:
  ExpensiveOperationRequestResultRpcClient(
      grpc::CentralControllerRpcService::StubInterface* stub,
      ::grpc::CompletionQueue* queue, ThreadSystem* thread_system,
      MessageHandler* handler, ExpensiveOperationCallback* callback)
      : RequestResultRpcClient(queue, thread_system, handler, callback) {
    // Nothing will happen until a call to Start() is made. We don't do it here
    // because the wrapper needs to call SetTransactionContext first.
  }

  std::unique_ptr<RequestResultRpcClient::ReaderWriter> StartRpc(
      grpc::CentralControllerRpcService::StubInterface* stub,
      ::grpc::ClientContext* context, ::grpc::CompletionQueue* queue,
      void* tag) override {
    return stub->AsyncScheduleExpensiveOperation(context, queue, tag);
  }

  ~ExpensiveOperationRequestResultRpcClient() {
    Done();
  }

  void Done() {
    ScheduleExpensiveOperationRequest req;
    // req has no fields.
    SendResultToServer(req);
  }

 private:
  void PopulateServerRequest(
      ScheduleExpensiveOperationRequest* request) override {
    // request has no fields, so this is a no-op.
  }
};

ExpensiveOperationRpcContext::ExpensiveOperationRpcContext(
    grpc::CentralControllerRpcService::StubInterface* stub,
    ::grpc::CompletionQueue* queue, ThreadSystem* thread_system,
    MessageHandler* handler, ExpensiveOperationCallback* callback)
    : client_(new ExpensiveOperationRequestResultRpcClient(
          stub, queue, thread_system, handler, callback)) {
  // SetTransactionContext takes ownership of "this".
  callback->SetTransactionContext(this);
  client_->Start(stub);
}

void ExpensiveOperationRpcContext::Done() { client_->Done(); }

}  // namespace net_instaweb
