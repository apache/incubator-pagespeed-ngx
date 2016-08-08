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

#ifndef PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_RPC_HANDLER_H_
#define PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_RPC_HANDLER_H_

#include "pagespeed/controller/controller.pb.h"
#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/util/grpc.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/controller/rpc_handler.h"
#include "pagespeed/controller/schedule_rewrite_controller.h"

namespace net_instaweb {

// RpcHandler for ScheduleRewriteController.
//
// The first message on the RPC contains the key that the client wants to
// rewrite. When the controller decides if it will allow the rewrite to
// proceed, we return that decision to the client. Once the client completes,
// it sends back success or failure and we hand that to the Controller.
// If the client disconnects after requesting a rewrite but before sending a
// response, we call NotifyRewriteFailed on the controller, so it can release
// any locks.

class ScheduleRewriteRpcHandler
    : public RpcHandler<grpc::CentralControllerRpcService::AsyncService,
                        ScheduleRewriteRequest, ScheduleRewriteResponse> {
 public:
  typedef RefCountedPtr<ScheduleRewriteRpcHandler> RefPtr;
  virtual ~ScheduleRewriteRpcHandler();

  // Call this to create a handler and add it to the gRPC event loop. It will
  // free itself.
  static void CreateAndStart(
      grpc::CentralControllerRpcService::AsyncService* service,
      ::grpc::ServerCompletionQueue* cq, ScheduleRewriteController* controller);

 protected:
  ScheduleRewriteRpcHandler(
      grpc::CentralControllerRpcService::AsyncService* service,
      ::grpc::ServerCompletionQueue* cq, ScheduleRewriteController* controller);

  // Hide the parent implementation so we can frob our own state machine.
  void Finish(const ::grpc::Status& status);

  // RpcHandler implementation.
  void HandleRequest(const ScheduleRewriteRequest& req) override;
  void HandleError() override;

  void InitResponder(grpc::CentralControllerRpcService::AsyncService* service,
                     ::grpc::ServerContext* ctx, ReaderWriterT* responder,
                     ::grpc::ServerCompletionQueue* cq,
                     void* callback) override;

  ScheduleRewriteRpcHandler* CreateHandler(
      grpc::CentralControllerRpcService::AsyncService* service,
      ::grpc::ServerCompletionQueue* cq) override;

 private:
  // This state machine is very similar to the one in rpc_handler.h.
  // However, trying to be too clever and merging them seems more dangerous
  // than useful.
  enum State {
    INIT,
    WAITING_FOR_CONTROLLER,
    REWRITE_RUNNING,
    DONE,
  };

  class NotifyClientCallback;

  // This are just dispatched to from HandleRequest based on the value of
  // the state machine. ClientRequest is for the first message to initiate
  // a rewrite, ClientResult is for the second that contains the success/failure
  // result.
  void HandleClientRequest(const ScheduleRewriteRequest& req);
  void HandleClientResult(const ScheduleRewriteRequest& req);

  // Inform the client of the Controller's decision. This is invoked by the
  // controller via a NotifyClientCallback passed into ScheduleRewrite().
  void NotifyClient(bool ok_to_rewrite);

  ScheduleRewriteController* controller_;
  State state_;
  GoogleString key_;  // What we told the controller that we're rewriting.

  friend class NotifyClientCallback;
  friend class ScheduleRewriteRpcHandlerTest;

  DISALLOW_COPY_AND_ASSIGN(ScheduleRewriteRpcHandler);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_RPC_HANDLER_H_
