// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GOOGLEURL_SRC_URL_CANON_IP_H__
#define GOOGLEURL_SRC_URL_CANON_IP_H__

#include "base/string16.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_common.h"
#include "googleurl/src/url_parse.h"

namespace url_canon {

// Searches the host name for the portions of the IPv4 address. On success,
// each component will be placed into |components| and it will return true.
// It will return false if the host can not be separated as an IPv4 address
// or if there are any non-7-bit characters or other characters that can not
// be in an IP address. (This is important so we fail as early as possible for
// common non-IP hostnames.)
//
// Not all components may exist. If there are only 3 components, for example,
// the last one will have a length of -1 or 0 to indicate it does not exist.
//
// Note that many platform's inet_addr will ignore everything after a space
// in certain curcumstances if the stuff before the space looks like an IP
// address. IE6 is included in this. We do NOT handle this case. In many cases,
// the browser's canonicalization will get run before this which converts
// spaces to %20 (in the case of IE7) or rejects them (in the case of
// Mozilla), so this code path never gets hit. Our host canonicalization will
// notice these spaces and escape them, which will make IP address finding
// fail. This seems like better behavior than stripping after a space.
GURL_API bool FindIPv4Components(const char* spec,
                                 const url_parse::Component& host,
                                 url_parse::Component components[4]);
GURL_API bool FindIPv4Components(const char16* spec,
                                 const url_parse::Component& host,
                                 url_parse::Component components[4]);

// Converts an IPv4 address to a 32-bit number (network byte order).
//
// Possible return values:
//   IPV4    - IPv4 address was successfully parsed.
//   BROKEN  - Input was formatted like an IPv4 address, but overflow occurred
//             during parsing.
//   NEUTRAL - Input couldn't possibly be interpreted as an IPv4 address.
//             It might be an IPv6 address, or a hostname.
//
// On success, |num_ipv4_components| will be populated with the number of
// components in the IPv4 address.
GURL_API CanonHostInfo::Family IPv4AddressToNumber(
    const char* spec,
    const url_parse::Component& host,
    unsigned char address[4],
    int* num_ipv4_components);
GURL_API CanonHostInfo::Family IPv4AddressToNumber(
    const char16* spec,
    const url_parse::Component& host,
    unsigned char address[4],
    int* num_ipv4_components);

// Converts an IPv6 address to a 128-bit number (network byte order), returning
// true on success. False means that the input was not a valid IPv6 address.
//
// NOTE that |host| is expected to be surrounded by square brackets.
// i.e. "[::1]" rather than "::1".
GURL_API bool IPv6AddressToNumber(const char* spec,
                                  const url_parse::Component& host,
                                  unsigned char address[16]);
GURL_API bool IPv6AddressToNumber(const char16* spec,
                                  const url_parse::Component& host,
                                  unsigned char address[16]);

}  // namespace url_canon

#endif  // GOOGLEURL_SRC_URL_CANON_IP_H__
