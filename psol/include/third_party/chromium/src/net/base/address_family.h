// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_FAMILY_H_
#define NET_BASE_ADDRESS_FAMILY_H_
#pragma once

namespace net {

// Enum wrapper around the address family types supported by host resolver
// procedures.
enum AddressFamily {
  ADDRESS_FAMILY_UNSPECIFIED,   // AF_UNSPEC
  ADDRESS_FAMILY_IPV4,          // AF_INET
  ADDRESS_FAMILY_IPV6,          // AF_INET6
};

// HostResolverFlags is a bitflag enum used by host resolver procedures to
// determine the value of addrinfo.ai_flags and work around getaddrinfo
// peculiarities.
enum {
  HOST_RESOLVER_CANONNAME = 1 << 0,  // AI_CANONNAME
  // Hint to the resolver proc that only loopback addresses are configured.
  HOST_RESOLVER_LOOPBACK_ONLY = 1 << 1,
  // Indicate the address family was set because no IPv6 support was detected.
  HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6 = 1 << 2,
};
typedef int HostResolverFlags;

}  // namespace net

#endif  // NET_BASE_ADDRESS_FAMILY_H_
