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

#include <map>
#include <set>
#include <vector>

#include "base/stringprintf.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"


#include <cstdlib>
#include <string>
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"

using base::StringAppendF;
using base::StringAppendV;
using base::SStringPrintf;
using base::StringPiece;

typedef StringPiece::size_type stringpiece_ssize_type;

// Quick macro to get the size of a static char[] without trailing '\0'.
// Note: Cannot be used for char*, std::string, etc.
#define STATIC_STRLEN(static_string) (arraysize(static_string) - 1)

namespace net_instaweb {

struct StringCompareInsensitive;

typedef std::map<GoogleString, GoogleString> StringStringMap;
typedef std::map<GoogleString, int> StringIntMap;
typedef std::set<GoogleString> StringSet;
typedef std::set<GoogleString, StringCompareInsensitive> StringSetInsensitive;
typedef std::vector<GoogleString> StringVector;
typedef std::vector<StringPiece> StringPieceVector;
typedef std::vector<const GoogleString*> ConstStringStarVector;
typedef std::vector<GoogleString*> StringStarVector;
typedef std::vector<const char*> CharStarVector;

inline GoogleString IntegerToString(int i) {
  return base::IntToString(i);
}

inline GoogleString UintToString(unsigned int i) {
  return base::UintToString(i);
}

inline GoogleString Integer64ToString(int64 i) {
  return base::Int64ToString(i);
}

inline GoogleString PointerToString(void* pointer) {
  return StringPrintf("%p", pointer);
}

// NOTE: For a string of the form "45x", this sets *out = 45 but returns false.
// It sets *out = 0 given "Junk45" or "".
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

inline bool StringToInt(const GoogleString& in, int* out) {
  return base::StringToInt(in, out);
}

inline bool StringToInt64(const GoogleString& in, int64* out) {
  return base::StringToInt64(in, out);
}

// Returns the part of the piece after the first '=', trimming any
// white space found at the beginning or end of the resulting piece.
// Returns an empty string if '=' was not found.
StringPiece PieceAfterEquals(const StringPiece& piece);

class EmptyString {
 public:
  static const StringPiece kEmptyString;
};

// TODO(jmarantz): use overloading instead of default args and get
// rid of this statically constructed global object.
GoogleString StrCat(const StringPiece& a, const StringPiece& b,
                    const StringPiece& c = EmptyString::kEmptyString,
                    const StringPiece& d = EmptyString::kEmptyString,
                    const StringPiece& e = EmptyString::kEmptyString,
                    const StringPiece& f = EmptyString::kEmptyString,
                    const StringPiece& g = EmptyString::kEmptyString,
                    const StringPiece& h = EmptyString::kEmptyString);

void StrAppend(GoogleString* target,
               const StringPiece& a,
               const StringPiece& b = EmptyString::kEmptyString,
               const StringPiece& c = EmptyString::kEmptyString,
               const StringPiece& d = EmptyString::kEmptyString,
               const StringPiece& e = EmptyString::kEmptyString,
               const StringPiece& f = EmptyString::kEmptyString,
               const StringPiece& g = EmptyString::kEmptyString,
               const StringPiece& h = EmptyString::kEmptyString);

// Split sp into pieces that are separated by any character in the given string
// of separators, and push those pieces in order onto components.
void SplitStringPieceToVector(const StringPiece& sp,
                              const StringPiece& separators,
                              StringPieceVector* components,
                              bool omit_empty_strings);

// Splits string 'full' using substr by searching it incrementally from
// left. Empty tokens are removed from the final result.
void SplitStringUsingSubstr(const StringPiece& full,
                            const StringPiece& substr,
                            StringPieceVector* result);

void BackslashEscape(const StringPiece& src,
                     const StringPiece& to_escape,
                     GoogleString* dest);

GoogleString CEscape(const StringPiece& src);

// TODO(jmarantz): Eliminate these definitions of HasPrefixString,
// UpperString, and LowerString, and re-add dependency on protobufs
// which also provide definitions for these.

bool HasPrefixString(const StringPiece& str, const StringPiece& prefix);

void UpperString(GoogleString* str);

void LowerString(GoogleString* str);

inline bool OnlyWhitespace(const GoogleString& str) {
  return ContainsOnlyWhitespaceASCII(str);
}

// Replaces all instances of 'substring' in 's' with 'replacement'.
// Returns the number of instances replaced.  Replacements are not
// subject to re-matching.
//
// NOTE: The string pieces must not overlap 's'.
int GlobalReplaceSubstring(const StringPiece& substring,
                           const StringPiece& replacement,
                           GoogleString* s);

int FindIgnoreCase(StringPiece haystack, StringPiece needle);


// Output a string which is the combination of all values in vector, separated
// by delim. Does not ignore empty strings in vector. So:
// JoinStringStar({"foo", "", "bar"}, ", ") == "foo, , bar". (Pseudocode)
GoogleString JoinStringStar(const ConstStringStarVector& vector,
                            const StringPiece& delim);

GoogleString JoinStringPieces(const StringPieceVector& vector,
                              int start_index, int size,
                              const StringPiece& delim);
inline GoogleString JoinStringPieces(const StringPieceVector& vector,
                                     const StringPiece& delim) {
  return JoinStringPieces(vector, 0, vector.size(), delim);
}

// See also: ./src/third_party/css_parser/src/strings/ascii_ctype.h
// We probably don't want our core string header file to have a
// dependecy on the Google CSS parser, so for now we'll write this here:

// upper-case a single character and return it.
// toupper() changes based on locale.  We don't want this!
inline char UpperChar(char c) {
  if ((c >= 'a') && (c <= 'z')) {
    c += 'A' - 'a';
  }
  return c;
}

// lower-case a single character and return it.
// tolower() changes based on locale.  We don't want this!
inline char LowerChar(char c) {
  if ((c >= 'A') && (c <= 'Z')) {
    c += 'a' - 'A';
  }
  return c;
}

// Check if given character is an HTML (or CSS) space (not the same as isspace,
// and not locale-dependent!).  Note in particular that isspace always includes
// '\v' and HTML does not.  See:
//    http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#space-character
//    http://www.w3.org/TR/CSS21/grammar.html
inline char IsHtmlSpace(char c) {
  return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '\f');
}

