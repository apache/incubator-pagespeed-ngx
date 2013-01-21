// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DNSSEC_KEYSET_H_
#define NET_BASE_DNSSEC_KEYSET_H_

#include <string>
#include <vector>

#include "base/string_piece.h"
#include "net/base/net_api.h"

namespace net {

// DNSSECKeySet function wraps crypto/signature_verifier.h to accept
// DNSSEC encodings. (See RFC 4043)
class NET_TEST DNSSECKeySet {
 public:
  DNSSECKeySet();
  ~DNSSECKeySet();

  // AddKey adds a key to the trusted set.
  //   dnskey: the RRDATA of a DNSKEY.
  bool AddKey(const base::StringPiece& dnskey);

  // CheckSignature checks the DNSSEC signature on set of resource records.
  //   name: the domain that the records are from
  //   zone: the signing zone
  //   signature: the RRSIG signature, not include the signing zone.
  //   rrtype: the type of the resource records
  //   rrdatas: the RRDATA of the signed resource records, in canonical order.
  bool CheckSignature(const base::StringPiece& name,
                      const base::StringPiece& zone,
                      const base::StringPiece& signature,
                      uint16 rrtype,
                      const std::vector<base::StringPiece>& rrdatas);

  // DNSKEYToKeyID converts the RRDATA of a DNSKEY to its key id. See RFC 4043,
  // app B.
  static uint16 DNSKEYToKeyID(const base::StringPiece& dnskey);

  // Used for testing: the timestamps on signatures will be ignored to allow
  // golden data to remain valid.
  void IgnoreTimestamps();

 private:
  bool VerifySignature(
      base::StringPiece signature_algorithm,
      base::StringPiece signature,
      base::StringPiece public_key,
      base::StringPiece signed_data);

  std::string ASN1WrapDNSKEY(const base::StringPiece& dnskey);

  bool ignore_timestamps_;
  std::vector<uint16> keyids_;
  std::vector<std::string> public_keys_;
};

}  // namespace net

#endif  // NET_BASE_DNSSEC_KEYSET_H_
