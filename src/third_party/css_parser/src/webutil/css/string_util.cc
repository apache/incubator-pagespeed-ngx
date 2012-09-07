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

#include "webutil/css/string_util.h"

#include <cerrno>
#include <cstdlib>  // strtod
#include <cstring>  // memcpy

#include "strings/ascii_ctype.h"  // ascii_tolower
#include "strings/memutil.h"
#include "util/utf8/public/unicodetext.h"

namespace Css {

// Addapted from RE2::Arg::parse_double() from the Regular Expressions package
// RE2 (http://code.google.com/p/re2/).
bool ParseDouble(const char* str, int len, double* dest) {
  static const int kMaxLength = 200;
  if (dest == NULL || len == 0 || len >= kMaxLength) {
    return false;
  }
  char buf[kMaxLength];
  memcpy(buf, str, len);
  buf[len] = '\0';

  char* end;
  errno = 0;
  *dest = strtod(buf, &end);
  if (errno != 0 || end != buf + len) {
    return false;
  }
  return true;
}

namespace {

inline bool IsAscii(char32 c) {
  return c < 0x80 && c >= 0;
}

}  // namespace

UnicodeText LowercaseAscii(const UnicodeText& in_text) {
  UnicodeText out_text;
  // TODO(sligocki): out_text.reserve(in_text.utf8_length())
  for (UnicodeText::const_iterator iter = in_text.begin();
       iter < in_text.end(); ++iter) {
    char32 c = *iter;
    if (IsAscii(c)) {
      out_text.push_back(ascii_tolower(c));
    } else {
      out_text.push_back(c);
    }
  }
  return out_text;
}

bool StringCaseEquals(const StringPiece& a, const StringPiece& b) {
  return (a.size() == b.size() &&
          (memcasecmp(a.data(), b.data(), a.size()) == 0));
}

bool StringCaseEquals(const UnicodeText& ident, const StringPiece& str) {
  return (ident.utf8_length() == str.size() &&
          (memcasecmp(str.data(), ident.utf8_data(), str.size()) == 0));
}

}  // namespace Css
