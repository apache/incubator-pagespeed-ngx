// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LISTEN_SOCKET_UNITTEST_H_
#define NET_BASE_LISTEN_SOCKET_UNITTEST_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#endif

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "net/base/net_util.h"
#include "net/base/tcp_listen_socket.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

enum ActionType {
  ACTION_NONE = 0,
  ACTION_LISTEN = 1,
  ACTION_ACCEPT = 2,
  ACTION_READ = 3,
  ACTION_SEND = 4,
  ACTION_CLOSE = 5,
  ACTION_SHUTDOWN = 6
};

class TCPListenSocketTestAction {
 public:
  TCPListenSocketTestAction() : action_(ACTION_NONE) {}
  explicit TCPListenSocketTestAction(ActionType action) : action_(action) {}
  TCPListenSocketTestAction(ActionType action, std::string data)
      : action_(action),
        data_(data) {}

  const std::string data() const { return data_; }
  ActionType type() const { return action_; }

 private:
  ActionType action_;
  std::string data_;
};


// This had to be split out into a separate class because I couldn't
// make the testing::Test class refcounted.
class TCPListenSocketTester :
    public StreamListenSocket::Delegate,
    public base::RefCountedThreadSafe<TCPListenSocketTester> {

 public:
  TCPListenSocketTester();

  void SetUp();
  void TearDown();

  void ReportAction(const TCPListenSocketTestAction& action);
  void NextAction();

  // read all pending data from the test socket
  int ClearTestSocket();
  // Release the connection and server sockets
  void Shutdown();
  void Listen();
  void SendFromTester();
  // verify the send/read from client to server
  void TestClientSend();
  // verify send/read of a longer string
  void TestClientSendLong();
  // verify a send/read from server to client
  void TestServerSend();
  // verify multiple sends and reads from server to client.
  void TestServerSendMultiple();

  virtual bool Send(SocketDescriptor sock, const std::string& str);

  // StreamListenSocket::Delegate:
  virtual void DidAccept(StreamListenSocket* server,
                         StreamListenSocket* connection) OVERRIDE;
  virtual void DidRead(StreamListenSocket* connection, const char* data,
                       int len) OVERRIDE;
  virtual void DidClose(StreamListenSocket* sock) OVERRIDE;

  scoped_ptr<base::Thread> thread_;
  MessageLoopForIO* loop_;
  scoped_refptr<TCPListenSocket> server_;
  StreamListenSocket* connection_;
  TCPListenSocketTestAction last_action_;

  SocketDescriptor test_socket_;
  static const int kTestPort;

  base::Lock lock_;  // protects |queue_| and wraps |cv_|
  base::ConditionVariable cv_;
  std::deque<TCPListenSocketTestAction> queue_;

 protected:
  friend class base::RefCountedThreadSafe<TCPListenSocketTester>;

  virtual ~TCPListenSocketTester();

  virtual scoped_refptr<TCPListenSocket> DoListen();
};

}  // namespace net

#endif  // NET_BASE_LISTEN_SOCKET_UNITTEST_H_
