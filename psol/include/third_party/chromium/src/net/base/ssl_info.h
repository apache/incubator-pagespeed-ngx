// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_INFO_H_
#define NET_BASE_SSL_INFO_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "net/base/cert_status_flags.h"
#include "net/base/net_export.h"
#include "net/base/x509_cert_types.h"

namespace net {

class X509Certificate;

// SSL connection info.
// This is really a struct.  All members are public.
class NET_EXPORT SSLInfo {
 public:
  // HandshakeType enumerates the possible resumption cases after an SSL
  // handshake.
  enum HandshakeType {
    HANDSHAKE_UNKNOWN = 0,
    HANDSHAKE_RESUME,  // we resumed a previous session.
    HANDSHAKE_FULL,  // we negotiated a new session.
  };

  SSLInfo();
  SSLInfo(const SSLInfo& info);
  ~SSLInfo();
  SSLInfo& operator=(const SSLInfo& info);

  void Reset();

  bool is_valid() const { return cert != NULL; }

  // Adds the specified |error| to the cert status.
  void SetCertError(int error);

  // The SSL certificate.
  scoped_refptr<X509Certificate> cert;

  // Bitmask of status info of |cert|, representing, for example, known errors
  // and extended validation (EV) status.
  // See cert_status_flags.h for values.
  CertStatus cert_status;

  // The security strength, in bits, of the SSL cipher suite.
  // 0 means the connection is not encrypted.
  // -1 means the security strength is unknown.
  int security_bits;

  // Information about the SSL connection itself. See
  // ssl_connection_status_flags.h for values. The protocol version,
  // ciphersuite, and compression in use are encoded within.
  int connection_status;

  // If the certificate is valid, then this is true iff it was rooted at a
  // standard CA root. (As opposed to a user-installed root.)
  bool is_issued_by_known_root;

  // True if a client certificate was sent to the server.  Note that sending
  // a Certificate message with no client certificate in it does not count.
  bool client_cert_sent;

  // True if a channel ID was sent to the server.
  bool channel_id_sent;

  HandshakeType handshake_type;

  // The hashes, in several algorithms, of the SubjectPublicKeyInfos from
  // each certificate in the chain.
  HashValueVector public_key_hashes;
};

}  // namespace net

#endif  // NET_BASE_SSL_INFO_H_
