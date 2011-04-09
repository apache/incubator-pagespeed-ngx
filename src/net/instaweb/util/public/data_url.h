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

// Author: jmaessen@google.com (Jan Maessen)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_DATA_URL_H_
#define NET_INSTAWEB_UTIL_PUBLIC_DATA_URL_H_

#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

enum Encoding {
  UNKNOWN,  // Used only for output of ParseDataUrl.
  BASE64,
//   LATIN1,  // TODO(jmaessen): implement non-BASE64 encodings.
//   UTF8,
  PLAIN
};

// Create a data: url from the given content-type and content.  See:
// http://en.wikipedia.org/wiki/Data_URI_scheme
//
// The ENCODING indicates how to encode the content; for binary data
// this is UTF8, for ascii / Latin1 it's LATIN1.  If you have ascii
// without high bits or NULs, use LATIN1.  If you have alphanumeric data,
// use PLAIN (which doesn't encode at all).
//
// Note in particular that IE<=7 does not support this, so it makes us
// UserAgent-dependent.  It also pretty much requires outgoing content to be
// compressed as we tend to base64-encode the content.
void DataUrl(const ContentType& content_type, const Encoding encoding,
             const StringPiece& content, GoogleString* result);

// Dismantle a data: url into its component pieces, but do not decode the
// content.  Note that encoded_content will be a substring of the input url and
// shares its lifetime.  Invalidates all outputs if url does not parse.
bool ParseDataUrl(const StringPiece& url,
                  const ContentType** content_type,
                  Encoding* encoding,
                  StringPiece* encoded_content);

bool DecodeDataUrlContent(Encoding encoding,
                          const StringPiece& encoded_content,
                          GoogleString* decoded_content);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_DATA_URL_H_
