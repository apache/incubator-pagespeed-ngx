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
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/util/grpc.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// CentralController implementation that forwards all requests to a gRPC server.
// RewriteDrivers wait for the controller response (possibly detaching) before
// proceeding to rewrite. If the controller stops responding but requests keep
// coming in, we could keep creating RewriteDrivers indefinitely and eat all
// available memory. To guard against this we looks at the number of outstanding
// gRPC requests. If that ever exceeds the max possible number, we declare the
// controller to have hung, cancel all outstanding requests and stop talking to
// it. We signal this via a statistic, so all processes can notice and do the
// same.

class CentralControllerRpcClient : public CentralController {
 public:
  static const char kControllerReconnectTimeStatistic[];
  static const int kControllerReconnectDelayMs;

  CentralControllerRpcClient(const GoogleString& server_address,
                             int panic_threshold, ThreadSystem* thread_system,
                             Timer* timer, Statistics* statistics,
                             MessageHandler* handler);
  virtual ~CentralControllerRpcClient();

  // CentralController implementation.
  void ScheduleExpensiveOperation(
      ExpensiveOperationCallback* callback) override;
  void ScheduleRewrite(ScheduleRewriteCallback* callback) override;

  void ShutDown() override LOCKS_EXCLUDED(mutex_);

  static void InitStats(Statistics* stats);

 private:
  enum State {
    DISCONNECTED,  // Temporary shutdown, can be revived.
    RUNNING,
    SHUTDOWN,  // Permanent shutdown.
  };

  class GrpcClientThread;
  class ClientRegistry;

  // Common code that checks shutdown status before creating a new ContextT.
  template <typename ContextT, typename CallbackT>
  void StartContext(CallbackT* callback) LOCKS_EXCLUDED(mutex_);

  // If we're not connected and kControllerReconnectDelay has passed, attempt
  // to reconnect.
  void ConsiderConnecting(int64 now_ms) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Have we passed reconnect_time_ms_ and reconnect_time_ms_statistic_?
  bool TimestampsAllowConnection(int64 now) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  ThreadSystem* thread_system_;
  Timer* timer_;
  std::unique_ptr<AbstractMutex> mutex_;
  // TODO(cheesy): This should probably by guarded by mutex_ but I'm worried
  // about deadlock in Shutdown() while the clients are draining. It *might*
  // be OK because the lock you absolutely cannot hold is the one in the
  // registry and that does get released.
  std::unique_ptr<ClientRegistry> clients_;
  MessageHandler* handler_;

  State state_ GUARDED_BY(mutex_);
  const int controller_panic_threshold_;
  int64 reconnect_time_ms_ GUARDED_BY(mutex_);
  UpDownCounter* reconnect_time_ms_statistic_;

  std::unique_ptr<::grpc::CompletionQueue> queue_;
  std::shared_ptr<::grpc::ChannelInterface> channel_;
  std::unique_ptr<grpc::CentralControllerRpcService::Stub> stub_;

  // This must be last so that it's destructed first.
  std::unique_ptr<GrpcClientThread> client_thread_ GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(CentralControllerRpcClient);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_CENTRAL_CONTROLLER_RPC_CLIENT_H_
