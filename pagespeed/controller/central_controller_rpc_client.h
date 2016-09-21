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

#ifndef PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_RPC_CLIENT_H_
#define PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_RPC_CLIENT_H_

#include <memory>
#include <unordered_set>

#include "base/macros.h"
#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/controller/central_controller.h"
#include "pagespeed/controller/expensive_operation_callback.h"
#include "pagespeed/controller/schedule_rewrite_callback.h"
#include "pagespeed/kernel/util/grpc.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// CentralController implementation that forwards all requests to a gRPC server.

class CentralControllerRpcClient : public CentralController {
 public:
  CentralControllerRpcClient(const GoogleString& server_address,
                             ThreadSystem* thread_system,
                             MessageHandler* handler);
  virtual ~CentralControllerRpcClient();

  // Shut down the request handling thread.
  void Shutdown();

  // CentralController implementation.
  void ScheduleExpensiveOperation(
      ExpensiveOperationCallback* callback) override;
  void ScheduleRewrite(ScheduleRewriteCallback* callback) override;

 private:
  class ClientRegistry;

  ThreadSystem* thread_system_;
  std::unique_ptr<AbstractMutex> mutex_;
  std::unique_ptr<ClientRegistry> clients_;
  MessageHandler* handler_;

  ::grpc::CompletionQueue queue_;
  std::shared_ptr<::grpc::ChannelInterface> channel_;
  std::unique_ptr<grpc::CentralControllerRpcService::Stub> stub_;

  // This must be last so that it's destructed first.
  std::unique_ptr<ThreadSystem::Thread> client_thread_ GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(CentralControllerRpcClient);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_RPC_CLIENT_H_
