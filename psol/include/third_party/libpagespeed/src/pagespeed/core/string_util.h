// Copyright 2010 Google Inc. All Rights Reserved.
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

#ifndef PAGESPEED_CORE_STRING_UTIL_H_
#define PAGESPEED_CORE_STRING_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "base/string_piece.h"
#include "pagespeed/core/compiler_specific.h"

#if defined(_WIN32)
#include "pagespeed/core/string_util_win.h"
#else
#include "pagespeed/core/string_util_posix.h"
#endif

namespace pagespeed {

namespace string_util {

class CaseInsensitiveStringComparator {
 public:
  bool operator()(const std::string& x, const std::string& y) const;
};

typedef std::map<std::string, std::string,
                 CaseInsensitiveStringComparator>
    CaseInsensitiveStringStringMap;

// Return true iff the two strings are equal, ignoring case.
bool StringCaseEqual(const base::StringPiece& s1,
                     const base::StringPiece& s2);
// Return true iff str starts with prefix, ignoring case.
bool StringCaseStartsWith(const base::StringPiece& str,
                          const base::StringPiece& prefix);
// Return true iff str ends with suffix, ignoring case.
bool StringCaseEndsWith(const base::StringPiece& str,
                        const base::StringPiece& suffix);

std::string IntToString(int value);
bool StringToInt(const std::string& input, int* output);
std::string DoubleToString(double value);

std::string JoinString(const std::vector<std::string>& parts, char s);

std::string ReplaceStringPlaceholders(
    const base::StringPiece& format_string,
    const std::vector<std::string>& subst,
    std::vector<size_t>* offsets);


// PRINTF_FORMAT not everywhere.
// Use PRINTF_FORMAT to force compiler to check for
// formatting errors where supported.
std::string StringPrintf(const char* format, ...)
    PRINTF_FORMAT(1, 2);

bool LowerCaseEqualsASCII(const std::string& a, const char* b);

// ASCII-specific tolower.  The standard library's tolower is locale sensitive,
// so we don't want to use it here.
inline char ToLowerASCII(char c) {
  return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

// ASCII-specific toupper.  The standard library's toupper is locale sensitive,
// so we don't want to use it here.
inline char ToUpperASCII(char c) {
  return (c >= 'a' && c <= 'z') ? (c + ('A' - 'a')) : c;
}

struct CaseInsensitiveCompareASCII {
 public:
  bool operator()(char x, char y) const {
    return ToLowerASCII(x) == ToLowerASCII(y);
  }
};

inline void StringToUpperASCII(std::string* s) {
  for (std::string::iterator i = s->begin(); i != s->end(); ++i)
    *i = ToUpperASCII(*i);
}

// Trims any whitespace from either end of the input string.  Returns where
// whitespace was found.
// NOTE: Safe to use the same variable for both input and output.
enum TrimPositions {
  TRIM_NONE     = 0,
  TRIM_LEADING  = 1 << 0,
  TRIM_TRAILING = 1 << 1,
  TRIM_ALL      = TRIM_LEADING | TRIM_TRAILING,
};
void TrimWhitespaceASCII(const std::string& input,
                         TrimPositions positions,
                         std::string* output);

inline bool IsAsciiDigit(char c) {
  return c >= '0' && c <= '9';
}

}  // namespace string_util

}  // namespace pagespeed

#endif  // PAGESPEED_CORE_STRING_UTIL_H_
