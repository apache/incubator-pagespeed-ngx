/*
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_BASE64_UTIL_H_
#define NET_INSTAWEB_UTIL_PUBLIC_BASE64_UTIL_H_

#include "net/instaweb/util/public/string.h"

#include "base/string_piece.h"
#include "third_party/base64/base64.h"

namespace net_instaweb {

typedef base::StringPiece StringPiece;

inline void Web64Encode(const StringPiece& in, GoogleString* out) {
  *out = web64_encode(reinterpret_cast<const unsigned char*>(in.data()),
                      in.size());
}

inline bool Web64Decode(const StringPiece& in, GoogleString* out) {
  bool ret = web64_decode(in.as_string(), out);
  return ret;
}

inline void Mime64Encode(const StringPiece& in, GoogleString* out) {
  *out = base64_encode(reinterpret_cast<const unsigned char*>(in.data()),
                      in.size());
}

inline bool Mime64Decode(const StringPiece& in, GoogleString* out) {
  bool ret = base64_decode(in.as_string(), out);
  return ret;
}

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_BASE64_UTIL_H_
