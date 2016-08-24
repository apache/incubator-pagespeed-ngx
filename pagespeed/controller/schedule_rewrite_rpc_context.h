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

#ifndef PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_RPC_CONTEXT_H_
#define PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_RPC_CONTEXT_H_

#include <memory>

#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/controller/schedule_rewrite_callback.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// ScheduleRewriteContext implementation for use with gRPC.
// All of the interesting code is in ScheduleRewriteRequestResultRpcClient
// (in the .cc file), this is just a wrapper to adapt it onto
// ScheduleRewriteContext.

class ScheduleRewriteRpcContext : public ScheduleRewriteContext {
 public:
  ScheduleRewriteRpcContext(
      grpc::CentralControllerRpcService::StubInterface* stub,
      ::grpc::CompletionQueue* queue, ThreadSystem* thread_system,
      MessageHandler* handler, ScheduleRewriteCallback* callback);

  void MarkSucceeded() override;
  void MarkFailed() override;

 private:
  class ScheduleRewriteRequestResultRpcClient;

  // This is a pointer simply so we can elide the implementation of the
  // client.
  std::unique_ptr<ScheduleRewriteRequestResultRpcClient> client_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_SCHEDULE_REWRITE_RPC_CONTEXT_H_
