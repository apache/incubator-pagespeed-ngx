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

#include "pagespeed/controller/central_controller_rpc_client.h"

#include "pagespeed/controller/central_controller_rpc_server.h"
#include "pagespeed/controller/context_registry.h"
#include "pagespeed/controller/expensive_operation_rpc_context.h"
#include "pagespeed/controller/schedule_rewrite_rpc_context.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {

class GrpcClientThread : public ThreadSystem::Thread {
 public:
  GrpcClientThread(::grpc::CompletionQueue* queue, ThreadSystem* thread_system)
      : Thread(thread_system, "central_controller_client",
               ThreadSystem::kJoinable),
        queue_(queue) {}

  ~GrpcClientThread() override {
    queue_->Shutdown();
    this->Join();
  }

 private:
  void Run() override { CentralControllerRpcServer::MainLoop(queue_); }

  ::grpc::CompletionQueue* queue_;

  DISALLOW_COPY_AND_ASSIGN(GrpcClientThread);
};

}  // namespace

// Adapt ContextRegistry to ClientContext::GlobalCallbacks, which are a
// convenient way of hooking up the Regitry.
class CentralControllerRpcClient::ClientRegistry
    : public ::grpc::ClientContext::GlobalCallbacks {
 public:
  explicit ClientRegistry(ThreadSystem* thread_system)
      : registry_(thread_system) {}

  // Called whenever a ClientContext is created.
  void DefaultConstructor(::grpc::ClientContext* context) override {
    CHECK(registry_.TryRegisterContext(context));
  }

  // Called whenever a ClientContext is destroyed.
  void Destructor(::grpc::ClientContext* context) override {
    registry_.RemoveContext(context);
  }

  void CancelAllActive() {
    registry_.CancelAllActive();
  }

 private:
  ContextRegistry<::grpc::ClientContext> registry_;
};

CentralControllerRpcClient::CentralControllerRpcClient(
    const GoogleString& server_address, ThreadSystem* thread_system,
    MessageHandler* handler)
    : thread_system_(thread_system),
      mutex_(thread_system_->NewMutex()),
      clients_(new ClientRegistry(thread_system_)),
      handler_(handler),
      channel_(::grpc::CreateChannel(server_address,
                                     ::grpc::InsecureChannelCredentials())),
      stub_(grpc::CentralControllerRpcService::NewStub(channel_)),
      client_thread_(new GrpcClientThread(&queue_, thread_system)) {
  ::grpc::ClientContext::SetGlobalCallbacks(clients_.get());
  CHECK(client_thread_->Start());
}

CentralControllerRpcClient::~CentralControllerRpcClient() {
  Shutdown();
  // It's not possible to clear the GlobalCallbacks here, but since Shutdown
  // prevents further clients from registering, that ought to be OK.
}

void CentralControllerRpcClient::Shutdown() {
  std::unique_ptr<ThreadSystem::Thread> thread;
  {
    ScopedMutex lock(mutex_.get());
    // Controller operations check that client_thread_ is not null, so this will
    // prevent any further operations from being started.
    thread = std::move(client_thread_);
  }

  // Calls TryCancel() on all outstanding requests and waits for those requests
  // to drain.
  clients_->CancelAllActive();

  // thread goes out of scope and is shutdown.
}

void CentralControllerRpcClient::ScheduleExpensiveOperation(
    ExpensiveOperationCallback* callback) {
  ScopedMutex lock(mutex_.get());
  if (client_thread_ != nullptr) {
    // Starts the transaction and deletes itself when done.
    new ExpensiveOperationRpcContext(stub_.get(), &queue_, thread_system_,
                                     handler_, callback);
  } else {
    callback->CallCancel();
  }
}

void CentralControllerRpcClient::ScheduleRewrite(ScheduleRewriteCallback*
                                                 callback) {
  ScopedMutex lock(mutex_.get());
  if (client_thread_ != nullptr) {
    // Starts the transaction and deletes itself when done.
    new ScheduleRewriteRpcContext(stub_.get(), &queue_, thread_system_,
                                  handler_, callback);
  } else {
    callback->CallCancel();
  }
}

}  // namespace net_instaweb
