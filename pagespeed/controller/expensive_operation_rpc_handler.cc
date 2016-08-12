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

#include "pagespeed/controller/expensive_operation_rpc_handler.h"

namespace net_instaweb {

ExpensiveOperationRpcHandler::ExpensiveOperationRpcHandler(
    grpc::CentralControllerRpcService::AsyncService* service,
    ::grpc::ServerCompletionQueue* cq, ExpensiveOperationController* controller)
    : RequestResultRpcHandler(service, cq, controller) {}

void ExpensiveOperationRpcHandler::HandleClientRequest(
    const ScheduleExpensiveOperationRequest& req, Function* callback) {
  controller()->ScheduleExpensiveOperation(callback);
}

void ExpensiveOperationRpcHandler::HandleClientResult(
    const ScheduleExpensiveOperationRequest& req) {
  controller()->NotifyExpensiveOperationComplete();
}

void ExpensiveOperationRpcHandler::HandleOperationFailed() {
  controller()->NotifyExpensiveOperationComplete();
}

void ExpensiveOperationRpcHandler::InitResponder(
    grpc::CentralControllerRpcService::AsyncService* service,
    ::grpc::ServerContext* ctx, ReaderWriterT* responder,
    ::grpc::ServerCompletionQueue* cq, void* callback) {
  service->RequestScheduleExpensiveOperation(ctx, responder, cq, cq, callback);
}

}  // namespace net_instaweb
