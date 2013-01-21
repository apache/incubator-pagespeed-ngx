// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CRL_FILTER_H_
#define NET_BASE_CRL_FILTER_H_
#pragma once

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/synchronization/lock.h"
#include "net/base/net_api.h"

class DictionaryValue;

namespace net {

class GolombCompressedSet;

// A CRLFilter is a probabilistic data structure for eliminating certificate
// revocation checks. A CRL filter contains information about some number of
// globally well known CRLs. Those CRLs are said to be `covered' by the filter.
//
// If a certificate specifies a CRL that is covered then the CRLFilter can give
// a firm "not revoked" answer or a probabilistic "revoked" answer.
// Additionally, a CRLFilter can contain a list of blocked public keys and, in
// that case, it can give a firm "revoked" answer.
class NET_TEST CRLFilter : public base::RefCounted<CRLFilter> {
 public:
  enum Result {
    REVOKED,  // the certificate should be rejected.
    PROBABLY_REVOKED,  // the certificate should be checked.
    NOT_REVOKED,  // the certificate is acceptable.
    UNKNOWN,  // no information available.
  };

  ~CRLFilter();

  static bool Parse(base::StringPiece data,
                    scoped_refptr<CRLFilter>* out_crl_filter);

  // CheckCertificate returns the information contained in the filter for a
  // given certificate:
  //   cert_spki: the SubjectPublicKeyInfo for the certificate
  //   serial_number: the serial number of the certificate
  //   crl_urls: the URLs for the CRL for the certificate
  //   parent_spki: the SubjectPublicKeyInfo of the CRL signer
  //
  // This does not check that the CRLFilter is timely. See |not_before| and
  // |not_after|.
  Result CheckCertificate(
      base::StringPiece cert_spki,
      const std::string& serial_number,
      const std::vector<base::StringPiece>& crl_urls,
      base::StringPiece parent_spki);

  // ApplyDelta returns a new CRLFilter in |out_crl_filter| that is the result
  // of updating the current filter with the delta information in
  // |delta_bytes|.
  bool ApplyDelta(base::StringPiece delta_bytes,
                  scoped_refptr<CRLFilter>* out_crl_filter);

  // not_before and not_after return the validity timespan of this filter.
  // |CheckCertificate| does not check the current time so it's up to the
  // caller to ensure that the CRLFilter is timely.
  int64 not_before() const;
  int64 not_after() const;

  // DebugValues return all GCS values, in order. This should only be used
  // for testing.
  std::vector<uint64> DebugValues();
  // num_entries returns the number of GCS values in the filter. This should
  // only be used for testing.
  unsigned num_entries() const;
  // max_range returns size of the hash range. This should only be used for
  // testing.
  uint64 max_range() const;
  // SHA256 returns a hash over the header and GCS bytes of the filter. This
  // should only be used for testing.
  std::string SHA256() const;

 private:
  CRLFilter();

  // These are the range coder symbols used in delta updates.
  enum {
    SYMBOL_SAME = 0,
    SYMBOL_INSERT = 1,
    SYMBOL_DELETE = 2,
  };

  static CRLFilter* CRLFilterFromHeader(base::StringPiece header);
  bool CRLIsCovered(const std::vector<base::StringPiece>& crl_urls,
                    const std::string& parent_spki_sha256);

  int64 not_before_, not_after_;
  uint64 max_range_;
  unsigned sequence_;
  unsigned num_entries_;

  std::string header_bytes_;

  std::set<std::pair<std::string, std::string> > crls_included_;
  std::string gcs_bytes_;
  scoped_ptr<GolombCompressedSet> gcs_;
};

}  // namespace net

#endif  // NET_BASE_CRL_FILTER_H_
