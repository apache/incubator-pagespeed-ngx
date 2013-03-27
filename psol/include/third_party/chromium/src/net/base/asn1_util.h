// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ASN1_UTIL_H_
#define NET_BASE_ASN1_UTIL_H_

#include <vector>

#include "base/string_piece.h"
#include "net/base/net_export.h"

namespace net {

namespace asn1 {

// These are the DER encodings of the tag byte for ASN.1 objects.
static const unsigned kBOOLEAN = 0x01;
static const unsigned kINTEGER = 0x02;
static const unsigned kBITSTRING = 0x03;
static const unsigned kOCTETSTRING = 0x04;
static const unsigned kOID = 0x06;
static const unsigned kSEQUENCE = 0x30;

// These are flags that can be ORed with the above tag numbers.
static const unsigned kContextSpecific = 0x80;
static const unsigned kConstructed = 0x20;

// kAny matches any tag value;
static const unsigned kAny = 0x10000;
// kOptional denotes an optional element.
static const unsigned kOptional = 0x20000;

// ParseElement parses a DER encoded ASN1 element from |in|, requiring that
// it have the given |tag_value|. It returns true on success. The following
// limitations are imposed:
//   1) tag numbers > 31 are not permitted.
//   2) lengths > 65535 are not permitted.
// On successful return:
//   |in| is advanced over the element
//   |out| contains the element, including the tag and length bytes.
//   |out_header_len| contains the length of the tag and length bytes in |out|.
//
// If |tag_value & kOptional| is true then *out_header_len can be zero after a
// true return value if the element was not found.
bool ParseElement(base::StringPiece* in,
                  unsigned tag_value,
                  base::StringPiece* out,
                  unsigned *out_header_len);

// GetElement performs the same actions as ParseElement, except that the header
// bytes are not included in the output.
//
// If |tag_value & kOptional| is true then this function cannot distinguish
// between a missing optional element and an empty one.
bool GetElement(base::StringPiece* in,
                unsigned tag_value,
                base::StringPiece* out);

// ExtractSPKIFromDERCert parses the DER encoded certificate in |cert| and
// extracts the bytes of the SubjectPublicKeyInfo. On successful return,
// |spki_out| is set to contain the SPKI, pointing into |cert|.
NET_EXPORT_PRIVATE bool ExtractSPKIFromDERCert(base::StringPiece cert,
                                               base::StringPiece* spki_out);

// ExtractSubjectPublicKeyFromSPKI parses the DER encoded SubjectPublicKeyInfo
// in |spki| and extracts the bytes of the SubjectPublicKey. On successful
// return, |spk_out| is set to contain the public key, pointing into |spki|.
NET_EXPORT_PRIVATE bool ExtractSubjectPublicKeyFromSPKI(
    base::StringPiece spki,
    base::StringPiece* spk_out);

// ExtractCRLURLsFromDERCert parses the DER encoded certificate in |cert| and
// extracts the URL of each CRL. On successful return, the elements of
// |urls_out| point into |cert|.
//
// CRLs that only cover a subset of the reasons are omitted as the spec
// requires that at least one CRL be included that covers all reasons.
//
// CRLs that use an alternative issuer are also omitted.
//
// The nested set of GeneralNames is flattened into a single list because
// having several CRLs with one location is equivalent to having one CRL with
// several locations as far as a CRL filter is concerned.
NET_EXPORT_PRIVATE bool ExtractCRLURLsFromDERCert(
    base::StringPiece cert,
    std::vector<base::StringPiece>* urls_out);

} // namespace asn1

} // namespace net

#endif // NET_BASE_ASN1_UTIL_H_
