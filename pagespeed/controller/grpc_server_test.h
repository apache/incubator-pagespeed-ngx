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

#ifndef PAGESPEED_CONTROLLER_GRPC_SERVER_TEST_H_
#define PAGESPEED_CONTROLLER_GRPC_SERVER_TEST_H_

#include <memory>

#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/grpc.h"

namespace net_instaweb {

// Fixture for testing gRPC servers. Contains an event dispatcher thread, plus
// some other bits you'll need.
class GrpcServerTest : public testing::Test {
 public:
  GrpcServerTest();
  virtual ~GrpcServerTest();

  // Binds server_ to the address returned by ListenAddress().
  void SetUp() override;

  // Blocks until server_ is stopped.
  void StopServer();

  // Have the server thread call Run() (never Cancel()) on the supplied
  // function, ASAP. This call is thread-safe.
  void QueueFunctionForServerThread(Function* func);

  GoogleString ServerAddress() const;

 protected:
  // Holder for various pieces of a client connection.
  class BaseClientConnection {
   public:
    BaseClientConnection(const GoogleString& address)
        : channel_(::grpc::CreateChannel(
              address, ::grpc::InsecureChannelCredentials())) {
    }

    ::grpc::ClientContext client_ctx_;
    std::shared_ptr<::grpc::Channel> channel_;
  };

  std::unique_ptr<ThreadSystem> thread_system_;
  std::unique_ptr<::grpc::ServerCompletionQueue> queue_;

 private:
  class GrpcServerThread : public ThreadSystem::Thread {
   public:
    GrpcServerThread(::grpc::CompletionQueue* queue,
                     ThreadSystem* thread_system);
    virtual ~GrpcServerThread();
    void Stop();

   private:
    void Run() override;

    ::grpc::CompletionQueue* queue_;
  };

  // Override this to call RegisterService on the ServerBuilder.
  virtual void RegisterServices(::grpc::ServerBuilder*) = 0;

  std::unique_ptr<GrpcServerThread> server_thread_;
  std::unique_ptr<::grpc::Server> server_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_GRPC_SERVER_TEST_H_
