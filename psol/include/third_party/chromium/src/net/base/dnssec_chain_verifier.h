// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DNSSEC_CHAIN_VERIFIER_H_
#define NET_BASE_DNSSEC_CHAIN_VERIFIER_H_

#include <map>
#include <string>
#include <vector>

#include "base/string_piece.h"
#include "net/base/net_export.h"

namespace net {

// DNSSECChainVerifier verifies a chain of DNSSEC records. These records
// eventually prove the validity of a set of resource records for the target
// name. For example, if the fingerprint of a certificate was stored in a CERT
// record for a given domain, then a chain could prove the validity of that
// fingerprint.
class NET_EXPORT_PRIVATE DNSSECChainVerifier {
 public:
  enum Error {
    OK = 0,
    BAD_DATA,  // The chain was corrupt in some fashion.
    UNKNOWN_ROOT_KEY,  // The chain is assuming an unknown DNS root.
    UNKNOWN_DIGEST,  // An omitted DS record used an unknown hash function.
    UNKNOWN_TERMINAL_RRTYPE,  // The chain proved an unknown RRTYPE.
    BAD_SIGNATURE,  // One of the signature was incorrect.
    NO_DS_LINK,  // a DS set didn't include the next entry key.
    OFF_COURSE,  // the chain is diverging from the target name.
    BAD_TARGET,  // the chain didn't end up at the target.
  };

  // |target|: the target hostname. This must be in canonical (all
  //     lower-case), length-prefixed DNS form. For example:
  //     "\003www\007example\003com\000"
  // |chain|: the contents of the chain.
  DNSSECChainVerifier(const std::string& target,
                      const base::StringPiece& chain);
  ~DNSSECChainVerifier();

  // If called, timestamps in the signatures will be ignored. This is for
  // testing only.
  void IgnoreTimestamps();

  // Verify verifies the chain. Returns |OK| on success.
  Error Verify();

  // rrtype returns the RRTYPE of the proven resource records. Only call this
  // after Verify has returned OK.
  uint16 rrtype() const;
  // rrdatas returns the contents of the proven resource records. Only call
  // this after Verify has returned OK.
  const std::vector<base::StringPiece>& rrdatas() const;

  // Exposed for testing only.
  static unsigned MatchingLabels(base::StringPiece a,
                                 base::StringPiece b);

 private:
  struct Zone;

  bool U8(uint8*);
  bool U16(uint16*);
  bool VariableLength16(base::StringPiece*);
  bool ReadName(base::StringPiece*);

  bool ReadAheadEntryKey(base::StringPiece*);
  bool ReadAheadKey(base::StringPiece*, uint8 entry_key);
  bool ReadDNSKEYs(std::vector<base::StringPiece>*, bool is_root);
  bool DigestKey(base::StringPiece* digest,
                 const base::StringPiece& name,
                 const base::StringPiece& dnskey,
                 uint8 digest_type,
                 uint16 keyid,
                 uint8 algorithm);

  Error EnterRoot();
  Error EnterZone(const base::StringPiece& zone);
  Error LeaveZone(base::StringPiece* next_name);
  Error ReadDSSet(std::vector<base::StringPiece>*,
                  const base::StringPiece& next_name);
  Error ReadGenericRRs(std::vector<base::StringPiece>*);
  Error ReadCNAME(std::vector<base::StringPiece>*);

  Zone* current_zone_;
  std::string target_;
  base::StringPiece chain_;
  bool ignore_timestamps_;
  bool valid_;
  // already_entered_zone_ is set to true when we unwind a Zone chain and start
  // off from a point where we have already entered a zone.
  bool already_entered_zone_;
  uint16 rrtype_;
  std::vector<base::StringPiece> rrdatas_;
  // A list of pointers which need to be free()ed on destruction.
  std::vector<void*> scratch_pool_;
};

// DnsCAARecord encapsulates code and types for dealing with Certificate
// Authority Authorization records. These are DNS records which can express
// limitations regarding acceptable certificates for a domain. See
// http://tools.ietf.org/html/draft-hallambaker-donotissue-04
class NET_EXPORT_PRIVATE DnsCAARecord {
 public:
  enum ParseResult {
    SUCCESS,  // parse successful.
    DISCARD,  // no policies applying to this client were found.
    SYNTAX_ERROR,  // the record was syntactically invalid.
    UNKNOWN_CRITICAL,  // a critical record was not understood.
  };

  // A CAAPolicy is the result of parsing a set of CAA records. It describes a
  // number of properies of certificates in a chain, any of which is sufficient
  // to validate the chain.
  struct NET_EXPORT_PRIVATE Policy {
   public:
    Policy();
    ~Policy();

    // A HashTarget identifies the object that we are hashing.
    enum HashTarget {
      USER_CERTIFICATE,
      CA_CERTIFICATE,
      SUBJECT_PUBLIC_KEY_INFO,
    };

    // A Hash is a digest of some property of a certificate.
    struct Hash {
      HashTarget target;  // what do we hash?
      int algorithm;  // NSS value, i.e. HASH_AlgSHA1.
      std::string data;  // digest, i.e. 20 bytes for SHA1.
      unsigned port;  // port number or 0 for any.
    };

    std::vector<Hash> authorized_hashes;
  };

  // Parse parses a series of DNS resource records and sets |output| to the
  // result.
  static ParseResult Parse(const std::vector<base::StringPiece>& rrdatas,
                           Policy* output);
};

}  // namespace net

#endif  // NET_BASE_DNSSEC_CHAIN_VERIFIER_H_
