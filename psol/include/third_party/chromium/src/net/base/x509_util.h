// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_UTIL_H_
#define NET_BASE_X509_UTIL_H_

#include <string>

#include "base/time.h"
#include "net/base/net_export.h"

namespace crypto {
class ECPrivateKey;
}

namespace net {

namespace x509_util {

// Returns true if the times can be used to create an X.509 certificate.
// Certificates can accept dates from Jan 1st, 1 to Dec 31, 9999.  A bug in NSS
// limited the range to 1950-9999
// (https://bugzilla.mozilla.org/show_bug.cgi?id=786531).  This function will
// return whether it is supported by the currently used crypto library.
bool IsSupportedValidityRange(base::Time not_valid_before,
                              base::Time not_valid_after);

// Creates a server bound certificate containing the public key in |key|.
// Domain, serial number and validity period are given as
// parameters. The certificate is signed by the private key in |key|.
// The hashing algorithm for the signature is SHA-1.
//
// See Internet Draft draft-balfanz-tls-obc-00 for more details:
// http://tools.ietf.org/html/draft-balfanz-tls-obc-00
bool NET_EXPORT_PRIVATE CreateDomainBoundCertEC(
    crypto::ECPrivateKey* key,
    const std::string& domain,
    uint32 serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after,
    std::string* der_cert);

} // namespace x509_util

} // namespace net

#endif  // NET_BASE_X509_UTIL_H_
