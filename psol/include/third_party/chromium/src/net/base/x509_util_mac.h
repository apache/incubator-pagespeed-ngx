// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_UTIL_MAC_H_
#define NET_BASE_X509_UTIL_MAC_H_

#include <CoreFoundation/CFArray.h>
#include <Security/Security.h>

#include <string>

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace net {

namespace x509_util {

// Creates a security policy for certificates used as client certificates
// in SSL.
// If a policy is successfully created, it will be stored in
// |*policy| and ownership transferred to the caller.
OSStatus NET_EXPORT CreateSSLClientPolicy(SecPolicyRef* policy);

// Create an SSL server policy. While certificate name validation will be
// performed by SecTrustEvaluate(), it has the following limitations:
// - Doesn't support IP addresses in dotted-quad literals (127.0.0.1)
// - Doesn't support IPv6 addresses
// - Doesn't support the iPAddress subjectAltName
// Providing the hostname is necessary in order to locate certain user or
// system trust preferences, such as those created by Safari. Preferences
// created by Keychain Access do not share this requirement.
// On success, stores the resultant policy in |*policy| and returns noErr.
OSStatus NET_EXPORT CreateSSLServerPolicy(const std::string& hostname,
                                          SecPolicyRef* policy);

// Creates a security policy for basic X.509 validation. If the policy is
// successfully created, it will be stored in |*policy| and ownership
// transferred to the caller.
OSStatus NET_EXPORT CreateBasicX509Policy(SecPolicyRef* policy);

// Creates security policies to control revocation checking (OCSP and CRL).
// If |enable_revocation_checking| is true, revocation checking will be
// explicitly enabled.
// If |enable_revocation_checking| is false, but |enable_ev_checking| is
// true, then the system policies for EV checking (which include checking
// for an online OCSP response) will be permitted. However, if the OS
// does not believe the certificate is EV, no revocation checking will be
// performed.
// If both are false, then the policies returned will be explicitly
// prohibited from accessing the network or the local cache, regardless of
// system settings.
// If the policies are successfully created, they will be appended to
// |policies|.
OSStatus NET_EXPORT CreateRevocationPolicies(bool enable_revocation_checking,
                                             bool enable_ev_checking,
                                             CFMutableArrayRef policies);

// Wrapper for a CSSM_DATA_PTR that was obtained via one of the CSSM field
// accessors (such as CSSM_CL_CertGet[First/Next]Value or
// CSSM_CL_CertGet[First/Next]CachedValue).
class CSSMFieldValue {
 public:
  CSSMFieldValue();
  CSSMFieldValue(CSSM_CL_HANDLE cl_handle,
                 const CSSM_OID* oid,
                 CSSM_DATA_PTR field);
  ~CSSMFieldValue();

  CSSM_OID_PTR oid() const { return oid_; }
  CSSM_DATA_PTR field() const { return field_; }

  // Returns the field as if it was an arbitrary type - most commonly, by
  // interpreting the field as a specific CSSM/CDSA parsed type, such as
  // CSSM_X509_SUBJECT_PUBLIC_KEY_INFO or CSSM_X509_ALGORITHM_IDENTIFIER.
  // An added check is applied to ensure that the current field is large
  // enough to actually contain the requested type.
  template <typename T> const T* GetAs() const {
    if (!field_ || field_->Length < sizeof(T))
      return NULL;
    return reinterpret_cast<const T*>(field_->Data);
  }

  void Reset(CSSM_CL_HANDLE cl_handle,
             CSSM_OID_PTR oid,
             CSSM_DATA_PTR field);

 private:
  CSSM_CL_HANDLE cl_handle_;
  CSSM_OID_PTR oid_;
  CSSM_DATA_PTR field_;

  DISALLOW_COPY_AND_ASSIGN(CSSMFieldValue);
};

// CSSMCachedCertificate is a container class that is used to wrap the
// CSSM_CL_CertCache APIs and provide safe and efficient access to
// certificate fields in their CSSM form.
//
// To provide efficient access to certificate/CRL fields, CSSM provides an
// API/SPI to "cache" a certificate/CRL. The exact meaning of a cached
// certificate is not defined by CSSM, but is documented to generally be some
// intermediate or parsed form of the certificate. In the case of Apple's
// CSSM CL implementation, the intermediate form is the parsed certificate
// stored in an internal format (which happens to be NSS). By caching the
// certificate, callers that wish to access multiple fields (such as subject,
// issuer, and validity dates) do not need to repeatedly parse the entire
// certificate, nor are they forced to convert all fields from their NSS types
// to their CSSM equivalents. This latter point is especially helpful when
// running on OS X 10.5, as it will fail to convert some fields that reference
// unsupported algorithms, such as ECC.
class CSSMCachedCertificate {
 public:
  CSSMCachedCertificate();
  ~CSSMCachedCertificate();

  // Initializes the CSSMCachedCertificate by caching the specified
  // |os_cert_handle|. On success, returns noErr.
  // Note: Once initialized, the cached certificate should only be accessed
  // from a single thread.
  OSStatus Init(SecCertificateRef os_cert_handle);

  // Fetches the first value for the field associated with |field_oid|.
  // If |field_oid| is a valid OID and is present in the current certificate,
  // returns CSSM_OK and stores the first value in |field|. If additional
  // values are associated with |field_oid|, they are ignored.
  OSStatus GetField(const CSSM_OID* field_oid, CSSMFieldValue* field) const;

 private:
  CSSM_CL_HANDLE cl_handle_;
  CSSM_HANDLE cached_cert_handle_;
};

}  // namespace x509_util

}  // namespace net

#endif  // NET_BASE_X509_UTIL_MAC_H_
