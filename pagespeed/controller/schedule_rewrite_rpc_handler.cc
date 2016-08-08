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

class ScheduleRewriteRpcHandler::NotifyClientCallback : public Function {
 public:
  NotifyClientCallback(ScheduleRewriteRpcHandler* handler)
      : handler_(handler) {}

  void Run() override {
    handler_->NotifyClient(true /* ok_to_rewrite */);
  }

  void Cancel() override {
    handler_->NotifyClient(false /* ok_to_rewrite */);
  }

 private:
  // The client may hangup before the Controller makes up its mind. We retain
  // a RefPtr to the handler to ensure that it doesn't delete itself until we
  // are done with it.
  ScheduleRewriteRpcHandler::RefPtr handler_;
};

ScheduleRewriteRpcHandler::ScheduleRewriteRpcHandler(
    grpc::CentralControllerRpcService::AsyncService* service,
    ::grpc::ServerCompletionQueue* cq, ScheduleRewriteController* controller)
    : RpcHandler<grpc::CentralControllerRpcService::AsyncService,
                 ScheduleRewriteRequest,
                 ScheduleRewriteResponse>::RpcHandler(service, cq),
      controller_(controller), state_(INIT) {}

ScheduleRewriteRpcHandler::~ScheduleRewriteRpcHandler() {}

void ScheduleRewriteRpcHandler::HandleClientRequest(
    const ScheduleRewriteRequest& req) {
  // This could also return a response with ok_to_rewrite = false, but
  // this seems more appropriate when the client is violating the protocol.
  if (req.key().empty() || req.status() != ScheduleRewriteRequest::PENDING) {
    LOG(ERROR) << "Malformed request to HandleRewriteRequest";
    Finish(::grpc::Status(::grpc::StatusCode::ABORTED,
                          "Protocol error (HandleRewriteRequest)"));
    return;
  }
  key_ = req.key();
  state_ = WAITING_FOR_CONTROLLER;
  controller_->ScheduleRewrite(key_, new NotifyClientCallback(this));
}

void ScheduleRewriteRpcHandler::HandleClientResult(
    const ScheduleRewriteRequest& req) {
  if ((!req.key().empty() && req.key() != key_) ||
      req.status() == ScheduleRewriteRequest::PENDING) {
    LOG(ERROR) << "Malformed request to HandleRewriteResult";
    controller_->NotifyRewriteFailed(key_);
    Finish(::grpc::Status(::grpc::StatusCode::ABORTED,
                          "Protocol error (HandleRewriteRequest)"));
  }
  if (req.status() == ScheduleRewriteRequest::SUCCESS) {
    controller_->NotifyRewriteComplete(key_);
  } else {
    controller_->NotifyRewriteFailed(key_);
  }
  Finish(::grpc::Status());
}

void ScheduleRewriteRpcHandler::HandleRequest(
    const ScheduleRewriteRequest& req) {
  switch (state_) {
    case INIT:
      HandleClientRequest(req);
      break;

    case REWRITE_RUNNING:
      HandleClientResult(req);
      break;

    default:
      LOG(ERROR) << "HandleRequest in unexpected state: " << state_;
      Finish(::grpc::Status(::grpc::StatusCode::ABORTED,
                            "State machine error (HandleRequest)"));
  }
}

void ScheduleRewriteRpcHandler::HandleError() {
  if (state_ == REWRITE_RUNNING) {
    controller_->NotifyRewriteFailed(key_);
  }
  // If we're in WAITING_FOR_CONTROLLER, this will cause a failure notification
  // when the controller calls back into NotifyClient().
  state_ = DONE;
}

void ScheduleRewriteRpcHandler::NotifyClient(bool ok_to_rewrite) {
  if (state_ != WAITING_FOR_CONTROLLER) {
    // Either the client disconnected (DONE) or something bad is happening.
    // Either way, tell the Controller that no rewrite happened.
    controller_->NotifyRewriteFailed(key_);
    if (state_ != DONE) {
      // If this fires, it's likely a coding error in this class. It should not
      // be possible just due to client misbehaviour.
      Finish(::grpc::Status(::grpc::StatusCode::ABORTED,
                            "State machine error (NotifyClient)"));
      LOG(DFATAL) << "NotifyClient in unexpected state: " << state_;
    }
    return;
  }

  // Actually inform the client of the Controller's decision.
  ScheduleRewriteResponse resp;
  resp.set_ok_to_rewrite(ok_to_rewrite);
  bool write_ok = Write(resp);
  if (write_ok && ok_to_rewrite) {
    state_ = REWRITE_RUNNING;
  } else if (write_ok && !ok_to_rewrite) {
    // Client isn't allowed to call back, so mark done.
    Finish(::grpc::Status());
  } else /* !write_ok */ {
    // Client already disconnected, mark as failed.
    controller_->NotifyRewriteFailed(key_);
    state_ = DONE;
  }
}

void ScheduleRewriteRpcHandler::Finish(const ::grpc::Status& status) {
  state_ = DONE;
  RpcHandler<grpc::CentralControllerRpcService::AsyncService,
             ScheduleRewriteRequest, ScheduleRewriteResponse>::Finish(status);
}

void ScheduleRewriteRpcHandler::InitResponder(
    grpc::CentralControllerRpcService::AsyncService* service,
    ::grpc::ServerContext* ctx, ReaderWriterT* responder,
    ::grpc::ServerCompletionQueue* cq, void* callback) {
  service->RequestScheduleRewrite(ctx, responder, cq, cq, callback);
}

void ScheduleRewriteRpcHandler::CreateAndStart(
    grpc::CentralControllerRpcService::AsyncService* service,
    ::grpc::ServerCompletionQueue* cq, ScheduleRewriteController* controller) {
  (new ScheduleRewriteRpcHandler(service, cq, controller))->Start();
}

ScheduleRewriteRpcHandler* ScheduleRewriteRpcHandler::CreateHandler(
    grpc::CentralControllerRpcService::AsyncService* service,
    ::grpc::ServerCompletionQueue* cq) {
  return new ScheduleRewriteRpcHandler(service, cq, controller_);
}

}  // namespace net_instaweb
