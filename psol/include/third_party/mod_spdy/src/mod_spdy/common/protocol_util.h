// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MOD_SPDY_COMMON_PROTOCOL_UTIL_H_
#define MOD_SPDY_COMMON_PROTOCOL_UTIL_H_

#include "base/string_piece.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"

namespace mod_spdy {

namespace http {

// HTTP header names.  These values are all lower-case, so they can be used
// directly in SPDY header blocks.
extern const char* const kAcceptEncoding;
extern const char* const kConnection;
extern const char* const kContentLength;
extern const char* const kContentType;
extern const char* const kHost;
extern const char* const kKeepAlive;
extern const char* const kProxyConnection;
extern const char* const kReferer;
extern const char* const kTransferEncoding;
extern const char* const kXAssociatedContent;
extern const char* const kXModSpdy;

// HTTP header values.
extern const char* const kChunked;
extern const char* const kGzipDeflate;

}  // namespace http

namespace spdy {

// Magic header names for SPDY v2.
extern const char* const kSpdy2Method;
extern const char* const kSpdy2Scheme;
extern const char* const kSpdy2Status;
extern const char* const kSpdy2Url;
extern const char* const kSpdy2Version;

// Magic header names for SPDY v3.
extern const char* const kSpdy3Host;
extern const char* const kSpdy3Method;
extern const char* const kSpdy3Path;
extern const char* const kSpdy3Scheme;
extern const char* const kSpdy3Status;
extern const char* const kSpdy3Version;

}  // namespace spdy

// Convert various SPDY enum types to strings.
const char* GoAwayStatusCodeToString(net::SpdyGoAwayStatus status);
inline const char* RstStreamStatusCodeToString(net::SpdyStatusCodes status) {
  return net::SpdyFramer::StatusCodeToString(status);
}
const char* SettingsIdToString(net::SpdySettingsIds id);

// Return a view of the raw bytes of the frame.
base::StringPiece FrameData(const net::SpdyFrame& frame);

// Return true if this header is forbidden in SPDY responses (ignoring case).
bool IsInvalidSpdyResponseHeader(base::StringPiece key);

// Return the SpdyPriority representing the least important priority for the
// given SPDY version.  For SPDY v2 and below, it's 3; for SPDY v3 and above,
// it's 7.  (The most important SpdyPriority is always 0.)
net::SpdyPriority LowestSpdyPriorityForVersion(int spdy_version);

// Add a header to a header table, lower-casing and merging if necessary.
void MergeInHeader(base::StringPiece key, base::StringPiece value,
                   net::SpdyHeaderBlock* headers);

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_PROTOCOL_UTIL_H_
