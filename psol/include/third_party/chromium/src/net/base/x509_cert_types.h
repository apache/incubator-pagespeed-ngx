// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_CERT_TYPES_H_
#define NET_BASE_X509_CERT_TYPES_H_

#include <string.h>

#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_piece.h"
#include "build/build_config.h"
#include "net/base/net_export.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include <Security/x509defs.h>
#endif

namespace base {
class Time;
}  // namespace base

namespace net {

class X509Certificate;

// SHA-1 fingerprint (160 bits) of a certificate.
struct NET_EXPORT SHA1HashValue {
  bool Equals(const SHA1HashValue& other) const {
    return memcmp(data, other.data, sizeof(data)) == 0;
  }

  unsigned char data[20];
};

class NET_EXPORT SHA1HashValueLessThan {
 public:
  bool operator()(const SHA1HashValue& lhs,
                  const SHA1HashValue& rhs) const {
    return memcmp(lhs.data, rhs.data, sizeof(lhs.data)) < 0;
  }
};

struct NET_EXPORT SHA256HashValue {
  bool Equals(const SHA256HashValue& other) const {
    return memcmp(data, other.data, sizeof(data)) == 0;
  }

  unsigned char data[32];
};

class NET_EXPORT SHA256HashValueLessThan {
 public:
  bool operator()(const SHA256HashValue& lhs,
                  const SHA256HashValue& rhs) const {
    return memcmp(lhs.data, rhs.data, sizeof(lhs.data)) < 0;
  }
};

enum HashValueTag {
  HASH_VALUE_SHA1,
  HASH_VALUE_SHA256,

  // This must always be last.
  HASH_VALUE_TAGS_COUNT
};

class NET_EXPORT HashValue {
 public:
  explicit HashValue(HashValueTag tag) : tag(tag) {}
  HashValue() : tag(HASH_VALUE_SHA1) {}

  bool Equals(const HashValue& other) const;
  size_t size() const;
  unsigned char* data();
  const unsigned char* data() const;

  HashValueTag tag;

 private:
  union {
    SHA1HashValue sha1;
    SHA256HashValue sha256;
  } fingerprint;
};

class NET_EXPORT HashValueLessThan {
 public:
  bool operator()(const HashValue& lhs,
                  const HashValue& rhs) const {
    size_t lhs_size = lhs.size();
    size_t rhs_size = rhs.size();

    if (lhs_size != rhs_size)
      return lhs_size < rhs_size;

    return memcmp(lhs.data(), rhs.data(), lhs_size) < 0;
  }
};

typedef std::vector<HashValue> HashValueVector;

// IsSHA1HashInSortedArray returns true iff |hash| is in |array|, a sorted
// array of SHA1 hashes.
bool NET_EXPORT IsSHA1HashInSortedArray(const SHA1HashValue& hash,
                                        const uint8* array,
                                        size_t array_byte_len);

// CertPrincipal represents the issuer or subject field of an X.509 certificate.
struct NET_EXPORT CertPrincipal {
  CertPrincipal();
  explicit CertPrincipal(const std::string& name);
  ~CertPrincipal();

#if (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_WIN)
  // Parses a BER-format DistinguishedName.
  bool ParseDistinguishedName(const void* ber_name_data, size_t length);
#endif

#if defined(OS_MACOSX)
  // Compare this CertPrincipal with |against|, returning true if they're
  // equal enough to be a possible match. This should NOT be used for any
  // security relevant decisions.
  // TODO(rsleevi): Remove once Mac client auth uses NSS for name comparison.
  bool Matches(const CertPrincipal& against) const;
#endif

  // Returns a name that can be used to represent the issuer.  It tries in this
  // order: CN, O and OU and returns the first non-empty one found.
  std::string GetDisplayName() const;

  // The different attributes for a principal, stored in UTF-8.  They may be "".
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
class NET_EXPORT CertPolicy {
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
  std::set<SHA1HashValue, SHA1HashValueLessThan> allowed_;

  // The set of fingerprints of denied certificates.
  std::set<SHA1HashValue, SHA1HashValueLessThan> denied_;
};

#if defined(OS_MACOSX) && !defined(OS_IOS)
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
