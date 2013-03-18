// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TCP_LISTEN_SOCKET_H_
#define NET_BASE_TCP_LISTEN_SOCKET_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/base/stream_listen_socket.h"

namespace net {

// Implements a TCP socket. Note that this is ref counted.
class NET_EXPORT TCPListenSocket : public StreamListenSocket {
 public:
  // Listen on port for the specified IP address.  Use 127.0.0.1 to only
  // accept local connections.
  static scoped_refptr<TCPListenSocket> CreateAndListen(
      const std::string& ip, int port, StreamListenSocket::Delegate* del);

  // Get raw TCP socket descriptor bound to ip:port.
  static SocketDescriptor CreateAndBind(const std::string& ip, int port);

 protected:
  friend class scoped_refptr<TCPListenSocket>;

  TCPListenSocket(SocketDescriptor s, StreamListenSocket::Delegate* del);
  virtual ~TCPListenSocket();

  // Implements StreamListenSocket::Accept.
  virtual void Accept() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TCPListenSocket);
};

// Factory that can be used to instantiate TCPListenSocket.
class NET_EXPORT TCPListenSocketFactory : public StreamListenSocketFactory {
 public:
  TCPListenSocketFactory(const std::string& ip, int port);
  virtual ~TCPListenSocketFactory();

  // StreamListenSocketFactory overrides.
  virtual scoped_refptr<StreamListenSocket> CreateAndListen(
      StreamListenSocket::Delegate* delegate) const OVERRIDE;

 private:
  const std::string ip_;
  const int port_;

  DISALLOW_COPY_AND_ASSIGN(TCPListenSocketFactory);
};

}  // namespace net

#endif  // NET_BASE_TCP_LISTEN_SOCKET_H_
