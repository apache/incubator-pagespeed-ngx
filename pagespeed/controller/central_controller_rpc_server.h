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
#include "pagespeed/kernel/util/grpc.h"
#include "pagespeed/system/controller_process.h"

namespace net_instaweb {

// ControllerProcess implementation that starts a gRPC server which handles
// CentralController operations.

class CentralControllerRpcServer : public ControllerProcess {
 public:
  CentralControllerRpcServer(int listen_port);
  virtual ~CentralControllerRpcServer() { }

  // ControllerProcess implementation.
  int Setup() override;
  int Run() override;
  void Stop() override;

 private:
  const int listen_port_;
  std::unique_ptr<::grpc::Server> server_;
  std::unique_ptr<::grpc::ServerCompletionQueue> queue_;
  grpc::CentralControllerRpcService::AsyncService service_;

  DISALLOW_COPY_AND_ASSIGN(CentralControllerRpcServer);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_RPC_SERVER_H_
