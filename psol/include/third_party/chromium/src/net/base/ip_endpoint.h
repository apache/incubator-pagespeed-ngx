// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IP_ENDPOINT_H_
#define NET_BASE_IP_ENDPOINT_H_
#pragma once

#include "base/basictypes.h"
#include "net/base/net_api.h"
#include "net/base/net_util.h"

struct sockaddr;

namespace net {

// An IPEndPoint represents the address of a transport endpoint:
//  * IP address (either v4 or v6)
//  * Port
class NET_API IPEndPoint {
 public:
  IPEndPoint();
  virtual ~IPEndPoint();
  IPEndPoint(const IPAddressNumber& address, int port);
  IPEndPoint(const IPEndPoint& endpoint);

  const IPAddressNumber& address() const { return address_; }
  int port() const { return port_; }

  // Returns AF_INET or AF_INET6 depending on the type of the address.
  int GetFamily() const;

  // Convert to a provided sockaddr struct.
  // |address| is the sockaddr to copy into.  Should be at least
  //    sizeof(struct sockaddr_storage) bytes.
  // |address_length| is an input/output parameter.  On input, it is the
  //    size of data in |address| available.  On output, it is the size of
  //    the address that was copied into |address|.
  // Returns true on success, false on failure.
  bool ToSockAddr(struct sockaddr* address, size_t* address_length) const;

  // Convert from a sockaddr struct.
  // |address| is the address.
  // |address_length| is the length of |address|.
  // Returns true on success, false on failure.
  bool FromSockAddr(const struct sockaddr* address, size_t address_length);

  // Returns value as a string (e.g. "127.0.0.1:80"). Returns empty
  // string if the address is invalid, and cannot not be converted to a
  // string.
  std::string ToString() const;

  bool operator<(const IPEndPoint& that) const;
  bool operator==(const IPEndPoint& that) const;

 private:
  IPAddressNumber address_;
  int port_;
};

}  // namespace net

#endif  // NET_BASE_IP_ENDPOINT_H_
