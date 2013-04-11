// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stream-based listen socket implementation that handles reading and writing
// to the socket, but does not handle creating the socket nor connecting
// sockets, which are handled by subclasses on creation and in Accept,
// respectively.

// StreamListenSocket handles IO asynchronously in the specified MessageLoop.
// This class is NOT thread safe. It uses WSAEVENT handles to monitor activity
// in a given MessageLoop. This means that callbacks will happen in that loop's
// thread always and that all other methods (including constructor and
// destructor) should also be called from the same thread.

#ifndef NET_BASE_STREAM_LISTEN_SOCKET_H_
#define NET_BASE_STREAM_LISTEN_SOCKET_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>
#endif
#include <string>
#if defined(OS_WIN)
#include "base/win/object_watcher.h"
#elif defined(OS_POSIX)
#include "base/message_loop.h"
#endif

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/base/stream_listen_socket.h"

#if defined(OS_POSIX)
typedef int SocketDescriptor;
#else
typedef SOCKET SocketDescriptor;
#endif

namespace net {

class IPEndPoint;

class NET_EXPORT StreamListenSocket
    : public base::RefCountedThreadSafe<StreamListenSocket>,
#if defined(OS_WIN)
      public base::win::ObjectWatcher::Delegate {
#elif defined(OS_POSIX)
      public MessageLoopForIO::Watcher {
#endif

 public:
  // TODO(erikkay): this delegate should really be split into two parts
  // to split up the listener from the connected socket.  Perhaps this class
  // should be split up similarly.
  class Delegate {
   public:
    // |server| is the original listening Socket, connection is the new
    // Socket that was created.  Ownership of |connection| is transferred
    // to the delegate with this call.
    virtual void DidAccept(StreamListenSocket* server,
                           StreamListenSocket* connection) = 0;
    virtual void DidRead(StreamListenSocket* connection,
                         const char* data,
                         int len) = 0;
    virtual void DidClose(StreamListenSocket* sock) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Send data to the socket.
  void Send(const char* bytes, int len, bool append_linefeed = false);
  void Send(const std::string& str, bool append_linefeed = false);

  // Copies the local address to |address|. Returns a network error code.
  int GetLocalAddress(IPEndPoint* address);

  static const SocketDescriptor kInvalidSocket;
  static const int kSocketError;

 protected:
  enum WaitState {
    NOT_WAITING      = 0,
    WAITING_ACCEPT   = 1,
    WAITING_READ     = 2
  };

  StreamListenSocket(SocketDescriptor s, Delegate* del);
  virtual ~StreamListenSocket();

  SocketDescriptor AcceptSocket();
  virtual void Accept() = 0;

  void Listen();
  void Read();
  void Close();
  void CloseSocket(SocketDescriptor s);

  // Pass any value in case of Windows, because in Windows
  // we are not using state.
  void WatchSocket(WaitState state);
  void UnwatchSocket();

  Delegate* const socket_delegate_;

 private:
  friend class base::RefCountedThreadSafe<StreamListenSocket>;
  friend class TransportClientSocketTest;

  void SendInternal(const char* bytes, int len);

#if defined(OS_WIN)
  // ObjectWatcher delegate.
  virtual void OnObjectSignaled(HANDLE object);
  base::win::ObjectWatcher watcher_;
  HANDLE socket_event_;
#elif defined(OS_POSIX)
  // Called by MessagePumpLibevent when the socket is ready to do I/O.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;
  WaitState wait_state_;
  // The socket's libevent wrapper.
  MessageLoopForIO::FileDescriptorWatcher watcher_;
#endif

  // NOTE: This is for unit test use only!
  // Pause/Resume calling Read(). Note that ResumeReads() will also call
  // Read() if there is anything to read.
  void PauseReads();
  void ResumeReads();

  const SocketDescriptor socket_;
  bool reads_paused_;
  bool has_pending_reads_;

  DISALLOW_COPY_AND_ASSIGN(StreamListenSocket);
};

// Abstract factory that must be subclassed for each subclass of
// StreamListenSocket.
class NET_EXPORT StreamListenSocketFactory {
 public:
  virtual ~StreamListenSocketFactory() {}

  // Returns a new instance of StreamListenSocket or NULL if an error occurred.
  virtual scoped_refptr<StreamListenSocket> CreateAndListen(
      StreamListenSocket::Delegate* delegate) const = 0;
};

}  // namespace net

#endif  // NET_BASE_STREAM_LISTEN_SOCKET_H_
