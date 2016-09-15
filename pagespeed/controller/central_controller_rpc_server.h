#ifndef PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_RPC_SERVER_H_
#define PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_RPC_SERVER_H_

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

#include <memory>

#include "base/macros.h"
#include "pagespeed/controller/controller.grpc.pb.h"
#include "pagespeed/controller/expensive_operation_controller.h"
#include "pagespeed/controller/schedule_rewrite_controller.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/util/grpc.h"
#include "pagespeed/system/controller_process.h"

namespace net_instaweb {

// ControllerProcess implementation that starts a gRPC server which handles
// CentralController operations.

class CentralControllerRpcServer : public ControllerProcess {
 public:
  // listen_address is passed directly into ServerBuilder::AddListeningPort,
  // so theoretically it can be anything gRPC supports. In practice we expect
  // it to be either "localhost:<port>" or "unix:<path>". This takes ownership
  // of rewrite_controller.
  CentralControllerRpcServer(
      const GoogleString& listen_address,
      ExpensiveOperationController* expensive_operation_controller,
      ScheduleRewriteController* rewrite_controller,
      MessageHandler* handler);
  virtual ~CentralControllerRpcServer() { }

  // ControllerProcess implementation.
  int Setup() override;
  int Run() override;
  void Stop() override;

  // Pulled out into a static function so it can also be used in tests.
  static void MainLoop(::grpc::CompletionQueue* queue);

 private:
  const GoogleString listen_address_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<::grpc::ServerCompletionQueue> queue_;
  grpc::CentralControllerRpcService::AsyncService service_;

  std::unique_ptr<ExpensiveOperationController> expensive_operation_controller_;
  std::unique_ptr<ScheduleRewriteController> rewrite_controller_;
  MessageHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(CentralControllerRpcServer);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_RPC_SERVER_H_
