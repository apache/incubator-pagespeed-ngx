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

#include <utility>

#include "base/logging.h"
#include "pagespeed/controller/central_controller_rpc_server.h"
#include "pagespeed/controller/context_registry.h"
#include "pagespeed/controller/expensive_operation_rpc_context.h"
#include "pagespeed/controller/schedule_rewrite_rpc_context.h"
#include "pagespeed/kernel/base/thread.h"

namespace net_instaweb {

// We might log if this rolls over, so don't be too aggressive.
const int CentralControllerRpcClient::kControllerReconnectDelayMs =
    5 * Timer::kMinuteMs;
const char CentralControllerRpcClient::kControllerReconnectTimeStatistic[] =
    "central-controller-reconnect-time-ms";

class CentralControllerRpcClient::GrpcClientThread
    : public ThreadSystem::Thread {
 public:
  explicit GrpcClientThread(ThreadSystem* thread_system)
      : Thread(thread_system, "central_controller_client",
               ThreadSystem::kJoinable) {}

  ~GrpcClientThread() override {
    queue_.Shutdown();
    if (this->Started()) {
      this->Join();
    }
  }

  ::grpc::CompletionQueue* queue() { return &queue_; }

 private:
  void Run() override { CentralControllerRpcServer::MainLoop(&queue_); }

  ::grpc::CompletionQueue queue_;

  DISALLOW_COPY_AND_ASSIGN(GrpcClientThread);
};

// Adapt ContextRegistry to ClientContext::GlobalCallbacks, which are a
// convenient way of hooking up the Regitry.
class CentralControllerRpcClient::ClientRegistry
    : public ::grpc::ClientContext::GlobalCallbacks {
 public:
  explicit ClientRegistry(ThreadSystem* thread_system)
      : thread_system_(thread_system),
        registry_(new ContextRegistry<::grpc::ClientContext>(thread_system)) {}

  // Called whenever a ClientContext is created.
  void DefaultConstructor(::grpc::ClientContext* context) override {
    CHECK(registry_->TryRegisterContext(context));
  }

  // Called whenever a ClientContext is destroyed.
  void Destructor(::grpc::ClientContext* context) override {
    registry_->RemoveContext(context);
  }

  void CancelAllActive() { registry_->CancelAllActive(); }
  void CancelAllActiveAndWait() { registry_->CancelAllActiveAndWait(); }

  // When re-connecting we need to "un-shutdown" the registry. We can't call
  // SetGlobalCallbacks() more than once and replace the pointer, so instead we
  // swap out the inner pointer. Bad things will happen if there are things
  // in the registry when this is called, so it's important that something
  // external is preventing clients from starting while you call this
  // (in this case it's state_ != RUNNING).
  void ReviveAfterShutdown() {
    std::unique_ptr<ContextRegistry<::grpc::ClientContext>> registry(
        new ContextRegistry<::grpc::ClientContext>(thread_system_));
    registry_.swap(registry);
    CHECK(registry->Empty());
  }

  // These are also not locked, so if you plan to rely on the result you need
  // some external assurance that the value hasn't changed in a way that
  // matters.
  int Size() const { return registry_->Size(); }
  bool Empty() const { return registry_->Empty(); }

 private:
  ThreadSystem* thread_system_;
  std::unique_ptr<ContextRegistry<::grpc::ClientContext>> registry_;
};

CentralControllerRpcClient::CentralControllerRpcClient(
    const GoogleString& server_address, int max_outstanding_requests,
    ThreadSystem* thread_system, Timer* timer, Statistics* statistics,
    MessageHandler* handler)
    : thread_system_(thread_system),
      timer_(timer),
      mutex_(thread_system_->NewMutex()),
      clients_(new ClientRegistry(thread_system_)),
      handler_(handler),
      state_(DISCONNECTED),
      // Fudge max_outstanding_requests a bit, just in case we're
      // single-process. We'd rather not panic unnecessarily.
      controller_panic_threshold_(max_outstanding_requests + 10),
      reconnect_time_ms_(0),
      reconnect_time_ms_statistic_(
          statistics->GetUpDownCounter(kControllerReconnectTimeStatistic)),
      channel_(::grpc::CreateChannel(server_address,
                                     ::grpc::InsecureChannelCredentials())),
      stub_(grpc::CentralControllerRpcService::NewStub(channel_)) {
  ::grpc::ClientContext::SetGlobalCallbacks(clients_.get());
  {
    ScopedMutex lock(mutex_.get());
    ConsiderConnecting(timer_->NowMs());
    // If someone already detected that the controller is stalled, we may not
    // connect and be left DISCONNECTED without a client_thread_.
  }
}

CentralControllerRpcClient::~CentralControllerRpcClient() {
  ShutDown();
  // It's not possible to clear the GlobalCallbacks here, but since
  // ShutDown prevents further clients from registering, that ought to
  // be OK.
  // TODO(cheesy): A better solution would be to install a static object for the
  // handler. It could call back into "this" only while it has a non-null
  // pointer, which would be cleared right here. The above remains true, though.
}

void CentralControllerRpcClient::InitStats(Statistics* statistics) {
  statistics->AddUpDownCounter(kControllerReconnectTimeStatistic);
}

void CentralControllerRpcClient::ShutDown() {
  {
    ScopedMutex lock(mutex_.get());
    if (state_ == SHUTDOWN) {
      return;
    }

    // This will reject all further requests.
    state_ = SHUTDOWN;
  }
  clients_->CancelAllActiveAndWait();
  {
    ScopedMutex lock(mutex_.get());
    CHECK_EQ(state_, SHUTDOWN);
    client_thread_.reset();
  }
}

bool CentralControllerRpcClient::TimestampsAllowConnection(int64 now_ms) {
  // At server startup, both the local variable and the statistic will be 0.
  // At process startup, the statistic will be non-zero if someone else detected
  // that the controller is not responding. In the event of a local problem,
  // the local-only reconnect_time_ms_ is advanced. If any worker detects the
  // controller is in trouble, they advance the statistic. For a connection to
  // be allowed, now must be >= both the local timestamp and the statistic.

  // Local reconnect_time_ms_ takes precedence over the statistic. Only once
  // that has expired do we consult the statistic.
  if (now_ms >= reconnect_time_ms_) {
    // Cache the statistic, because why not?
    reconnect_time_ms_ = reconnect_time_ms_statistic_->Get();
  }
  return now_ms >= reconnect_time_ms_;
}

void CentralControllerRpcClient::ConsiderConnecting(
    int64 now_ms) /* EXCLUSIVE_LOCKS_REQUIRED(mutex_) */ {
  // Reconnect if the time to connect has passed and we're not connected or
  // shutdown.
  if (state_ == DISCONNECTED && TimestampsAllowConnection(now_ms)) {
    if (!clients_->Empty()) {
      // We expect that there should be no clients in the registry now!
      // Clients can only be added to the registry when state_ is RUNNING and
      // we currently have a lock on state_. So, nothing can be added until we
      // return.
#ifndef NDEBUG
      handler_->Message(kError, "clients_ not empty for reconnect!");
#endif
      reconnect_time_ms_ = now_ms + 5 * Timer::kSecondMs;
      return;
    }

    std::unique_ptr<GrpcClientThread> thread(
        new GrpcClientThread(thread_system_));
    // We check fail if the thread fails to start at startup, but that's
    // probably not OK here. This should rarely fail.
    if (thread->Start()) {
      clients_->ReviveAfterShutdown();
      client_thread_ = std::move(thread);
      state_ = RUNNING;
    } else {
      handler_->Message(
          kError, "Couldn't start thread for talking to the controller!");
      // This is the local variable, not the statistic. We don't want to
      // force everyone to reconnect just because we had trouble starting a
      // thread.
      reconnect_time_ms_ = now_ms + kControllerReconnectDelayMs;
    }
  }
}

template <typename ContextT, typename CallbackT>
void CentralControllerRpcClient::StartContext(CallbackT* callback) {
  bool shutdown_required = false;
  int64 now_ms = timer_->NowMs();
  {
    ScopedMutex lock(mutex_.get());
    ConsiderConnecting(now_ms);
    if (state_ == RUNNING) {
      CHECK(client_thread_ != nullptr);
      if (!TimestampsAllowConnection(now_ms)) {
        // Someone else (another thread or process) detected that the
        // controller is not responding. Kill the client thread.
        shutdown_required = true;
      } else if (clients_->Size() > controller_panic_threshold_) {
        // We've accumulated a crazy number of gRPC clients in the registry.
        // It looks like the controller isn't responding and we're just piling
        // up detached RewriteDrivers.
        handler_->Message(
            kError,
            "The central controller isn't responding, "
            "stopping image rewrites for %d seconds.",
            static_cast<int>(kControllerReconnectDelayMs / Timer::kSecondMs));
        // Tell everyone else to stop talking to the controller, too.
        reconnect_time_ms_statistic_->Set(now_ms + kControllerReconnectDelayMs);
        shutdown_required = true;
      } else {
        // Starts the transaction and deletes itself when done.
        new ContextT(stub_.get(), client_thread_->queue(), thread_system_,
                     handler_, callback);
        return;  // Do not fall through, as callback will be canceled!
      }

      if (shutdown_required) {
        // Stop further requests. We must do this before releasing the lock.
        state_ = DISCONNECTED;
      }
    }
  }

  // Someone noticed that the controller is in trouble, so flush all outstanding
  // requests to it.
  if (shutdown_required) {
    // We can't use CancelAllActiveAndWait here, because the thread calling
    // us is the same one that needs to process the cancellations. However,
    // nothing should be *added* to clients_ because because state_ was set to
    // DISCONNECTED above.
    clients_->CancelAllActive();
  }
  callback->CallCancel();
}

void CentralControllerRpcClient::ScheduleExpensiveOperation(
    ExpensiveOperationCallback* callback) {
  StartContext<ExpensiveOperationRpcContext>(callback);
}

void CentralControllerRpcClient::ScheduleRewrite(
    ScheduleRewriteCallback* callback) {
  StartContext<ScheduleRewriteRpcContext>(callback);
}

}  // namespace net_instaweb
