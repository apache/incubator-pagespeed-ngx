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

#ifndef PAGESPEED_KERNEL_BASE_TCP_SERVER_THREAD_FOR_TESTING_H_
#define PAGESPEED_KERNEL_BASE_TCP_SERVER_THREAD_FOR_TESTING_H_

#include "apr_pools.h"
#include "apr_network_io.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// Implementation of Thread that uses APR to listen on a TCP port and then
// delegates to a virtual function to handle single connection. This code is
// absolutely not suitable for use outside of tests.

class TcpServerThreadForTesting : public ThreadSystem::Thread {
 public:
  // listen_port may be 0, in which case the system will pick the port.
  // The socket isn't actually created until the thread is Start()ed.
  TcpServerThreadForTesting(apr_port_t listen_port, StringPiece thread_name,
                            ThreadSystem* thread_system);
  // Calls this->Join() which will hang if a connection has not been handled.
  virtual ~TcpServerThreadForTesting();

  // Wait for thread to successfully start listening and then return the actual
  // bound port number, which will be bound to IPv4 localhost.
  apr_port_t GetListeningPort();

  // Helper to deal with only allocating the listening port once.
  // port_number must be a pointer to a static apr_port_t.
  static void PickListenPortOnce(apr_port_t* port_number);

 private:
  // Called after a successful call to apr_accept. Implementor must close
  // sock.
  virtual void HandleClientConnection(apr_socket_t* sock) = 0;

  // Returns a socket bound to requested_listen_port_ if non-zero, otherwise
  // whatever the system picked. Updates actual_listening_port_.
  apr_socket_t* CreateAndBindSocket();

  // Thread implementation.
  void Run() override;

  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> ready_notify_;
  apr_pool_t* pool_;
  const apr_port_t requested_listen_port_;
  apr_port_t actual_listening_port_ GUARDED_BY(mutex_);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_TCP_SERVER_THREAD_FOR_TESTING_H_
