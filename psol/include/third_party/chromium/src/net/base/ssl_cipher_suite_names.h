// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CIPHER_SUITE_NAMES_H_
#define NET_BASE_SSL_CIPHER_SUITE_NAMES_H_
#pragma once

#include "base/basictypes.h"
#include "net/base/net_api.h"

namespace net {

// SSLCipherSuiteToStrings returns three strings for a given cipher suite
// number, the name of the key exchange algorithm, the name of the cipher and
// the name of the MAC. The cipher suite number is the number as sent on the
// wire and recorded at
// http://www.iana.org/assignments/tls-parameters/tls-parameters.xml
// If the cipher suite is unknown, the strings are set to "???".
NET_API void SSLCipherSuiteToStrings(const char** key_exchange_str,
                                     const char** cipher_str,
                                     const char** mac_str,
                                     uint16 cipher_suite);

// SSLCompressionToString returns the name of the compression algorithm
// specified by |compression_method|, which is the TLS compression id.
// If the algorithm is unknown, |name| is set to "???".
NET_API void SSLCompressionToString(const char** name,
                                    uint8 compression_method);

// SSLVersionToString returns the name of the SSL protocol version
// specified by |ssl_version|, which is defined in
// net/base/ssl_connection_status_flags.h.
// If the version is unknown, |name| is set to "???".
NET_API void SSLVersionToString(const char** name, int ssl_version);

}  // namespace net

#endif  // NET_BASE_SSL_CIPHER_SUITE_NAMES_H_
