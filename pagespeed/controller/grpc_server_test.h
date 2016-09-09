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

#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <grpc++/alarm.h>

#include <memory>

#include "base/logging.h"
#include "pagespeed/controller/central_controller_rpc_server.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/grpc.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

// Fixture for testing gRPC servers. Contains an event dispatcher thread, plus
// some other bits you'll need.
class GrpcServerTest : public testing::Test {
 public:
  GrpcServerTest() : thread_system_(Platform::CreateThreadSystem()) {
  }

  virtual ~GrpcServerTest() {
    StopServer();
  }

  void SetUp() override {
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(ServerAddress(),
                             ::grpc::InsecureServerCredentials(),
                             &listen_port_);
    queue_ = builder.AddCompletionQueue();
    RegisterServices(&builder);
    server_ = builder.BuildAndStart();
    CHECK(server_.get() != nullptr);
    // listen_port_ may have been 0 first time through. gRPC sets -1 on failure.
    CHECK_GT(listen_port_, 0);

    server_thread_.reset(
        new GrpcServerThread(queue_.get(), thread_system_.get()));
    CHECK(server_thread_->Start());
  }

  // Blocks until shutdown is complete.
  void StopServer() {
    if (server_thread_ != nullptr) {
      server_->Shutdown();
      server_thread_->Stop();
      server_thread_.reset();
      server_.reset();
    }
  }

  // Have the server thread call Run() (never Cancel()) on the supplied
  // function, ASAP. This call is thread-safe.
  void QueueFunctionForServerThread(Function* func) {
    // Function to schedule an alarm that invokes CallRun ASAP on a supplied
    // function.
    class DelayedCallFunction : public Function {
     public:
      DelayedCallFunction(ThreadSystem* thread_system,
                          ::grpc::CompletionQueue* queue, Function* function)
          : mutex_(thread_system->NewMutex()), func_(function) {
        // The annotations don't require it, but it's important that we take
        // a lock before creating the alarm; We can immediately get a callback
        // on the other thread, before alarm_ is properly populated.
        ScopedMutex lock(mutex_.get());
        alarm_.reset(
            new ::grpc::Alarm(queue, gpr_inf_past(GPR_CLOCK_MONOTONIC), this));
      }
      virtual ~DelayedCallFunction() { }

      void Run() override {
        ScopedMutex lock(mutex_.get());
        func_->CallRun();
      }

      void Cancel() override {
        ScopedMutex lock(mutex_.get());
        func_->CallRun();
      }

     private:
      std::unique_ptr<AbstractMutex> mutex_;
      Function* func_ GUARDED_BY(mutex_);
      std::unique_ptr<::grpc::Alarm> alarm_ GUARDED_BY(mutex_);

      DISALLOW_COPY_AND_ASSIGN(DelayedCallFunction);
    };

    // Schedule the call. This will clean itself up.
    new DelayedCallFunction(thread_system_.get(), queue_.get(), func);
  }

  static void SetUpTestCase() {
    // When this is 0, gRPC will pick an unused port for us.
    listen_port_ = 0;
  }

  GoogleString ServerAddress() const {
    return StrCat("localhost:", IntegerToString(listen_port_));
  }

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
                     ThreadSystem* thread_system)
        : Thread(thread_system, "grpc_test_server", ThreadSystem::kJoinable),
          queue_(queue) {}
    virtual ~GrpcServerThread() {}

    void Stop() {
      queue_->Shutdown();
      this->Join();
    }

   private:
    void Run() override {
      CentralControllerRpcServer::MainLoop(queue_);
    }

    ::grpc::CompletionQueue* queue_;
  };

  virtual void RegisterServices(::grpc::ServerBuilder*) = 0;

  static int listen_port_;
  std::unique_ptr<GrpcServerThread> server_thread_;
  std::unique_ptr<::grpc::Server> server_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_GRPC_SERVER_TEST_H_
