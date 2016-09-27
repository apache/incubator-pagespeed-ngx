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
// Author: yeputons@google.com (Egor Suvorov)

#ifndef PAGESPEED_SYSTEM_TCP_CONNECTION_FOR_TESTING_H_
#define PAGESPEED_SYSTEM_TCP_CONNECTION_FOR_TESTING_H_

#include "apr_pools.h"
#include "apr_network_io.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// Simple TCP connection class for talking to external servers. Initiates
// connection in Connect(), closes it in destructor. Does not handle any errors
// except for connection failure, crashes with CHECK() failures on the rest.
class TcpConnectionForTesting {
 public:
  TcpConnectionForTesting();
  ~TcpConnectionForTesting();

  bool Connect(const GoogleString& hostname, int port);

  void Send(StringPiece data);

  // Reads specific amount of bytes, fais if EOF happens before.
  GoogleString ReadBytes(int length);

  // LF is included unless EOF happened before it.
  GoogleString ReadLine() {
    return ReadUntil("\n");
  }

  // CRLF is included unless EOF happened before it.
  GoogleString ReadLineCrLf() {
    return ReadUntil("\r\n");
  }

  // Returns data read, marker is included unless EOF happened before it.
  GoogleString ReadUntil(StringPiece marker);

 private:
  apr_pool_t* pool_;
  apr_socket_t* socket_;

  DISALLOW_COPY_AND_ASSIGN(TcpConnectionForTesting);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_TCP_CONNECTION_FOR_TESTING_H_
