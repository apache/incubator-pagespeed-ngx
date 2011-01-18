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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STRING_UTIL_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STRING_UTIL_H_


#include <set>
#include <vector>
#include <string>

#include <stdlib.h>
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"

namespace net_instaweb {

typedef base::StringPiece StringPiece;

inline std::string IntegerToString(int i) {
  return base::IntToString(i);
}

inline std::string Integer64ToString(int64 i) {
  return base::Int64ToString(i);
}

inline bool StringToInt(const char* in, int* out) {
  // TODO(bmcquade): Use char*-based StringToInt once we sync the
  // Chromium repository.
  std::string str(in);
  return base::StringToInt(str, out);
}

inline bool StringToInt64(const char* in, int64* out) {
  // TODO(bmcquade): Use char*-based StringToInt64 once we sync the
  // Chromium repository.
  std::string str(in);
  return base::StringToInt64(str, out);
}

inline bool StringToInt(const std::string& in, int* out) {
  return base::StringToInt(in, out);
}

inline bool StringToInt64(const std::string& in, int64* out) {
  return base::StringToInt64(in, out);
}

class EmptyString {
 public:
  static const StringPiece kEmptyString;
};

// TODO(jmarantz): use overloading instead of default args and get
// rid of this statically constructed global object.
std::string StrCat(const StringPiece& a, const StringPiece& b,
                    const StringPiece& c = EmptyString::kEmptyString,
                    const StringPiece& d = EmptyString::kEmptyString,
                    const StringPiece& e = EmptyString::kEmptyString,
                    const StringPiece& f = EmptyString::kEmptyString,
                    const StringPiece& g = EmptyString::kEmptyString,
                    const StringPiece& h = EmptyString::kEmptyString);

void SplitStringPieceToVector(const StringPiece& sp, const char* separator,
                              std::vector<StringPiece>* components,
                              bool omit_empty_strings);

void BackslashEscape(const StringPiece& src,
                     const StringPiece& to_escape,
                     std::string* dest);

bool HasPrefixString(const StringPiece& str, const StringPiece& prefix);

void LowerString(std::string* str);

inline bool OnlyWhitespace(const std::string& str) {
  return ContainsOnlyWhitespaceASCII(str);
}

inline char* strdup(const char* str) {
  return base::strdup(str);
}

inline int strcasecmp(const char* s1, const char* s2) {
  return base::strcasecmp(s1, s2);
}

inline int strncasecmp(const char* s1, const char* s2, size_t count) {
  return base::strncasecmp(s1, s2, count);
}

inline void TrimWhitespace(const StringPiece& in, std::string* output) {
  static const char whitespace[] = " \r\n\t";
  TrimString(std::string(in.data(), in.size()), whitespace, output);
}

// Accumulates a decimal value from 'c' into *value.
// Returns false and leaves *value unchanged if c is not a decimal digit.
bool AccumulateDecimalValue(char c, int* value);

// Accumulates a hex value from 'c' into *value
// Returns false and leaves *value unchanged if c is not a hex digit.
bool AccumulateHexValue(char c, int* value);

// Return true iff the two strings are equal, ignoring case.
bool StringCaseEqual(const StringPiece& s1, const StringPiece& s2);
// Return true iff str starts with prefix, ignoring case.
bool StringCaseStartsWith(const StringPiece& str, const StringPiece& prefix);
// Return true iff str ends with suffix, ignoring case.
bool StringCaseEndsWith(const StringPiece& str, const StringPiece& suffix);

struct CharStarCompareInsensitive {
  bool operator()(const char* s1, const char* s2) const {
    return strcasecmp(s1, s2) < 0;
  };
};

struct CharStarCompareSensitive {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }
};

struct StringCompareSensitive {
  bool operator()(const std::string& s1, const std::string& s2) const {
    return strcmp(s1.c_str(), s2.c_str()) < 0;
  };
};

struct StringCompareInsensitive {
  bool operator()(const std::string& s1, const std::string& s2) const {
    return strcasecmp(s1.c_str(), s2.c_str()) < 0;
  };
};

typedef std::vector<const char*> CharStarVector;
typedef std::vector<std::string> StringVector;
typedef std::set<std::string> StringSet;

// Does a path end in slash?
inline bool EndsInSlash(const StringPiece& path) {
  return path.ends_with("/");
}

// Make sure directory's path ends in '/'.
inline void EnsureEndsInSlash(std::string* dir) {
  if (!EndsInSlash(*dir)) {
    dir->append("/");
  }
}

// Given a string such as:  a b "c d" e 'f g'
// Parse it into a vector:  ["a", "b", "c d", "e", "f g"]
void ParseShellLikeString(const StringPiece& input,
                          std::vector<std::string>* output);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STRING_UTIL_H_
