// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNC_SOCKET_H_
#define BASE_SYNC_SOCKET_H_
#pragma once

// A socket abstraction used for sending and receiving plain
// data.  Because they are blocking, they can be used to perform
// rudimentary cross-process synchronization with low latency.

#include "base/basictypes.h"
#if defined(OS_WIN)
#include <windows.h>
#endif
#include <sys/types.h>

#include "base/base_api.h"

namespace base {

class BASE_API SyncSocket {
 public:
#if defined(OS_WIN)
  typedef HANDLE Handle;
#else
  typedef int Handle;
#endif

  // Creates a SyncSocket from a Handle.  Used in transport.
  explicit SyncSocket(Handle handle) : handle_(handle) { }
  ~SyncSocket() { Close(); }

  // Creates an unnamed pair of connected sockets.
  // pair is a pointer to an array of two SyncSockets in which connected socket
  // descriptors are returned.  Returns true on success, false on failure.
  static bool CreatePair(SyncSocket* pair[2]);

  // Closes the SyncSocket.  Returns true on success, false on failure.
  bool Close();

  // Sends the message to the remote peer of the SyncSocket.
  // Note it is not safe to send messages from the same socket handle by
  // multiple threads simultaneously.
  // buffer is a pointer to the data to send.
  // length is the length of the data to send (must be non-zero).
  // Returns the number of bytes sent, or 0 upon failure.
  size_t Send(const void* buffer, size_t length);

  // Receives a message from an SyncSocket.
  // buffer is a pointer to the buffer to receive data.
  // length is the number of bytes of data to receive (must be non-zero).
  // Returns the number of bytes received, or 0 upon failure.
  size_t Receive(void* buffer, size_t length);

  // Returns the number of bytes available. If non-zero, Receive() will not
  // not block when called. NOTE: Some implementations cannot reliably
  // determine the number of bytes available so avoid using the returned
  // size as a promise and simply test against zero.
  size_t Peek();

  // Extracts the contained handle.  Used for transferring between
  // processes.
  Handle handle() const { return handle_; }

 private:
  Handle handle_;

  DISALLOW_COPY_AND_ASSIGN(SyncSocket);
};

}  // namespace base

#endif  // BASE_SYNC_SOCKET_H_
