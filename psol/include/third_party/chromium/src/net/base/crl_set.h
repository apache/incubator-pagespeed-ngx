// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CRL_SET_H_
#define NET_BASE_CRL_SET_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/string_piece.h"
#include "net/base/net_export.h"

namespace base {
class DictionaryValue;
}

namespace net {

// A CRLSet is a structure that lists the serial numbers of revoked
// certificates from a number of issuers where issuers are identified by the
// SHA256 of their SubjectPublicKeyInfo.
class NET_EXPORT CRLSet : public base::RefCountedThreadSafe<CRLSet> {
 public:
  enum Result {
    REVOKED,  // the certificate should be rejected.
    UNKNOWN,  // the CRL for the certificate is not included in the set.
    GOOD,     // the certificate is not listed.
  };

  // Parse parses the bytes in |data| and, on success, puts a new CRLSet in
  // |out_crl_set| and returns true.
  static bool Parse(base::StringPiece data,
                    scoped_refptr<CRLSet>* out_crl_set);

  // CheckSPKI checks whether the given SPKI has been listed as blocked.
  //   spki_hash: the SHA256 of the SubjectPublicKeyInfo of the certificate.
  Result CheckSPKI(const base::StringPiece& spki_hash) const;

  // CheckSerial returns the information contained in the set for a given
  // certificate:
  //   serial_number: the serial number of the certificate
  //   issuer_spki_hash: the SHA256 of the SubjectPublicKeyInfo of the CRL
  //       signer
  Result CheckSerial(
      const base::StringPiece& serial_number,
      const base::StringPiece& issuer_spki_hash) const;

  // IsExpired returns true iff the current time is past the NotAfter time
  // specified in the CRLSet.
  bool IsExpired() const;

  // ApplyDelta returns a new CRLSet in |out_crl_set| that is the result of
  // updating the current CRL set with the delta information in |delta_bytes|.
  bool ApplyDelta(const base::StringPiece& delta_bytes,
                  scoped_refptr<CRLSet>* out_crl_set);

  // GetIsDeltaUpdate extracts the header from |bytes|, sets *is_delta to
  // whether |bytes| is a delta CRL set or not and returns true. In the event
  // of a parse error, it returns false.
  static bool GetIsDeltaUpdate(const base::StringPiece& bytes, bool *is_delta);

  // Serialize returns a string of bytes suitable for passing to Parse. Parsing
  // and serializing a CRLSet is a lossless operation - the resulting bytes
  // will be equal.
  std::string Serialize() const;

  // sequence returns the sequence number of this CRL set. CRL sets generated
  // by the same source are given strictly monotonically increasing sequence
  // numbers.
  uint32 sequence() const;

  // CRLList contains a list of (issuer SPKI hash, revoked serial numbers)
  // pairs.
  typedef std::vector< std::pair<std::string, std::vector<std::string> > >
      CRLList;

  // crls returns the internal state of this CRLSet. It should only be used in
  // testing.
  const CRLList& crls() const;

  // EmptyCRLSetForTesting returns a valid, but empty, CRLSet for unit tests.
  static CRLSet* EmptyCRLSetForTesting();

  // ExpiredCRLSetForTesting returns a expired, empty CRLSet for unit tests.
  static CRLSet* ExpiredCRLSetForTesting();

 private:
  CRLSet();
  ~CRLSet();

  friend class base::RefCountedThreadSafe<CRLSet>;

  // CopyBlockedSPKIsFromHeader sets |blocked_spkis_| to the list of values
  // from "BlockedSPKIs" in |header_dict|.
  bool CopyBlockedSPKIsFromHeader(base::DictionaryValue* header_dict);

  uint32 sequence_;
  CRLList crls_;
  // not_after_ contains the time, in UNIX epoch seconds, after which the
  // CRLSet should be considered stale, or 0 if no such time was given.
  uint64 not_after_;
  // crls_index_by_issuer_ maps from issuer SPKI hashes to the index in |crls_|
  // where the information for that issuer can be found. We have both |crls_|
  // and |crls_index_by_issuer_| because, when applying a delta update, we need
  // to identify a CRL by index.
  std::map<std::string, size_t> crls_index_by_issuer_;
  // blocked_spkis_ contains the SHA256 hashes of SPKIs which are to be blocked
  // no matter where in a certificate chain they might appear.
  std::vector<std::string> blocked_spkis_;
};

}  // namespace net

#endif  // NET_BASE_CRL_SET_H_
