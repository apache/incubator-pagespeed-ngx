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

#include "net/instaweb/util/public/charset_util.h"

#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

bool StripUtf8Bom(StringPiece* contents) {
  bool result = false;
  StringPiece bom;
  bom.set(kUtf8Bom, STATIC_STRLEN(kUtf8Bom));
  if (contents->starts_with(bom)) {
    contents->remove_prefix(bom.length());
    result = true;
  }
  return result;
}

const StringPiece GetCharsetForBom(const StringPiece contents) {
  // Bad/empty data?
  if (contents == NULL || contents.length() == 0) {
    return StringPiece();
  }
  // If it starts with a printable ASCII character it can't have a BOM, and
  // since that's the most common case and the comparisons below are relatively
  // expensive, test this here for early exit.
  if (contents[0] >= ' ' && contents[0] <= '~') {
    return StringPiece();
  }

  // Check for the BOMs we know about. Since some BOMs contain NUL(s) we have
  // to use STATIC_STRLEN and manual StringPiece construction.
  StringPiece bom;
  bom.set(kUtf8Bom, STATIC_STRLEN(kUtf8Bom));
  if (contents.starts_with(bom)) {
    return kUtf8Charset;
  }
  bom.set(kUtf16BigEndianBom, STATIC_STRLEN(kUtf16BigEndianBom));
  if (contents.starts_with(bom)) {
    return kUtf16BigEndianCharset;
  }
  // UTF-16LE's BOM is a leading substring of UTF-32LE's BOM, so we must
  // check the longer one first. All the others have unique prefixes.
  bom.set(kUtf32LittleEndianBom, STATIC_STRLEN(kUtf32LittleEndianBom));
  if (contents.starts_with(bom)) {
    return kUtf32LittleEndianCharset;
  }
  bom.set(kUtf16LittleEndianBom, STATIC_STRLEN(kUtf16LittleEndianBom));
  if (contents.starts_with(bom)) {
    return kUtf16LittleEndianCharset;
  }
  bom.set(kUtf32BigEndianBom, STATIC_STRLEN(kUtf32BigEndianBom));
  if (contents.starts_with(bom)) {
    return kUtf32BigEndianCharset;
  }

  return StringPiece();
}

}  // namespace net_instaweb
