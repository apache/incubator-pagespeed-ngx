// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_UTIL_NSS_H_
#define NET_BASE_X509_UTIL_NSS_H_

#include <string>
#include <vector>

#include "base/time.h"
#include "net/base/x509_certificate.h"

class PickleIterator;

typedef struct CERTCertificateStr CERTCertificate;
typedef struct CERTNameStr CERTName;
typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey;
typedef struct SECItemStr SECItem;
typedef struct SECKEYPublicKeyStr SECKEYPublicKey;

namespace net {

namespace x509_util {

// Creates a self-signed certificate containing |public_key|.  Subject, serial
// number and validity period are given as parameters.  The certificate is
// signed by |private_key|. The hashing algorithm for the signature is SHA-1.
// |subject| is a distinguished name defined in RFC4514.
CERTCertificate* CreateSelfSignedCert(
    SECKEYPublicKey* public_key,
    SECKEYPrivateKey* private_key,
    const std::string& subject,
    uint32 serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after);

#if defined(USE_NSS) || defined(OS_IOS)
// Parses the Principal attribute from |name| and outputs the result in
// |principal|.
void ParsePrincipal(CERTName* name,
                    CertPrincipal* principal);

// Parses the date from |der_date| and outputs the result in |result|.
void ParseDate(const SECItem* der_date, base::Time* result);

// Parses the serial number from |certificate|.
std::string ParseSerialNumber(const CERTCertificate* certificate);

// Gets the subjectAltName extension field from the certificate, if any.
void GetSubjectAltName(CERTCertificate* cert_handle,
                       std::vector<std::string>* dns_names,
                       std::vector<std::string>* ip_addrs);

// Creates all possible OS certificate handles from |data| encoded in a specific
// |format|. Returns an empty collection on failure.
X509Certificate::OSCertHandles CreateOSCertHandlesFromBytes(
    const char* data,
    int length,
    X509Certificate::Format format);

// Reads a single certificate from |pickle_iter| and returns a platform-specific
// certificate handle. Returns an invalid handle, NULL, on failure.
X509Certificate::OSCertHandle ReadOSCertHandleFromPickle(
    PickleIterator* pickle_iter);

// Sets |*size_bits| to be the length of the public key in bits, and sets
// |*type| to one of the |PublicKeyType| values. In case of
// |kPublicKeyTypeUnknown|, |*size_bits| will be set to 0.
void GetPublicKeyInfo(CERTCertificate* handle,
                      size_t* size_bits,
                      X509Certificate::PublicKeyType* type);
#endif  // defined(USE_NSS) || defined(OS_IOS)

} // namespace x509_util

} // namespace net

#endif  // NET_BASE_X509_UTIL_NSS_H_
