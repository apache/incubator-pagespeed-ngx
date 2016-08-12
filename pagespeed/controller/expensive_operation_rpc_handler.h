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

#ifndef PAGESPEED_CONTROLLER_EXPENSIVE_OPERATION_RPC_HANDLER_H_
#define PAGESPEED_CONTROLLER_EXPENSIVE_OPERATION_RPC_HANDLER_H_

#include "pagespeed/controller/controller.pb.h"
#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/controller/rpc_handler.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/util/grpc.h"
#include "pagespeed/controller/expensive_operation_controller.h"
#include "pagespeed/controller/request_result_rpc_handler.h"

namespace net_instaweb {

// RpcHandler for ExpensiveOperationController.
//
// The request message on the RPC contains no payload, it's just the client
// saying "I have something expensive to do now, let me know when". This
// will trigger a call to HandleClientRequest() which we use to call
// ScheduleExpensiveOperation(). When the controller decides if it will allow
// the rewrite to proceed, RequestResultRpcHandler returns that decision to
// the client. Once the client completes, it sends another Request message,
// which will trigger a call to HandleClientResult() and we in-turn call
// NotifyExpensiveOperationComplete().
//
// If the client disconnects after requesting an operation but before sending a
// second "completed" message, we receive a call to HandleOperationFailed() and
// will call NotifyExpensiveOperationComplete() on the controller, so it can
// release "locks".

class ExpensiveOperationRpcHandler
    : public RequestResultRpcHandler<
          ExpensiveOperationRpcHandler, ExpensiveOperationController,
          grpc::CentralControllerRpcService::AsyncService,
          ScheduleExpensiveOperationRequest,
          ScheduleExpensiveOperationResponse> {
 protected:
  ExpensiveOperationRpcHandler(
      grpc::CentralControllerRpcService::AsyncService* service,
      ::grpc::ServerCompletionQueue* cq,
      ExpensiveOperationController* controller);

  // RequestResultRpcHandler implementation.
  void HandleClientRequest(
      const ScheduleExpensiveOperationRequest& req, Function* cb) override;
  void HandleClientResult(
      const ScheduleExpensiveOperationRequest& req) override;
  void HandleOperationFailed() override;

  void InitResponder(grpc::CentralControllerRpcService::AsyncService* service,
                     ::grpc::ServerContext* ctx, ReaderWriterT* responder,
                     ::grpc::ServerCompletionQueue* cq,
                     void* callback) override;

 private:
  // Allow access to protected constructor.
  friend class RequestResultRpcHandler;
  friend class ExpensiveOperationRpcHandlerTest;

  DISALLOW_COPY_AND_ASSIGN(ExpensiveOperationRpcHandler);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_EXPENSIVE_OPERATION_RPC_HANDLER_H_
