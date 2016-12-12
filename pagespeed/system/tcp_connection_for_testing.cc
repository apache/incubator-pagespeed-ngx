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
#include "pagespeed/system/tcp_connection_for_testing.h"

#include <sys/socket.h>

#include "apr_network_io.h"
#include "base/logging.h"
#include "strings/stringpiece_utils.h"
#include "pagespeed/system/apr_thread_compatible_pool.h"

namespace net_instaweb {

TcpConnectionForTesting::TcpConnectionForTesting()
    : pool_(AprCreateThreadCompatiblePool(nullptr)), socket_(nullptr) {}

bool TcpConnectionForTesting::Connect(const GoogleString& hostname, int port) {
  CHECK(!socket_);

  apr_sockaddr_t* addr;
  if (apr_sockaddr_info_get(&addr, hostname.c_str(), APR_INET, port, 0,
                            pool_) != APR_SUCCESS) {
    return false;
  }
  if (apr_socket_create(&socket_, APR_INET, SOCK_STREAM, APR_PROTO_TCP,
                        pool_) != APR_SUCCESS) {
    return false;
  }
  return apr_socket_connect(socket_, addr) == APR_SUCCESS;
}

void TcpConnectionForTesting::Send(StringPiece data) {
  CHECK(socket_);
  apr_size_t length = data.size();
  CHECK_EQ(apr_socket_send(socket_, data.data(), &length), APR_SUCCESS);
  CHECK_EQ(length, data.size());
}

GoogleString TcpConnectionForTesting::ReadBytes(int length) {
  CHECK(socket_);

  GoogleString result;
  result.resize(length);
  int read = 0;
  while (read < length) {
    apr_size_t l = length - read;
    // 'l' is buffer size on input, amount of bytes received on output.
    CHECK(apr_socket_recv(socket_, &result[read], &l) == APR_SUCCESS);
    read += l;
  }
  return result;
}

GoogleString TcpConnectionForTesting::ReadUntil(StringPiece marker) {
  CHECK(socket_);
  GoogleString result;
  while (!strings::EndsWith(StringPiece(result), marker)) {
    char data;
    apr_size_t length = 1;
    apr_status_t status = apr_socket_recv(socket_, &data, &length);
    result += data;
    if (status == APR_EOF) {
      break;
    }
    CHECK_EQ(status, APR_SUCCESS);
  }
  return result;
}

TcpConnectionForTesting::~TcpConnectionForTesting() {
  if (socket_) {
    apr_socket_close(socket_);
  }
  apr_pool_destroy(pool_);
}

}  // namespace net_instaweb
