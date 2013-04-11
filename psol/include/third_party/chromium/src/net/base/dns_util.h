// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DNS_UTIL_H_
#define NET_BASE_DNS_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/string_piece.h"
#include "net/base/net_export.h"

namespace net {

// DNSDomainFromDot - convert a domain string to DNS format. From DJB's
// public domain DNS library.
//
//   dotted: a string in dotted form: "www.google.com"
//   out: a result in DNS form: "\x03www\x06google\x03com\x00"
NET_EXPORT_PRIVATE bool DNSDomainFromDot(const base::StringPiece& dotted,
                                         std::string* out);

// DNSDomainToString converts a domain in DNS format to a dotted string.
// Excludes the dot at the end.
NET_EXPORT_PRIVATE std::string DNSDomainToString(
    const base::StringPiece& domain);

// Returns true iff the given character is in the set of valid DNS label
// characters as given in RFC 3490, 4.1, 3(a)
NET_EXPORT_PRIVATE bool IsSTD3ASCIIValidCharacter(char c);

// Returns the hostname by trimming the ending dot, if one exists.
NET_EXPORT std::string TrimEndingDot(const base::StringPiece& host);

// TODO(szym): remove all definitions below once DnsRRResolver migrates to
// DnsClient

// DNS class types.
static const uint16 kClassIN = 1;

// DNS resource record types. See
// http://www.iana.org/assignments/dns-parameters
// WARNING: if you're adding any new values here you may need to add them to
// dnsrr_resolver.cc:DnsRRIsParsedByWindows.
static const uint16 kDNS_A = 1;
static const uint16 kDNS_CNAME = 5;
static const uint16 kDNS_TXT = 16;
static const uint16 kDNS_AAAA = 28;
static const uint16 kDNS_CERT = 37;
static const uint16 kDNS_DS = 43;
static const uint16 kDNS_RRSIG = 46;
static const uint16 kDNS_DNSKEY = 48;
static const uint16 kDNS_ANY = 0xff;
static const uint16 kDNS_CAA = 257;
static const uint16 kDNS_TESTING = 0xfffe;  // in private use area.

// http://www.iana.org/assignments/dns-sec-alg-numbers/dns-sec-alg-numbers.xhtml
static const uint8 kDNSSEC_RSA_SHA1 = 5;
static const uint8 kDNSSEC_RSA_SHA1_NSEC3 = 7;
static const uint8 kDNSSEC_RSA_SHA256 = 8;
static const uint8 kDNSSEC_RSA_SHA512 = 10;

// RFC 4509
static const uint8 kDNSSEC_SHA1 = 1;
static const uint8 kDNSSEC_SHA256 = 2;

// A Buffer is used for walking over a DNS response packet.
class DnsResponseBuffer {
 public:
  DnsResponseBuffer(const uint8* p, unsigned len)
      : p_(p),
        packet_(p),
        len_(len),
        packet_len_(len) {
  }

  bool U8(uint8* v);
  bool U16(uint16* v);
  bool U32(uint32* v);
  bool Skip(unsigned n);

  bool Block(base::StringPiece* out, unsigned len);

  // DNSName parses a (possibly compressed) DNS name from the packet. If |name|
  // is not NULL, then the name is written into it. See RFC 1035 section 4.1.4.
  bool DNSName(std::string* name);

 private:
  const uint8* p_;
  const uint8* const packet_;
  unsigned len_;
  const unsigned packet_len_;

  DISALLOW_COPY_AND_ASSIGN(DnsResponseBuffer);
};


}  // namespace net

#endif  // NET_BASE_DNS_UTIL_H_
