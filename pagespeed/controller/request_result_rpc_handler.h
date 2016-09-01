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

#ifndef PAGESPEED_CONTROLLER_REQUEST_RESULT_RPC_HANDLER_H_
#define PAGESPEED_CONTROLLER_REQUEST_RESULT_RPC_HANDLER_H_

#include "base/logging.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/controller/rpc_handler.h"
#include "pagespeed/kernel/util/grpc.h"

// RpcHandler for the case there the client uses a streaming RPC to the server
// to attempt an operation, waits for response and then calls back to let the
// server know it's done.
//
// The first message on the RPC will result in a call to HandleClientRequest(),
// which the client should use to notify its controller of a request. When the
// controller decides if it will allow the rewrite to proceed it invokes the
// provided callback and we return that decision to the client via
// NotifyClient(). Once the client completes, it sends back a final message
// which will result in a final call to HandleClientResult().
//
// If the client disconnects after the call to HandleClientRequest() but before
// the call to HandleClientResult(), we call HandleOperationFailed() to let the
// subclass know.

namespace net_instaweb {

template <typename HandlerT, typename ControllerT, typename AsyncServiceT,
          typename RequestT, typename ResponseT>
class RequestResultRpcHandler
    : public RpcHandler<AsyncServiceT, RequestT, ResponseT> {
 public:
  typedef RefCountedPtr<RequestResultRpcHandler> RefPtr;

  virtual ~RequestResultRpcHandler() {}

  // Call this to create a handler and add it to the gRPC event loop. It will
  // free itself.
  static void CreateAndStart(AsyncServiceT* service,
                             ::grpc::ServerCompletionQueue* cq,
                             ControllerT* controller) {
    (new HandlerT(service, cq, controller))->Start();
  }

 protected:
  RequestResultRpcHandler(AsyncServiceT* service,
                          ::grpc::ServerCompletionQueue* cq,
                          ControllerT* controller)
      : RpcHandler<AsyncServiceT, RequestT, ResponseT>::RpcHandler(service, cq),
        controller_(controller),
        state_(INIT) {}

  // Hide the parent implementation so we can frob our own state machine.
  void Finish(const ::grpc::Status& status) {
    state_ = DONE;
    RpcHandler<AsyncServiceT, RequestT, ResponseT>::Finish(status);
  }

  ControllerT* controller() { return controller_; }

 private:
  // This state machine is very similar to the one in rpc_handler.h.
  // However, trying to be too clever and merging them seems more dangerous
  // than useful.
  enum State {
    INIT,
    WAITING_FOR_CONTROLLER,
    OPERATION_RUNNING,
    DONE,
  };

  // Callback passed to HandleClientRequest, which the controller will use to
  // signify "Go ahead" or not.
  class NotifyClientCallback : public Function {
   public:
    NotifyClientCallback(RequestResultRpcHandler* handler)
        : handler_(handler) {}

    void Run() override { handler_->NotifyClient(true /* can_proceed */); }
    void Cancel() override { handler_->NotifyClient(false /* can_proceed */); }

   private:
    // The client may hangup before the Controller makes up its mind. We retain
    // a RefPtr to the handler to ensure that it doesn't delete itself until we
    // are done with it.
    RequestResultRpcHandler::RefPtr handler_;
  };

  // HandleRequest dispatches to these based on the value of the state machine.
  // ClientRequest is for the first message to initiate an operation. If you
  // abort the operation by calling Finish, you should delete the callback.
  // ClientResult is for the second that contains the success/failure result.
  virtual void HandleClientRequest(const RequestT& req, Function* cb) = 0;
  virtual void HandleClientResult(const RequestT& req) = 0;

  // Called if anything goes wrong in WAITING_FOR_CONTROLLER or
  // OPERATION_RUNNING states. After such a call, the state will be DONE and no
  // other calls will be made.
  virtual void HandleOperationFailed() = 0;

  // Inform the client of the Controller's decision. This is invoked by the
  // controller via a NotifyClientCallback passed into HandleClientRequest().
  void NotifyClient(bool ok_to_rewrite);

  // RpcHandler implementation.
  void HandleRequest(const RequestT& req) override;
  void HandleError() override;

  RpcHandler<AsyncServiceT, RequestT, ResponseT>* CreateHandler(
      AsyncServiceT* service, ::grpc::ServerCompletionQueue* cq) override {
    return new HandlerT(service, cq, controller());
  }

  // Also from RpcHandler, but we can't implement this.
  void InitResponder(AsyncServiceT* service, ::grpc::ServerContext* ctx,
                     typename RpcHandler<AsyncServiceT, RequestT,
                                         ResponseT>::ReaderWriterT* responder,
                     ::grpc::ServerCompletionQueue* cq,
                     void* callback) override = 0;

  // This is on RequestResultRpcHandler solely to allow CreateHandler() to
  // be implemented here and not in yet more boilerplate on the subclass.
  ControllerT* controller_;
  State state_;

  friend class NotifyClientCallback;
  friend class RequestResultRpcHandlerTest;

  DISALLOW_COPY_AND_ASSIGN(RequestResultRpcHandler);
};

template <typename HandlerT, typename ControllerT, typename AsyncServiceT,
          typename RequestT, typename ResponseT>
void RequestResultRpcHandler<HandlerT, ControllerT, AsyncServiceT, RequestT,
                             ResponseT>::HandleRequest(const RequestT& req) {
  switch (state_) {
    case INIT:
      state_ = WAITING_FOR_CONTROLLER;
      HandleClientRequest(req, new NotifyClientCallback(this));
      break;

    case OPERATION_RUNNING:
      HandleClientResult(req);
      // The above may have called Finish if something bad happened, but
      // redundant calls to Finish are ignored.
      Finish(::grpc::Status());
      break;

    default:
      LOG(ERROR) << "HandleRequest in unexpected state: " << state_;
      Finish(::grpc::Status(::grpc::StatusCode::ABORTED,
                            "State machine error (HandleRequest)"));
  }
}

template <typename HandlerT, typename ControllerT, typename AsyncServiceT,
          typename RequestT, typename ResponseT>
void RequestResultRpcHandler<HandlerT, ControllerT, AsyncServiceT, RequestT,
                             ResponseT>::HandleError() {
  if (state_ == OPERATION_RUNNING) {
    HandleOperationFailed();
  }
  // If we're in WAITING_FOR_CONTROLLER, this will cause a failure notification
  // when the controller calls back into NotifyClient().
  state_ = DONE;
}

template <typename HandlerT, typename ControllerT, typename AsyncServiceT,
          typename RequestT, typename ResponseT>
void RequestResultRpcHandler<HandlerT, ControllerT, AsyncServiceT, RequestT,
                             ResponseT>::NotifyClient(bool ok_to_proceed) {
  if (state_ != WAITING_FOR_CONTROLLER) {
    // Either the client disconnected (DONE) or something bad is happening.
    // If the client's controller just told us to do work, we cannot, so tell
    // the Controller that we did nothing.
    if (ok_to_proceed) {
      HandleOperationFailed();
    }
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
  // Instead of hard-coding the use of ok_to_proceed, this could be a down-call.
  // However, this meets our needs just fine right now.
  ResponseT resp;
  resp.set_ok_to_proceed(ok_to_proceed);
  bool write_ok = this->Write(resp);
  if (write_ok && ok_to_proceed) {
    state_ = OPERATION_RUNNING;
  } else if (write_ok && !ok_to_proceed) {
    // Client isn't allowed to call back, so mark done.
    Finish(::grpc::Status());
  } else /* !write_ok */ {
    // Client already disconnected, mark as failed.
    HandleOperationFailed();
    state_ = DONE;
  }
}

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_REQUEST_RESULT_RPC_HANDLER_H_
