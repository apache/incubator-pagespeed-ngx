/*
 * Copyright 2012 Google Inc.
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

// Author: matterbury@google.com (Matt Atterbury)

// A set of utility functions for handling character sets/encodings and
// related concepts like byte-order-marks (BOM). Currently the only methods
// relate to BOMs.

#ifndef NET_INSTAWEB_UTIL_PUBLIC_CHARSET_UTIL_H_
#define NET_INSTAWEB_UTIL_PUBLIC_CHARSET_UTIL_H_

#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// The charsets we understand. Currently only those that have BOMs below.
const char kUtf8Charset[]                  = "utf-8";
const char kUtf16BigEndianCharset[]        = "utf-16be";
const char kUtf16LittleEndianCharset[]     = "utf-16le";
const char kUtf32BigEndianCharset[]        = "utf-32be";
const char kUtf32LittleEndianCharset[]     = "utf-32le";

// The Byte-Order-Mark (BOM) for the various UTF encodings.
const char kUtf8Bom[]                  = "\xEF\xBB\xBF";
const char kUtf16BigEndianBom[]        = "\xFE\xFF";
const char kUtf16LittleEndianBom[]     = "\xFF\xFE";
const char kUtf32BigEndianBom[]        = "\x00\x00\xFE\xFF";
const char kUtf32LittleEndianBom[]     = "\xFF\xFE\x00\x00";

// Strips any initial UTF-8 BOM (Byte Order Mark) from the given contents.
// Returns true if a BOM was stripped, false if not.
//
// In addition to specifying the encoding in the ContentType header, one
// can also specify it at the beginning of the file using a Byte Order Mark.
//
// Bytes        Encoding Form
// 00 00 FE FF  UTF-32, big-endian
// FF FE 00 00  UTF-32, little-endian
// FE FF        UTF-16, big-endian
// FF FE        UTF-16, little-endian
// EF BB BF     UTF-8
// See: http://www.unicode.org/faq/utf_bom.html
//
// TODO(nforman): Possibly handle stripping BOMs from non-utf-8 files.
// We currently handle only utf-8 BOM because we assume the resources
// we get are not in utf-16 or utf-32 when we read and parse them, anyway.
bool StripUtf8Bom(StringPiece* contents);

// Return the charset string for the given contents' BOM if any. If the
// contents start with one of the BOMs defined above then the corresponding
// charset is returned, otherwise an empty StringPiece.
const StringPiece GetCharsetForBom(const StringPiece contents);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_CHARSET_UTIL_H_
