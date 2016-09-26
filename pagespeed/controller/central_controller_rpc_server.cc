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

#include "pagespeed/controller/central_controller_rpc_server.h"

#include <memory>

#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/controller/expensive_operation_rpc_handler.h"
#include "pagespeed/controller/schedule_rewrite_rpc_handler.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

CentralControllerRpcServer::CentralControllerRpcServer(
    const GoogleString& listen_address,
    ExpensiveOperationController* expensive_operation_controller,
    ScheduleRewriteController* rewrite_controller, MessageHandler* handler)
    : listen_address_(listen_address),
      expensive_operation_controller_(expensive_operation_controller),
      rewrite_controller_(rewrite_controller),
      handler_(handler) {
}

int CentralControllerRpcServer::Setup() {
  ::grpc::ServerBuilder builder;
  // InsecureServerCredentials means unencrytped, unauthenticated. In future
  // we may wish to look into different Credentials which would allow us to
  // encrypt and/or authenticate.
  builder.AddListeningPort(listen_address_,
                           ::grpc::InsecureServerCredentials());
  builder.RegisterService(&service_);
  queue_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();
  if (server_ == nullptr) {
    PS_LOG_ERROR(handler_, "CentralControllerRpcServer failed to start");
    return 1;
  }

  ExpensiveOperationRpcHandler::CreateAndStart(
      &service_, queue_.get(), expensive_operation_controller_.get());

  ScheduleRewriteRpcHandler::CreateAndStart(&service_, queue_.get(),
                                            rewrite_controller_.get());
  return 0;
}

int CentralControllerRpcServer::Run() {
  PS_LOG_INFO(handler_,
              "CentralControllerRpcServer processing requests on %s",
              listen_address_.c_str());

  MainLoop(queue_.get());

  PS_LOG_INFO(handler_, "CentralControllerRpcServer terminated");
  return 0;
}

void CentralControllerRpcServer::MainLoop(::grpc::CompletionQueue* queue) {
  void* tag;
  bool succeeded;
  while (queue->Next(&tag, &succeeded)) {
    Function* function = static_cast<Function*>(tag);
    if (succeeded) {
      function->CallRun();
    } else {
      function->CallCancel();
    }
  }
}

void CentralControllerRpcServer::Stop() {
  PS_LOG_INFO(handler_, "Shutting down CentralControllerRpcServer.");
  // Stop accepting new RPCs and forcibly terminate all outstanding ones. Blocks
  // until cancel callbacks have been invoked on all oustanding RPCs.
  // It doesn't make much sense to try and wait here, since mostly the client
  // is waiting for us and a clean shutdown doesn't make any difference since we
  // don't actually write any state to disk.
  server_->Shutdown(gpr_inf_past(GPR_CLOCK_MONOTONIC));
  // This should terminate the event loop immediately.
  queue_->Shutdown();
}

}  // namespace net_instaweb
