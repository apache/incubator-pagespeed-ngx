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

// Author: jmarantz@google.com (Joshua Marantz)

#ifndef NET_INSTAWEB_UTIL_PUBLIC_STRING_UTIL_H_
#define NET_INSTAWEB_UTIL_PUBLIC_STRING_UTIL_H_


#include <set>
#include <vector>
#include <string>

#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "third_party/protobuf2/src/src/google/protobuf/stubs/strutil.h"

namespace net_instaweb {

typedef base::StringPiece StringPiece;

inline std::string IntegerToString(int i) {
  return base::IntToString(i);
}

inline bool StringToInt(const char* in, int* out) {
  return base::StringToInt(in, out);
}

inline bool StringToInt(const std::string& in, int* out) {
  return base::StringToInt(in, out);
}

const StringPiece kEmptyString;
std::string StrCat(const StringPiece& a,
                    const StringPiece& b,
                    const StringPiece& c = kEmptyString,
                    const StringPiece& d = kEmptyString,
                    const StringPiece& e = kEmptyString,
                    const StringPiece& f = kEmptyString,
                    const StringPiece& g = kEmptyString,
                    const StringPiece& h = kEmptyString);

void SplitStringPieceToVector(const StringPiece& sp, const char* separator,
                              std::vector<StringPiece>* components,
                              bool omit_empty_strings);

void BackslashEscape(const StringPiece& src,
                     const StringPiece& to_escape,
                     std::string* dest);

inline bool HasPrefixString(const std::string& str,
                            const std::string& prefix) {
  return google::protobuf::HasPrefixString(str, prefix);
}

inline void LowerString(std::string* str) {
  google::protobuf::LowerString(str);
}

inline char* strdup(const char* str) {
  return base::strdup(str);
}

inline int strcasecmp(const char* s1, const char* s2) {
  return base::strcasecmp(s1, s2);
}

inline void TrimWhitespace(const StringPiece& in, std::string* output) {
  static const char whitespace[] = " \r\n\t";
  TrimString(std::string(in.data(), in.size()), whitespace, output);
}

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

struct StringCompareInsensitive {
  bool operator()(const std::string& s1, const std::string& s2) const {
    return strcasecmp(s1.c_str(), s2.c_str()) < 0;
  };
};

typedef std::vector<const char*> CharStarVector;
typedef std::vector<std::string> StringVector;
typedef std::set<std::string> StringSet;

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STRING_UTIL_H_
