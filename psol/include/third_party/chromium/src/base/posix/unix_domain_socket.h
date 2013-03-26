// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POSIX_UNIX_DOMAIN_SOCKET_H_
#define BASE_POSIX_UNIX_DOMAIN_SOCKET_H_

#include <stdint.h>
#include <sys/types.h>
#include <vector>

#include "base/base_export.h"

class Pickle;

class BASE_EXPORT UnixDomainSocket {
 public:
  // Use sendmsg to write the given msg and include a vector of file
  // descriptors. Returns true if successful.
  static bool SendMsg(int fd,
                      const void* msg,
                      size_t length,
                      const std::vector<int>& fds);

  // Use recvmsg to read a message and an array of file descriptors. Returns
  // -1 on failure. Note: will read, at most, 16 descriptors.
  static ssize_t RecvMsg(int fd,
                         void* msg,
                         size_t length,
                         std::vector<int>* fds);

  // Perform a sendmsg/recvmsg pair.
  //   1. This process creates a UNIX DGRAM socketpair.
  //   2. This proces writes a request to |fd| with an SCM_RIGHTS control
  //      message containing on end of the fresh socket pair.
  //   3. This process blocks reading from the other end of the fresh
  //      socketpair.
  //   4. The target process receives the request, processes it and writes the
  //      reply to the end of the socketpair contained in the request.
  //   5. This process wakes up and continues.
  //
  //   fd: descriptor to send the request on
  //   reply: buffer for the reply
  //   reply_len: size of |reply|
  //   result_fd: (may be NULL) the file descriptor returned in the reply
  //              (if any)
  //   request: the bytes to send in the request
  static ssize_t SendRecvMsg(int fd,
                             uint8_t* reply,
                             unsigned reply_len,
                             int* result_fd,
                             const Pickle& request);
};

#endif  // BASE_POSIX_UNIX_DOMAIN_SOCKET_POSIX_H_
