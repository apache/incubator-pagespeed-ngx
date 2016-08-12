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

#include "pagespeed/controller/schedule_rewrite_rpc_handler.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/util/grpc.h"

namespace net_instaweb {

ScheduleRewriteRpcHandler::ScheduleRewriteRpcHandler(
    grpc::CentralControllerRpcService::AsyncService* service,
    ::grpc::ServerCompletionQueue* cq, ScheduleRewriteController* controller)
    : RequestResultRpcHandler(service, cq, controller) {}

void ScheduleRewriteRpcHandler::HandleClientRequest(
    const ScheduleRewriteRequest& req, Function* callback) {
  // This could also return a response with ok_to_proceed = false, but
  // this seems more appropriate when the client is violating the protocol.
  if (req.key().empty() || req.status() != ScheduleRewriteRequest::PENDING) {
    LOG(ERROR) << "Malformed request to HandleRewriteRequest";
    Finish(::grpc::Status(::grpc::StatusCode::ABORTED,
                          "Protocol error (HandleRewriteRequest)"));
    // Delete the callback without running it, which requires clearing delete
    // after callback.
    callback->set_delete_after_callback(false);
    delete callback;
    return;
  }
  key_ = req.key();
  controller()->ScheduleRewrite(key_, callback);
}

void ScheduleRewriteRpcHandler::HandleClientResult(
    const ScheduleRewriteRequest& req) {
  if ((!req.key().empty() && req.key() != key_) ||
      req.status() == ScheduleRewriteRequest::PENDING) {
    LOG(ERROR) << "Malformed request to HandleRewriteResult";
    controller()->NotifyRewriteFailed(key_);
    Finish(::grpc::Status(::grpc::StatusCode::ABORTED,
                          "Protocol error (HandleRewriteRequest)"));
    return;
  }
  if (req.status() == ScheduleRewriteRequest::SUCCESS) {
    controller()->NotifyRewriteComplete(key_);
  } else {
    controller()->NotifyRewriteFailed(key_);
  }
}

void ScheduleRewriteRpcHandler::HandleOperationFailed() {
  controller()->NotifyRewriteFailed(key_);
}

void ScheduleRewriteRpcHandler::InitResponder(
    grpc::CentralControllerRpcService::AsyncService* service,
    ::grpc::ServerContext* ctx, ReaderWriterT* responder,
    ::grpc::ServerCompletionQueue* cq, void* callback) {
  service->RequestScheduleRewrite(ctx, responder, cq, cq, callback);
}

}  // namespace net_instaweb
