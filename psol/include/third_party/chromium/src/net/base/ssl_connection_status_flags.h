// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CONNECTION_STATUS_FLAGS_H_
#define NET_BASE_SSL_CONNECTION_STATUS_FLAGS_H_
#pragma once

namespace net {

// Status flags for SSLInfo::connection_status.
enum {
  // The lower 16 bits are reserved for the TLS ciphersuite id.
  SSL_CONNECTION_CIPHERSUITE_SHIFT = 0,
  SSL_CONNECTION_CIPHERSUITE_MASK = 0xffff,

  // The next two bits are reserved for the compression used.
  SSL_CONNECTION_COMPRESSION_SHIFT = 16,
  SSL_CONNECTION_COMPRESSION_MASK = 3,

  // We fell back to SSLv3 for this connection.
  SSL_CONNECTION_SSL3_FALLBACK = 1 << 18,

  // The server doesn't support the renegotiation_info extension. If this bit
  // is not set then either the extension isn't supported, or we don't have any
  // knowledge either way. (The latter case will occur when we use an SSL
  // library that doesn't report it, like SChannel.)
  SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION = 1 << 19,

  // The next three bits are reserved for the SSL version.
  SSL_CONNECTION_VERSION_SHIFT = 20,
  SSL_CONNECTION_VERSION_MASK = 7,

  // 1 << 31 (the sign bit) is reserved so that the SSL connection status will
  // never be negative.
};

// NOTE: the SSL version enum constants must be between 0 and
// SSL_CONNECTION_VERSION_MASK, inclusive.
enum {
  SSL_CONNECTION_VERSION_UNKNOWN = 0,  // Unknown SSL version.
  SSL_CONNECTION_VERSION_SSL2 = 1,
  SSL_CONNECTION_VERSION_SSL3 = 2,
  SSL_CONNECTION_VERSION_TLS1 = 3,
  SSL_CONNECTION_VERSION_TLS1_1 = 4,
  SSL_CONNECTION_VERSION_TLS1_2 = 5,
  SSL_CONNECTION_VERSION_MAX,
};
COMPILE_ASSERT(SSL_CONNECTION_VERSION_MAX - 1 <= SSL_CONNECTION_VERSION_MASK,
               SSL_CONNECTION_VERSION_MASK_too_small);

inline int SSLConnectionStatusToCipherSuite(int connection_status) {
  return (connection_status >> SSL_CONNECTION_CIPHERSUITE_SHIFT) &
         SSL_CONNECTION_CIPHERSUITE_MASK;
}

inline int SSLConnectionStatusToCompression(int connection_status) {
  return (connection_status >> SSL_CONNECTION_COMPRESSION_SHIFT) &
         SSL_CONNECTION_COMPRESSION_MASK;
}

inline int SSLConnectionStatusToVersion(int connection_status) {
  return (connection_status >> SSL_CONNECTION_VERSION_SHIFT) &
         SSL_CONNECTION_VERSION_MASK;
}

}  // namespace net

#endif  // NET_BASE_SSL_CONNECTION_STATUS_FLAGS_H_