inline char* strdup(const char* str) {
  return base::strdup(str);
}

// Case-insensitive string comparison that is locale-independent.
int StringCaseCompare(const StringPiece& s1, const StringPiece& s2);

// Determines whether the character is a US Ascii number or letter.  This
// is preferable to isalnum() for working with computer languages, as
// opposed to human languages.
inline bool IsAsciiAlphaNumeric(char ch) {
  return (((ch >= 'a') && (ch <= 'z')) ||
          ((ch >= 'A') && (ch <= 'Z')) ||
          ((ch >= '0') && (ch <= '9')));
}

// In-place removal of leading and trailing HTML whitespace.  Returns true if
// any whitespace was trimmed.
bool TrimWhitespace(StringPiece* str);

// In-place removal of leading and trailing quote.
void TrimQuote(StringPiece* str);

// Trims leading HTML whitespace.  Returns true if any whitespace was trimmed.
bool TrimLeadingWhitespace(StringPiece* str);

// Trims trailing HTML whitespace.  Returns true if any whitespace was trimmed.
bool TrimTrailingWhitespace(StringPiece* str);

// Non-destructive TrimWhitespace.
inline void TrimWhitespace(const StringPiece& in, GoogleString* output) {
  StringPiece temp(in);   // Mutable copy
  TrimWhitespace(&temp);  // Modifies temp
  temp.CopyToString(output);
}

// Accumulates a decimal value from 'c' into *value.
// Returns false and leaves *value unchanged if c is not a decimal digit.
bool AccumulateDecimalValue(char c, uint32* value);

// Accumulates a hex value from 'c' into *value
// Returns false and leaves *value unchanged if c is not a hex digit.
bool AccumulateHexValue(char c, uint32* value);

// Return true iff the two strings are equal, ignoring case.
bool StringCaseEqual(const StringPiece& s1, const StringPiece& s2);
// Return true iff str starts with prefix, ignoring case.
bool StringCaseStartsWith(const StringPiece& str, const StringPiece& prefix);
// Return true iff str ends with suffix, ignoring case.
bool StringCaseEndsWith(const StringPiece& str, const StringPiece& suffix);

// Return true if str is equal to the concatenation of first and second. Note
// that this respects case.
bool StringEqualConcat(const StringPiece& str, const StringPiece& first,
                       const StringPiece& second);

struct CharStarCompareInsensitive {
  bool operator()(const char* s1, const char* s2) const {
    return (StringCaseCompare(s1, s2) < 0);
  };
};

struct CharStarCompareSensitive {
  bool operator()(const char* s1, const char* s2) const {
    return (strcmp(s1, s2) < 0);
  }
};

struct StringCompareSensitive {
  bool operator()(const GoogleString& s1, const GoogleString& s2) const {
    return (strcmp(s1.c_str(), s2.c_str()) < 0);
  };
};

struct StringCompareInsensitive {
  bool operator()(const GoogleString& s1, const GoogleString& s2) const {
    return (StringCaseCompare(s1, s2) < 0);
  };
};

// Does a path end in slash?
inline bool EndsInSlash(const StringPiece& path) {
  return path.ends_with("/");
}

// Make sure directory's path ends in '/'.
inline void EnsureEndsInSlash(GoogleString* dir) {
  if (!EndsInSlash(*dir)) {
    dir->append("/");
  }
}

// Given a string such as:  a b "c d" e 'f g'
// Parse it into a vector:  ["a", "b", "c d", "e", "f g"]
// NOTE: actually used for html doctype recognition,
// so assumes HtmlSpace separation.
void ParseShellLikeString(const StringPiece& input,
                          std::vector<GoogleString>* output);

// Counts the number of times that substring appears in text
// Note: for a substring that can overlap itself, it counts not necessarily
// disjoint occurrences of the substring.
// For example: "aaa" appears in "aaaaa" 3 times, not once
int CountSubstring(const StringPiece& text, const StringPiece& substring);

// Returns true if the string contains a character that is not legal
// in an http header.
bool HasIllicitTokenCharacter(const StringPiece& str);

// Appends new empty string to a StringVector and returns a pointer to it.
inline GoogleString* StringVectorAdd(StringVector* v) {
  v->push_back(GoogleString());
  return &v->back();
}


}  // namespace net_instaweb

#endif  // NET_INSTAWEB_UTIL_PUBLIC_STRING_UTIL_H_
