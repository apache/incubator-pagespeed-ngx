// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TCP/IP server that handles IO asynchronously in the specified MessageLoop.
// These objects are NOT thread safe.  They use WSAEVENT handles to monitor
// activity in a given MessageLoop.  This means that callbacks will
// happen in that loop's thread always and that all other methods (including
// constructors and destructors) should also be called from the same thread.

#ifndef NET_BASE_LISTEN_SOCKET_H_
#define NET_BASE_LISTEN_SOCKET_H_
#pragma once

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
#include "base/memory/ref_counted.h"
#include "net/base/net_api.h"

#if defined(OS_POSIX)
typedef int SOCKET;
#endif

// Implements a raw socket interface
class NET_API ListenSocket : public base::RefCountedThreadSafe<ListenSocket>,
#if defined(OS_WIN)
                             public base::win::ObjectWatcher::Delegate {
#elif defined(OS_POSIX)
                             public MessageLoopForIO::Watcher {
#endif
 public:
  // TODO(erikkay): this delegate should really be split into two parts
  // to split up the listener from the connected socket.  Perhaps this class
  // should be split up similarly.
  class ListenSocketDelegate {
   public:
    virtual ~ListenSocketDelegate() {}

    // server is the original listening Socket, connection is the new
    // Socket that was created.  Ownership of connection is transferred
    // to the delegate with this call.
    virtual void DidAccept(ListenSocket *server, ListenSocket *connection) = 0;
    virtual void DidRead(ListenSocket *connection,
                         const char* data,
                         int len) = 0;
    virtual void DidClose(ListenSocket *sock) = 0;
  };

  // Listen on port for the specified IP address.  Use 127.0.0.1 to only
  // accept local connections.
  static ListenSocket* Listen(std::string ip, int port,
                              ListenSocketDelegate* del);

  // Send data to the socket.
  void Send(const char* bytes, int len, bool append_linefeed = false);
  void Send(const std::string& str, bool append_linefeed = false);

  // NOTE: This is for unit test use only!
  // Pause/Resume calling Read().  Note that ResumeReads() will also call
  // Read() if there is anything to read.
  void PauseReads();
  void ResumeReads();

 protected:
  friend class base::RefCountedThreadSafe<ListenSocket>;

  enum WaitState {
    NOT_WAITING      = 0,
    WAITING_ACCEPT   = 1,
    WAITING_READ     = 3,
    WAITING_CLOSE    = 4
  };

  static const SOCKET kInvalidSocket;
  static const int kSocketError;

  ListenSocket(SOCKET s, ListenSocketDelegate* del);
  virtual ~ListenSocket();
  static SOCKET Listen(std::string ip, int port);
  // if valid, returned SOCKET is non-blocking
  static SOCKET Accept(SOCKET s);

  virtual void SendInternal(const char* bytes, int len);

  virtual void Listen();
  virtual void Accept();
  virtual void Read();
  virtual void Close();
  virtual void CloseSocket(SOCKET s);

  // Pass any value in case of Windows, because in Windows
  // we are not using state.
  void WatchSocket(WaitState state);
  void UnwatchSocket();

#if defined(OS_WIN)
  // ObjectWatcher delegate
  virtual void OnObjectSignaled(HANDLE object);
  base::win::ObjectWatcher watcher_;
  HANDLE socket_event_;
#elif defined(OS_POSIX)
  // Called by MessagePumpLibevent when the socket is ready to do I/O
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd);
  WaitState wait_state_;
  // The socket's libevent wrapper
  MessageLoopForIO::FileDescriptorWatcher watcher_;
#endif

  SOCKET socket_;
  ListenSocketDelegate *socket_delegate_;

 private:
  bool reads_paused_;
  bool has_pending_reads_;

  DISALLOW_COPY_AND_ASSIGN(ListenSocket);
};

#endif  // NET_BASE_LISTEN_SOCKET_H_
