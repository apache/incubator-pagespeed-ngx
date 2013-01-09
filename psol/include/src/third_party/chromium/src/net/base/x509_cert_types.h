// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_CERT_TYPES_H_
#define NET_BASE_X509_CERT_TYPES_H_
#pragma once

#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "net/base/net_api.h"

#if defined(OS_MACOSX)
#include <Security/x509defs.h>
#endif

namespace base {
class Time;
class StringPiece;
}  // namespace base

namespace net {

class X509Certificate;

// SHA-1 fingerprint (160 bits) of a certificate.
struct SHA1Fingerprint {
  bool Equals(const SHA1Fingerprint& other) const {
    return memcmp(data, other.data, sizeof(data)) == 0;
  }

  unsigned char data[20];
};

class SHA1FingerprintLessThan {
 public:
  bool operator() (const SHA1Fingerprint& lhs,
                   const SHA1Fingerprint& rhs) const {
    return memcmp(lhs.data, rhs.data, sizeof(lhs.data)) < 0;
  }
};

// CertPrincipal represents the issuer or subject field of an X.509 certificate.
struct NET_API CertPrincipal {
  CertPrincipal();
  explicit CertPrincipal(const std::string& name);
  ~CertPrincipal();

#if defined(OS_MACOSX)
  // Parses a BER-format DistinguishedName.
  bool ParseDistinguishedName(const void* ber_name_data, size_t length);

  // Parses a CSSM_X509_NAME struct.
  void Parse(const CSSM_X509_NAME* name);

  // Compare this CertPrincipal with |against|, returning true if they're
  // equal enough to be a possible match. This should NOT be used for any
  // security relevant decisions.
  // TODO(rsleevi): Remove once Mac client auth uses NSS for name comparison.
  bool Matches(const CertPrincipal& against) const;
#endif

  // Returns a name that can be used to represent the issuer.  It tries in this
  // order: CN, O and OU and returns the first non-empty one found.
  std::string GetDisplayName() const;

  // The different attributes for a principal.  They may be "".
  // Note that some of them can have several values.

  std::string common_name;
  std::string locality_name;
  std::string state_or_province_name;
  std::string country_name;

  std::vector<std::string> street_addresses;
  std::vector<std::string> organization_names;
  std::vector<std::string> organization_unit_names;
  std::vector<std::string> domain_components;
};

// This class is useful for maintaining policies about which certificates are
// permitted or forbidden for a particular purpose.
class NET_API CertPolicy {
 public:
  // The judgments this policy can reach.
  enum Judgment {
    // We don't have policy information for this certificate.
    UNKNOWN,

    // This certificate is allowed.
    ALLOWED,

    // This certificate is denied.
    DENIED,
  };

  CertPolicy();
  ~CertPolicy();

  // Returns the judgment this policy makes about this certificate.
  Judgment Check(X509Certificate* cert) const;

  // Causes the policy to allow this certificate.
  void Allow(X509Certificate* cert);

  // Causes the policy to deny this certificate.
  void Deny(X509Certificate* cert);

  // Returns true if this policy has allowed at least one certificate.
  bool HasAllowedCert() const;

  // Returns true if this policy has denied at least one certificate.
  bool HasDeniedCert() const;

 private:
  // The set of fingerprints of allowed certificates.
  std::set<SHA1Fingerprint, SHA1FingerprintLessThan> allowed_;

  // The set of fingerprints of denied certificates.
  std::set<SHA1Fingerprint, SHA1FingerprintLessThan> denied_;
};

#if defined(OS_MACOSX)
// Compares two OIDs by value.
inline bool CSSMOIDEqual(const CSSM_OID* oid1, const CSSM_OID* oid2) {
  return oid1->Length == oid2->Length &&
  (memcmp(oid1->Data, oid2->Data, oid1->Length) == 0);
}
#endif

// A list of ASN.1 date/time formats that ParseCertificateDate() supports,
// encoded in the canonical forms specified in RFC 2459/3280/5280.
enum CertDateFormat {
  // UTCTime: Format is YYMMDDHHMMSSZ
  CERT_DATE_FORMAT_UTC_TIME,

  // GeneralizedTime: Format is YYYYMMDDHHMMSSZ
  CERT_DATE_FORMAT_GENERALIZED_TIME,
};

// Attempts to parse |raw_date|, an ASN.1 date/time string encoded as
// |format|, and writes the result into |*time|. If an invalid date is
// specified, or if parsing fails, returns false, and |*time| will not be
// updated.
bool ParseCertificateDate(const base::StringPiece& raw_date,
                          CertDateFormat format,
                          base::Time* time);
}  // namespace net

#endif  // NET_BASE_X509_CERT_TYPES_H_
