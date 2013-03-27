// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_STATUS_FLAGS_H_
#define NET_BASE_CERT_STATUS_FLAGS_H_

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace net {

// Bitmask of status flags of a certificate, representing any errors, as well as
// other non-error status information such as whether the certificate is EV.
typedef uint32 CertStatus;

// The possible status bits for CertStatus.
// NOTE: Because these names have appeared in bug reports, we preserve them as
// MACRO_STYLE for continuity, instead of renaming them to kConstantStyle as
// befits most static consts.
// Bits 0 to 15 are for errors.
static const CertStatus CERT_STATUS_ALL_ERRORS                 = 0xFFFF;
static const CertStatus CERT_STATUS_COMMON_NAME_INVALID        = 1 << 0;
static const CertStatus CERT_STATUS_DATE_INVALID               = 1 << 1;
static const CertStatus CERT_STATUS_AUTHORITY_INVALID          = 1 << 2;
// 1 << 3 is reserved for ERR_CERT_CONTAINS_ERRORS (not useful with WinHTTP).
static const CertStatus CERT_STATUS_NO_REVOCATION_MECHANISM    = 1 << 4;
static const CertStatus CERT_STATUS_UNABLE_TO_CHECK_REVOCATION = 1 << 5;
static const CertStatus CERT_STATUS_REVOKED                    = 1 << 6;
static const CertStatus CERT_STATUS_INVALID                    = 1 << 7;
static const CertStatus CERT_STATUS_WEAK_SIGNATURE_ALGORITHM   = 1 << 8;
// 1 << 9 was used for CERT_STATUS_NOT_IN_DNS
static const CertStatus CERT_STATUS_NON_UNIQUE_NAME            = 1 << 10;
static const CertStatus CERT_STATUS_WEAK_KEY                   = 1 << 11;

// Bits 16 to 31 are for non-error statuses.
static const CertStatus CERT_STATUS_IS_EV                      = 1 << 16;
static const CertStatus CERT_STATUS_REV_CHECKING_ENABLED       = 1 << 17;
static const CertStatus CERT_STATUS_IS_DNSSEC                  = 1 << 18;

// Returns true if the specified cert status has an error set.
static inline bool IsCertStatusError(CertStatus status) {
  return (CERT_STATUS_ALL_ERRORS & status) != 0;
}

// IsCertStatusMinorError returns true iff |cert_status| indicates a condition
// that should typically be ignored by automated requests. (i.e. a revocation
// check failure.)
NET_EXPORT bool IsCertStatusMinorError(CertStatus cert_status);

// Maps a network error code to the equivalent certificate status flag.  If
// the error code is not a certificate error, it is mapped to 0.
NET_EXPORT CertStatus MapNetErrorToCertStatus(int error);

// Maps the most serious certificate error in the certificate status flags
// to the equivalent network error code.
NET_EXPORT int MapCertStatusToNetError(CertStatus cert_status);

}  // namespace net

#endif  // NET_BASE_CERT_STATUS_FLAGS_H_
