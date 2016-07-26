#ifndef PAGESPEED_CONTROLLER_RPC_HANDLER_H_
#define PAGESPEED_CONTROLLER_RPC_HANDLER_H_

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

#include "base/logging.h"
#include "base/macros.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/util/grpc.h"

namespace net_instaweb {

// Class that manages the server side of a single gRPC bi-directional streaming
// RPC call; the invocation of a single method on a stub. We use bi-directional
// streaming RPCs because they allow us to send multiple messages over a single
// session and detect if either end disconnects.
//
// RpcHandler should be bootstrapped by calling Start() on an instance, after
// which subsequent instances are automatically created as needed. The class
// cleans up after itself (ie: You call new without calling delete). This is
// modeled after CallData in gRPCs
// examples/cpp/helloworld/greeter_async_server.cc.
//
// Every message received from a client will result in a call to HandleRequest.
// If gRPC detects a problem with the client (Read/Write failure, disconnect)
// after the first call to HandleRequest() but before Finish() is called,
// HandleError() will be invoked.
//
// The class is expected to be manipulated on a single thread, so it should be
// considered thread compatible.

// Template args:
// AsyncService should be the AsyncService member of the service in your proto.
// For example, given:
// service SampleService {
//   rpc SaveConfiguration(stream Req) returns(stream Resp) {}
// }
// the template should be <SampleService::AsyncService, Req, Resp>
template <typename AsyncService, typename RequestT, typename ResponseT>
class RpcHandler
    : public RefCounted<RpcHandler<AsyncService, RequestT, ResponseT>> {
 public:
  virtual ~RpcHandler() { }

 protected:
  typedef ::grpc::ServerAsyncReaderWriter<ResponseT, RequestT> ReaderWriterT;

  RpcHandler(AsyncService* service, ::grpc::ServerCompletionQueue* cq);

  // Send a response to the client. Returns true if the Write was successfully
  // queued or false if the message cannot be sent, which may be because the
  // client is not connected or because there is already a message outstanding.
  // Once the Write has queued, one of either HandleWriteDone() or HandleError()
  // will be called, depending on success.
  //
  // The single queued write is a restriction of the gRPC ReaderWriter class.
  // If desired, it would be simple to expand this class with a write queue.
  bool Write(const ResponseT& resp);

  // Disconnect the client. Returns true if the client was not already
  // disconnected. HandleError will not be called after Finish.
  bool Finish(const ::grpc::Status& status);

  // Invokes InitResponder with the various arguments that are private
  // members of this class.  This should be called in a factory used to create
  // the initial RpcHandler. RpcHandler will take care of calling it on all the
  // subsequent ones made with CreateHandler().
  void Start();

 private:
  enum State {
    INIT,
    WAITING_FOR_FIRST_READ,
    RUNNING,
    FINISHED,
  };

  typedef RefCountedPtr<RpcHandler<AsyncService, RequestT, ResponseT>>
      RefPtrT;

  // Called once for every message received from the client.
  virtual void HandleRequest(const RequestT& req) = 0;

  // Called if any of the gRPC operations fail after the first call to
  // HandeRequest, after which the client will be disconnected (ie: It will
  // only be called once). 'this' may be freed immediately upon return, if there
  // are no other references. This method is *not* called if the client
  // disconnects after the call to Finish.
  virtual void HandleError() = 0;

  // Called when a Write completes successfully.
  virtual void HandleWriteDone() { }

  // Attempt to initiate a gRPC client session by calling the appropriate
  // RequestXXXRpcMethodName on the AsyncService object. In the SampleService
  // example above, this would call service->RequestSaveConfiguration(...).
  // "tag" should be passed directly to the gRPC API and will be initialised to
  // a callback that will invoke the success/failure handlers in this class.
  virtual void InitResponder(AsyncService* service, ::grpc::ServerContext* ctx,
                             ReaderWriterT* responder,
                             ::grpc::ServerCompletionQueue* cq,
                             void* tag) = 0;

  // Create a new one of "this", ie: Just chain to the constructor.
  // This is boilerplate because RpcHandler can't directly call the
  // constructor of the implmenting class.
  virtual RpcHandler* CreateHandler(AsyncService* service,
                                    ::grpc::ServerCompletionQueue* cq) = 0;

  // Internal callback handlers. These all expect to be passed a RefPtrT to
  // ensure that "this" can't be deleted while gRPC operations are still
  // pending.
  void InitDone(RefPtrT r);
  void AttemptRead(RefPtrT r);
  void ReadDone(RefPtrT r);
  void FinishDone(RefPtrT r);
  void WriteDone(RefPtrT r);
  void CallHandleError(RefPtrT r);

  // Is the current state compatible with making a call to Write or Finish
  // (Not on the public API since write_outstanding_ makes this confusing).
  bool IsClientWriteable() const;

  AsyncService* service_;
  ::grpc::ServerCompletionQueue* cq_;
  ::grpc::ServerContext ctx_;
  ReaderWriterT responder_;

  // We pretty much always have a Read() request outstanding and we need a
  // place to store the result.
  RequestT request_;

  State state_;
  bool write_outstanding_;

  DISALLOW_COPY_AND_ASSIGN(RpcHandler);
};

template <typename AsyncService, typename RequestT, typename ResponseT>
RpcHandler<AsyncService, RequestT, ResponseT>::RpcHandler(
    AsyncService* service, ::grpc::ServerCompletionQueue* cq)
    : service_(service),
      cq_(cq),
      responder_(&ctx_),
      state_(INIT),
      write_outstanding_(false) {
  // It would be nice to put the contents of Start() in this function,
  // but InitResponder is pure virtual at this point, so we can't downcall
  // into it.
}

template <typename AsyncService, typename RequestT, typename ResponseT>
void RpcHandler<AsyncService, RequestT, ResponseT>::Start() {
  // RequestFoo should only fail if the service or queue have been shutdown.
  // So we just hook this up to CallHandleError which will silently destroy
  // "this" without calling HandleError(), because state_ is INIT.
  InitResponder(service_, &ctx_, &responder_, cq_,
                MakeFunction(this, &RpcHandler::InitDone,
                             &RpcHandler::CallHandleError, RefPtrT(this)));
}

template <typename AsyncService, typename RequestT, typename ResponseT>
void RpcHandler<AsyncService, RequestT, ResponseT>::InitDone(RefPtrT ref) {
  CreateHandler(service_, cq_)->Start();  // Cleans up after itself.

  if (state_ != FINISHED) {
    state_ = WAITING_FOR_FIRST_READ;
    // It's now safe for our subclass to invoke Write() or Finish(). At some
    // point in the future we could implement a callback to signal that.
    AttemptRead(ref);
  } else {
    // When state is FINISHED, it shouldn't be possible for anything else to be
    // holding a ref to "this" right now, so we rely on the refcount to actually
    // force a disconnect. If somehow that's not true, we'd need to actually
    // wire up a call to responder_.Finish(). It would be nice to:
    // DCHECK(ref->HasOneRef());
    // but there are at least 2 copies of the RefPtr (one here, one in the
    // Function).
    // Note that the status from our Finish() call does not make it to the
    // client in this case.
  }
}

template <typename AsyncService, typename RequestT, typename ResponseT>
void RpcHandler<AsyncService, RequestT, ResponseT>::AttemptRead(RefPtrT ref) {
  if (state_ != WAITING_FOR_FIRST_READ) {
    DCHECK_EQ(state_, RUNNING);
  }

  if (state_ != FINISHED) {
    responder_.Read(&request_, MakeFunction(this, &RpcHandler::ReadDone,
                                            &RpcHandler::CallHandleError, ref));
  }
}

template <typename AsyncService, typename RequestT, typename ResponseT>
void RpcHandler<AsyncService, RequestT, ResponseT>::ReadDone(RefPtrT ref) {
  if (state_ == WAITING_FOR_FIRST_READ) {
    state_ = RUNNING;
  }

  HandleRequest(request_);
  request_.Clear();  // Save a little memory.

  if (state_ != FINISHED) {
    AttemptRead(ref);
  }
}

template <typename AsyncService, typename RequestT, typename ResponseT>
bool RpcHandler<AsyncService, RequestT, ResponseT>::Finish(
    const ::grpc::Status& status) {
  if (state_ == FINISHED) {
    return false;
  }
  if (IsClientWriteable()) {
    responder_.Finish(
        status, MakeFunction(this, &RpcHandler::FinishDone,
                             &RpcHandler::CallHandleError, RefPtrT(this)));
  }
  state_ = FINISHED;
  return true;
}

template <typename AsyncService, typename RequestT, typename ResponseT>
bool RpcHandler<AsyncService, RequestT, ResponseT>::Write(
    const ResponseT& response) {
  if (IsClientWriteable() && !write_outstanding_) {
    write_outstanding_ = true;
    responder_.Write(response,
                     MakeFunction(this, &RpcHandler::WriteDone,
                                  &RpcHandler::CallHandleError, RefPtrT(this)));
    return true;
  } else {
    return false;
  }
}

template <typename AsyncService, typename RequestT, typename ResponseT>
void RpcHandler<AsyncService, RequestT, ResponseT>::WriteDone(RefPtrT ref) {
  write_outstanding_ = false;
  HandleWriteDone();
}

template <typename AsyncService, typename RequestT, typename ResponseT>
void RpcHandler<AsyncService, RequestT, ResponseT>::FinishDone(RefPtrT ref) {
  DCHECK_EQ(state_, FINISHED);
  // This discards ref, which may free "this".
}

template <typename AsyncService, typename RequestT, typename ResponseT>
void RpcHandler<AsyncService, RequestT, ResponseT>::CallHandleError(
    RefPtrT ref) {
  // If an error occurs before we've read a message or after a call to
  // Finish, we don't report it downwards. Note that HandleError is
  // also called indirectly via CallHandleErrorAsync.
  if (state_ == RUNNING) {
    HandleError();
  }
  state_ = FINISHED;
  // This discards ref, which may free "this".
}

template <typename AsyncService, typename RequestT, typename ResponseT>
bool RpcHandler<AsyncService, RequestT, ResponseT>::IsClientWriteable() const {
  return state_ == WAITING_FOR_FIRST_READ || state_ == RUNNING;
}

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_RPC_HANDLER_H_
