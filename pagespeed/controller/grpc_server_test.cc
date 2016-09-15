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

#include "pagespeed/controller/grpc_server_test.h"

#include <sys/stat.h>
#include <memory>

#include "base/logging.h"
#include "pagespeed/controller/central_controller_rpc_server.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/grpc.h"
#include "pagespeed/kernel/util/platform.h"

#include <grpc++/alarm.h>

namespace net_instaweb {

// Fixture for testing gRPC servers. Contains an event dispatcher thread, plus
// some other bits you'll need.
GrpcServerTest::GrpcServerTest()
    : thread_system_(Platform::CreateThreadSystem()) {}

GrpcServerTest::~GrpcServerTest() { StopServer(); }

void GrpcServerTest::SetUp() {
  // This doesn't seem to be created automatically!
  mkdir(GTestTempDir().c_str(), 0755);

  ::grpc::ServerBuilder builder;
  int bound_port = 0;
  builder.AddListeningPort(ServerAddress(), ::grpc::InsecureServerCredentials(),
                           &bound_port);
  queue_ = builder.AddCompletionQueue();
  RegisterServices(&builder);
  server_ = builder.BuildAndStart();
  CHECK(server_ != nullptr);
  // Unix sockets are 1 on success. gRPC sets -1 on failure.
  CHECK_GT(bound_port, 0);

  server_thread_.reset(
      new GrpcServerThread(queue_.get(), thread_system_.get()));
  CHECK(server_thread_->Start());
}

void GrpcServerTest::StopServer() {
  if (server_thread_ != nullptr) {
    server_->Shutdown();
    server_thread_->Stop();
    server_thread_.reset();
    server_.reset();
  }
}

// Have the server thread call Run() (never Cancel()) on the supplied
// function, ASAP. This call is thread-safe.
void GrpcServerTest::QueueFunctionForServerThread(Function* func) {
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
    ~DelayedCallFunction() override {}

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

GoogleString GrpcServerTest::ServerAddress() const {
  return StrCat("unix:", GTestTempDir(), "/grpc.sock");
}

GrpcServerTest::GrpcServerThread::GrpcServerThread(
    ::grpc::CompletionQueue* queue, ThreadSystem* thread_system)
    : Thread(thread_system, "grpc_test_server", ThreadSystem::kJoinable),
      queue_(queue) {}

GrpcServerTest::GrpcServerThread::~GrpcServerThread() {}

void GrpcServerTest::GrpcServerThread::Stop() {
  queue_->Shutdown();
  this->Join();
}

void GrpcServerTest::GrpcServerThread::Run() {
  CentralControllerRpcServer::MainLoop(queue_);
}

}  // namespace net_instaweb
