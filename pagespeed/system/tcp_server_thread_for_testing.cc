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
// Author: cheesy@google.com (cheesy@google.com)

#include "pagespeed/system/tcp_server_thread_for_testing.h"

#include <sys/socket.h>
#include <cstdlib>

#include "apr_network_io.h"
#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/system/apr_thread_compatible_pool.h"

namespace net_instaweb {

TcpServerThreadForTesting::TcpServerThreadForTesting(
    apr_port_t listen_port, StringPiece name, ThreadSystem* thread_system)
    : Thread(thread_system, name, ThreadSystem::kJoinable),
      mutex_(thread_system->NewMutex()),
      ready_notify_(mutex_->NewCondvar()),
      pool_(AprCreateThreadCompatiblePool(nullptr)),
      requested_listen_port_(listen_port),
      actual_listening_port_(0),
      listen_sock_(nullptr),
      terminating_(false),
      is_shut_down_(false) {}

void TcpServerThreadForTesting::ShutDown() {
  // We want to ensure that the thread is terminated and it has accepted exactly
  // one connection. Consider several scenarios:
  // 1. Thread was not started before destructor is called. Then the Join()
  //    below will fail as expected.
  // 2. Thread was started and our mutex-guarded block happened after creation
  //    of listen_sock_. Then we will shut down the socket so accept() in the
  //    thread will fail and the thread will close the socket and terminate.
  // 3. Thread was started and our mutex-guarded block happened before creation
  //    of listen_sock_. It's extremely unlikely race as it requires the
  //    destructor to be called right after Start(). If it ever happens, CHECK()
  //    in the thread will fail.
  {
    ScopedMutex lock(mutex_.get());
    terminating_ = true;
    if (listen_sock_) {
      apr_socket_shutdown(listen_sock_, APR_SHUTDOWN_READWRITE);
    }
  }
  this->Join();
  if (pool_ != nullptr) {
    apr_pool_destroy(pool_);
    pool_ = nullptr;
  }
  is_shut_down_ = true;
}

TcpServerThreadForTesting::~TcpServerThreadForTesting() {
  CHECK(is_shut_down_)
      << "TcpServerThreadForTesting::ShutDown() was not called";
}

// static
void TcpServerThreadForTesting::PickListenPortOnce(
    apr_port_t* static_port_number) {
  // A listen_port of 0 means the system will pick for us.
  *static_port_number = 0;
    // Looks like creating a socket and looking at its port is the easiest way
    // to find an available port.
    apr_pool_t* pool = AprCreateThreadCompatiblePool(nullptr);
    apr_socket_t* sock;
    CreateAndBindSocket(pool, &sock, static_port_number);
    apr_socket_close(sock);
    apr_pool_destroy(pool);
  CHECK_NE(*static_port_number, 0);
}

void TcpServerThreadForTesting::Run() {
  // We do not want to hold mutex during accept(), hence the local copy.
  apr_socket_t* local_listen_sock;
  {
    ScopedMutex lock(mutex_.get());
    CHECK(!terminating_);
    local_listen_sock = listen_sock_ = CreateAndBindSocket();
  }
  apr_socket_t* accepted_socket;
  apr_status_t status =
      apr_socket_accept(&accepted_socket, local_listen_sock, pool_);
  EXPECT_EQ(APR_SUCCESS, status)
      << "TcpServerThreadForTesting: "
         "apr_socket_accept failed (did not receive a connection?)";
  if (status == APR_SUCCESS) {
    HandleClientConnection(accepted_socket);
  }
  {
    ScopedMutex lock(mutex_.get());
    apr_socket_close(listen_sock_);
    listen_sock_ = nullptr;
  }
}

apr_port_t TcpServerThreadForTesting::GetListeningPort() {
  ScopedMutex lock(mutex_.get());
  while (actual_listening_port_ == 0) {
    ready_notify_->Wait();
  }
  return actual_listening_port_;
}

/* static */ void TcpServerThreadForTesting::CreateAndBindSocket(
    apr_pool_t* pool, apr_socket_t** socket, apr_port_t* port) {
  // Create TCP socket with SO_REUSEADDR.
  apr_status_t status =
      apr_socket_create(socket, APR_INET, SOCK_STREAM, APR_PROTO_TCP, pool);
  CHECK_EQ(status, APR_SUCCESS) << "CreateAndBindSocket apr_socket_create";
  status = apr_socket_opt_set(*socket, APR_SO_REUSEADDR, 1);
  CHECK_EQ(status, APR_SUCCESS) << "CreateAndBindSocket apr_socket_opt_set";

  // port may be zero, in which case apr_socket_bind will pick a port for us.
  apr_sockaddr_t* sa;
  status = apr_sockaddr_info_get(&sa, "127.0.0.1", APR_INET,
                                 *port, 0 /* flags */, pool);
  CHECK_EQ(status, APR_SUCCESS) << "CreateAndBindSocket apr_sockaddr_info_get";

  // bind and listen.
  status = apr_socket_bind(*socket, sa);
  CHECK_EQ(status, APR_SUCCESS) << "CreateAndBindSocket apr_socket_bind";
  status = apr_socket_listen(*socket, 1 /* backlog */);
  CHECK_EQ(status, APR_SUCCESS) << "CreateAndBindSocket apr_socket_listen";

  // Now the socket is bound and listening, find the local port we're actually
  // using. If requested_listen_port_ is non-zero, they really should match.
  apr_sockaddr_t* bound_sa;
  status = apr_socket_addr_get(&bound_sa, APR_LOCAL, *socket);
  CHECK_EQ(status, APR_SUCCESS) << "CreateAndBindSocket apr_socket_addr_get";
  if (*port != 0) {
    // If a specific port was requested, it should be used.
    CHECK_EQ(*port, bound_sa->port);
  }
  *port = bound_sa->port;
}

apr_socket_t* TcpServerThreadForTesting::CreateAndBindSocket() {
  apr_socket_t* sock;
  apr_port_t port = requested_listen_port_;
  CreateAndBindSocket(pool_, &sock, &port);

  actual_listening_port_ = port;
  ready_notify_->Broadcast();

  return sock;
}

}  // namespace net_instaweb
