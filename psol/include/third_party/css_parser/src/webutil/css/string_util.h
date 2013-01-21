/**
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

// Copyright 2010 Google Inc. All Rights Reserved.
// Author: sligocki@google.com (Shawn Ligocki)
//
// Useful string utils.

#ifndef WEBUTIL_CSS_STRING_UTIL_H_
#define WEBUTIL_CSS_STRING_UTIL_H_

#include "strings/stringpiece.h"

class UnicodeText;

namespace Css {

// Using isascii with signed chars is unfortunately undefined.
inline bool IsAscii(char c) {
  return isascii(static_cast<unsigned char>(c));
}

// Convert a given block of chars to a double.
bool ParseDouble(const char* str, int len, double* dest);

// Lowercase all ASCII chars in the UnicodeText in_text.
// Leaves non-ASCII chars alone.
UnicodeText LowercaseAscii(const UnicodeText& in_text);

bool StringCaseEquals(const StringPiece& a, const StringPiece& b);
// Compare ident's UTF8 bytes with string str ignoring ASCII case differences.
// Meant to be used with static ASCII strings, Ex: StringCaseEquals(id, "foo").
bool StringCaseEquals(const UnicodeText& ident, const StringPiece& str);

}  // namespace Css

#endif  // WEBUTIL_CSS_STRING_UTIL_H_
