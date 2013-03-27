// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains functions for iOS to glue NSS and Security.framework
// together.

#ifndef NET_BASE_X509_UTIL_IOS_H_
#define NET_BASE_X509_UTIL_IOS_H_

#include <Security/Security.h>
#include <vector>

#include "net/base/x509_cert_types.h"

// Forward declaration; real one in <cert.h>
typedef struct CERTCertificateStr CERTCertificate;

namespace net {

class X509Certificate;

namespace x509_util_ios {

// Converts a Security.framework certificate handle (SecCertificateRef) into
// an NSS certificate handle (CERTCertificate*).
CERTCertificate* CreateNSSCertHandleFromOSHandle(SecCertificateRef cert_handle);

// Converts an NSS certificate handle (CERTCertificate*) into a
// Security.framework handle (SecCertificateRef)
SecCertificateRef CreateOSCertHandleFromNSSHandle(
    CERTCertificate* nss_cert_handle);

// Create a new X509Certificate from the specified NSS server cert and
// intermediates. This is functionally equivalent to
// X509Certificate::CreateFromHandle(), except it supports receiving
// NSS CERTCertificate*s rather than iOS SecCertificateRefs.
X509Certificate* CreateCertFromNSSHandles(
    CERTCertificate* cert_handle,
    const std::vector<CERTCertificate*>& intermediates);

SHA1HashValue CalculateFingerprintNSS(CERTCertificate* cert);

// This is a wrapper class around the native NSS certificate handle.
// The constructor copies the certificate data from |cert_handle| and
// uses the NSS library to parse it.
class NSSCertificate {
 public:
  explicit NSSCertificate(SecCertificateRef cert_handle);
  ~NSSCertificate();
  CERTCertificate* cert_handle() const;
 private:
  CERTCertificate* nss_cert_handle_;
};

// A wrapper class that loads a certificate and all of its intermediates into
// NSS. This is necessary for libpkix path building to be able to locate
// needed intermediates.
class NSSCertChain {
 public:
  explicit NSSCertChain(X509Certificate* certificate);
  ~NSSCertChain();
  CERTCertificate* cert_handle() const;
 private:
  std::vector<CERTCertificate*> certs_;
};

}  // namespace x509_util_ios
}  // namespace net

#endif  // NET_BASE_X509_UTIL_IOS_H_
