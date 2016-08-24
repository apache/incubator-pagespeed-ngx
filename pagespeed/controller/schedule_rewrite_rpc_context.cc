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

#include "pagespeed/controller/schedule_rewrite_rpc_context.h"

#include <memory>

#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/controller/controller.pb.h"
#include "pagespeed/controller/schedule_rewrite_callback.h"
#include "pagespeed/controller/request_result_rpc_client.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/grpc.h"

namespace net_instaweb {

class ScheduleRewriteRpcContext::ScheduleRewriteRequestResultRpcClient
    : public RequestResultRpcClient<ScheduleRewriteRequest,
                                    ScheduleRewriteResponse,
                                    ScheduleRewriteCallback> {
 public:
  ScheduleRewriteRequestResultRpcClient(
      const GoogleString& key,
      grpc::CentralControllerRpcService::StubInterface* stub,
      ::grpc::CompletionQueue* queue, ThreadSystem* thread_system,
      MessageHandler* handler, ScheduleRewriteCallback* callback)
      : RequestResultRpcClient(queue, thread_system, handler, callback),
        key_(key) {
    // Nothing will happen until a call to Start() is made. We don't do it here
    // because the wrapper needs to call SetTransactionContext first.
  }

  std::unique_ptr<RequestResultRpcClient::ReaderWriter> StartRpc(
      grpc::CentralControllerRpcService::StubInterface* stub,
      ::grpc::ClientContext* context, ::grpc::CompletionQueue* queue,
      void* tag) override {
    return stub->AsyncScheduleRewrite(context, queue, tag);
  }

  ~ScheduleRewriteRequestResultRpcClient() {
    MarkSucceeded();
  }

  void MarkFailed() {
    ScheduleRewriteRequest req;
    req.set_status(ScheduleRewriteRequest::FAILED);
    SendResultToServer(req);
  }

  void MarkSucceeded() {
    ScheduleRewriteRequest req;
    req.set_status(ScheduleRewriteRequest::SUCCESS);
    SendResultToServer(req);
  }

 private:
  void PopulateServerRequest(ScheduleRewriteRequest* request) override {
    request->set_key(key_);
  }

  const GoogleString key_;
};

ScheduleRewriteRpcContext::ScheduleRewriteRpcContext(
    grpc::CentralControllerRpcService::StubInterface* stub,
    ::grpc::CompletionQueue* queue, ThreadSystem* thread_system,
    MessageHandler* handler, ScheduleRewriteCallback* callback)
    : client_(new ScheduleRewriteRequestResultRpcClient(
          callback->key(), stub, queue, thread_system, handler, callback)) {
  // SetTransactionContext takes ownership of "this".
  callback->SetTransactionContext(this);
  client_->Start(stub);
}

void ScheduleRewriteRpcContext::MarkSucceeded() { client_->MarkSucceeded(); }
void ScheduleRewriteRpcContext::MarkFailed() { client_->MarkFailed(); }

}  // namespace net_instaweb
