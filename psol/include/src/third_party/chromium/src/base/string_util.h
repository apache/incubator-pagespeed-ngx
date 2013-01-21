// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for working with strings.

#ifndef BASE_STRING_UTIL_H_
#define BASE_STRING_UTIL_H_
#pragma once

#include <stdarg.h>   // va_list

#include <string>
#include <vector>

#include "base/base_api.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "base/string_piece.h"  // For implicit conversions.

// TODO(brettw) remove this dependency. Previously StringPrintf lived in this
// file. We need to convert the callers over to using stringprintf.h instead
// and then remove this.
#include "base/stringprintf.h"

// Safe standard library wrappers for all platforms.

namespace base {

// C standard-library functions like "strncasecmp" and "snprintf" that aren't
// cross-platform are provided as "base::strncasecmp", and their prototypes
// are listed below.  These functions are then implemented as inline calls
// to the platform-specific equivalents in the platform-specific headers.

// Compares the two strings s1 and s2 without regard to case using
// the current locale; returns 0 if they are equal, 1 if s1 > s2, and -1 if
// s2 > s1 according to a lexicographic comparison.
int strcasecmp(const char* s1, const char* s2);

// Compares up to count characters of s1 and s2 without regard to case using
// the current locale; returns 0 if they are equal, 1 if s1 > s2, and -1 if
// s2 > s1 according to a lexicographic comparison.
int strncasecmp(const char* s1, const char* s2, size_t count);

// Same as strncmp but for char16 strings.
int strncmp16(const char16* s1, const char16* s2, size_t count);

// Wrapper for vsnprintf that always null-terminates and always returns the
// number of characters that would be in an untruncated formatted
// string, even when truncation occurs.
int vsnprintf(char* buffer, size_t size, const char* format, va_list arguments)
    PRINTF_FORMAT(3, 0);

// vswprintf always null-terminates, but when truncation occurs, it will either
// return -1 or the number of characters that would be in an untruncated
// formatted string.  The actual return value depends on the underlying
// C library's vswprintf implementation.
int vswprintf(wchar_t* buffer, size_t size,
              const wchar_t* format, va_list arguments)
    WPRINTF_FORMAT(3, 0);

// Some of these implementations need to be inlined.

// We separate the declaration from the implementation of this inline
// function just so the PRINTF_FORMAT works.
inline int snprintf(char* buffer, size_t size, const char* format, ...)
    PRINTF_FORMAT(3, 4);
inline int snprintf(char* buffer, size_t size, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = vsnprintf(buffer, size, format, arguments);
  va_end(arguments);
  return result;
}

// We separate the declaration from the implementation of this inline
// function just so the WPRINTF_FORMAT works.
inline int swprintf(wchar_t* buffer, size_t size, const wchar_t* format, ...)
    WPRINTF_FORMAT(3, 4);
inline int swprintf(wchar_t* buffer, size_t size, const wchar_t* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = vswprintf(buffer, size, format, arguments);
  va_end(arguments);
  return result;
}

// BSD-style safe and consistent string copy functions.
// Copies |src| to |dst|, where |dst_size| is the total allocated size of |dst|.
// Copies at most |dst_size|-1 characters, and always NULL terminates |dst|, as
// long as |dst_size| is not 0.  Returns the length of |src| in characters.
// If the return value is >= dst_size, then the output was truncated.
// NOTE: All sizes are in number of characters, NOT in bytes.
BASE_API size_t strlcpy(char* dst, const char* src, size_t dst_size);
BASE_API size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t dst_size);

// Scan a wprintf format string to determine whether it's portable across a
// variety of systems.  This function only checks that the conversion
// specifiers used by the format string are supported and have the same meaning
// on a variety of systems.  It doesn't check for other errors that might occur
// within a format string.
//
// Nonportable conversion specifiers for wprintf are:
//  - 's' and 'c' without an 'l' length modifier.  %s and %c operate on char
//     data on all systems except Windows, which treat them as wchar_t data.
//     Use %ls and %lc for wchar_t data instead.
//  - 'S' and 'C', which operate on wchar_t data on all systems except Windows,
//     which treat them as char data.  Use %ls and %lc for wchar_t data
//     instead.
//  - 'F', which is not identified by Windows wprintf documentation.
//  - 'D', 'O', and 'U', which are deprecated and not available on all systems.
//     Use %ld, %lo, and %lu instead.
//
// Note that there is no portable conversion specifier for char data when
// working with wprintf.
//
// This function is intended to be called from base::vswprintf.
BASE_API bool IsWprintfFormatPortable(const wchar_t* format);

// ASCII-specific tolower.  The standard library's tolower is locale sensitive,
// so we don't want to use it here.
template <class Char> inline Char ToLowerASCII(Char c) {
  return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

// ASCII-specific toupper.  The standard library's toupper is locale sensitive,
// so we don't want to use it here.
template <class Char> inline Char ToUpperASCII(Char c) {
  return (c >= 'a' && c <= 'z') ? (c + ('A' - 'a')) : c;
}

// Function objects to aid in comparing/searching strings.

template<typename Char> struct CaseInsensitiveCompare {
 public:
  bool operator()(Char x, Char y) const {
    // TODO(darin): Do we really want to do locale sensitive comparisons here?
    // See http://crbug.com/24917
    return tolower(x) == tolower(y);
  }
};

template<typename Char> struct CaseInsensitiveCompareASCII {
 public:
  bool operator()(Char x, Char y) const {
    return ToLowerASCII(x) == ToLowerASCII(y);
  }
};

}  // namespace base

#if defined(OS_WIN)
#include "base/string_util_win.h"
#elif defined(OS_POSIX)
#include "base/string_util_posix.h"
#else
#error Define string operations appropriately for your platform
#endif

// These threadsafe functions return references to globally unique empty
// strings.
//
// DO NOT USE THESE AS A GENERAL-PURPOSE SUBSTITUTE FOR DEFAULT CONSTRUCTORS.
// There is only one case where you should use these: functions which need to
// return a string by reference (e.g. as a class member accessor), and don't
// have an empty string to use (e.g. in an error case).  These should not be
// used as initializers, function arguments, or return values for functions
// which return by value or outparam.
BASE_API const std::string& EmptyString();
BASE_API const std::wstring& EmptyWString();
BASE_API const string16& EmptyString16();

BASE_API extern const wchar_t kWhitespaceWide[];
BASE_API extern const char16 kWhitespaceUTF16[];
BASE_API extern const char kWhitespaceASCII[];

BASE_API extern const char kUtf8ByteOrderMark[];

// Removes characters in remove_chars from anywhere in input.  Returns true if
// any characters were removed.
// NOTE: Safe to use the same variable for both input and output.
BASE_API bool RemoveChars(const string16& input,
                          const char16 remove_chars[],
                          string16* output);
BASE_API bool RemoveChars(const std::string& input,
                          const char remove_chars[],
                          std::string* output);

// Removes characters in trim_chars from the beginning and end of input.
// NOTE: Safe to use the same variable for both input and output.
BASE_API bool TrimString(const std::wstring& input,
                         const wchar_t trim_chars[],
                         std::wstring* output);
BASE_API bool TrimString(const string16& input,
                         const char16 trim_chars[],
                         string16* output);
BASE_API bool TrimString(const std::string& input,
                         const char trim_chars[],
                         std::string* output);

// Truncates a string to the nearest UTF-8 character that will leave
// the string less than or equal to the specified byte size.
BASE_API void TruncateUTF8ToByteSize(const std::string& input,
                                     const size_t byte_size,
                                     std::string* output);

// Trims any whitespace from either end of the input string.  Returns where
// whitespace was found.
// The non-wide version has two functions:
// * TrimWhitespaceASCII()
//   This function is for ASCII strings and only looks for ASCII whitespace;
// Please choose the best one according to your usage.
// NOTE: Safe to use the same variable for both input and output.
enum TrimPositions {
  TRIM_NONE     = 0,
  TRIM_LEADING  = 1 << 0,
  TRIM_TRAILING = 1 << 1,
  TRIM_ALL      = TRIM_LEADING | TRIM_TRAILING,
};
BASE_API TrimPositions TrimWhitespace(const string16& input,
                                      TrimPositions positions,
                                      string16* output);
BASE_API TrimPositions TrimWhitespaceASCII(const std::string& input,
                                           TrimPositions positions,
                                           std::string* output);

// Deprecated. This function is only for backward compatibility and calls
// TrimWhitespaceASCII().
BASE_API TrimPositions TrimWhitespace(const std::string& input,
                                      TrimPositions positions,
                                      std::string* output);

// Searches  for CR or LF characters.  Removes all contiguous whitespace
// strings that contain them.  This is useful when trying to deal with text
// copied from terminals.
// Returns |text|, with the following three transformations:
// (1) Leading and trailing whitespace is trimmed.
// (2) If |trim_sequences_with_line_breaks| is true, any other whitespace
//     sequences containing a CR or LF are trimmed.
// (3) All other whitespace sequences are converted to single spaces.
BASE_API std::wstring CollapseWhitespace(const std::wstring& text,
                                         bool trim_sequences_with_line_breaks);
BASE_API string16 CollapseWhitespace(const string16& text,
                                     bool trim_sequences_with_line_breaks);
BASE_API std::string CollapseWhitespaceASCII(
    const std::string& text, bool trim_sequences_with_line_breaks);

// Returns true if the passed string is empty or contains only white-space
// characters.
BASE_API bool ContainsOnlyWhitespaceASCII(const std::string& str);
BASE_API bool ContainsOnlyWhitespace(const string16& str);

// Returns true if |input| is empty or contains only characters found in
// |characters|.
BASE_API bool ContainsOnlyChars(const std::wstring& input,
                                const std::wstring& characters);
BASE_API bool ContainsOnlyChars(const string16& input,
                                const string16& characters);
BASE_API bool ContainsOnlyChars(const std::string& input,
                                const std::string& characters);

// Converts to 7-bit ASCII by truncating. The result must be known to be ASCII
// beforehand.
BASE_API std::string WideToASCII(const std::wstring& wide);
BASE_API std::string UTF16ToASCII(const string16& utf16);

// Converts the given wide string to the corresponding Latin1. This will fail
// (return false) if any characters are more than 255.
BASE_API bool WideToLatin1(const std::wstring& wide, std::string* latin1);

// Returns true if the specified string matches the criteria. How can a wide
// string be 8-bit or UTF8? It contains only characters that are < 256 (in the
// first case) or characters that use only 8-bits and whose 8-bit
// representation looks like a UTF-8 string (the second case).
//
// Note that IsStringUTF8 checks not only if the input is structurally
// valid but also if it doesn't contain any non-character codepoint
// (e.g. U+FFFE). It's done on purpose because all the existing callers want
// to have the maximum 'discriminating' power from other encodings. If
// there's a use case for just checking the structural validity, we have to
// add a new function for that.
BASE_API bool IsStringUTF8(const std::string& str);
BASE_API bool IsStringASCII(const std::wstring& str);
BASE_API bool IsStringASCII(const base::StringPiece& str);
BASE_API bool IsStringASCII(const string16& str);

// Converts the elements of the given string.  This version uses a pointer to
// clearly differentiate it from the non-pointer variant.
template <class str> inline void StringToLowerASCII(str* s) {
  for (typename str::iterator i = s->begin(); i != s->end(); ++i)
    *i = base::ToLowerASCII(*i);
}

template <class str> inline str StringToLowerASCII(const str& s) {
  // for std::string and std::wstring
  str output(s);
  StringToLowerASCII(&output);
  return output;
}

// Converts the elements of the given string.  This version uses a pointer to
// clearly differentiate it from the non-pointer variant.
template <class str> inline void StringToUpperASCII(str* s) {
  for (typename str::iterator i = s->begin(); i != s->end(); ++i)
    *i = base::ToUpperASCII(*i);
}

template <class str> inline str StringToUpperASCII(const str& s) {
  // for std::string and std::wstring
  str output(s);
  StringToUpperASCII(&output);
  return output;
}

// Compare the lower-case form of the given string against the given ASCII
// string.  This is useful for doing checking if an input string matches some
// token, and it is optimized to avoid intermediate string copies.  This API is
// borrowed from the equivalent APIs in Mozilla.
BASE_API bool LowerCaseEqualsASCII(const std::string& a, const char* b);
BASE_API bool LowerCaseEqualsASCII(const std::wstring& a, const char* b);
BASE_API bool LowerCaseEqualsASCII(const string16& a, const char* b);

// Same thing, but with string iterators instead.
BASE_API bool LowerCaseEqualsASCII(std::string::const_iterator a_begin,
                                   std::string::const_iterator a_end,
                                   const char* b);
BASE_API bool LowerCaseEqualsASCII(std::wstring::const_iterator a_begin,
                                   std::wstring::const_iterator a_end,
                                   const char* b);
BASE_API bool LowerCaseEqualsASCII(string16::const_iterator a_begin,
                                   string16::const_iterator a_end,
                                   const char* b);
BASE_API bool LowerCaseEqualsASCII(const char* a_begin,
                                   const char* a_end,
                                   const char* b);
BASE_API bool LowerCaseEqualsASCII(const wchar_t* a_begin,
                                   const wchar_t* a_end,
                                   const char* b);
BASE_API bool LowerCaseEqualsASCII(const char16* a_begin,
                                   const char16* a_end,
                                   const char* b);

// Performs a case-sensitive string compare. The behavior is undefined if both
// strings are not ASCII.
BASE_API bool EqualsASCII(const string16& a, const base::StringPiece& b);

// Returns true if str starts with search, or false otherwise.
BASE_API bool StartsWithASCII(const std::string& str,
                              const std::string& search,
                              bool case_sensitive);
BASE_API bool StartsWith(const std::wstring& str,
                         const std::wstring& search,
                         bool case_sensitive);
BASE_API bool StartsWith(const string16& str,
                         const string16& search,
                         bool case_sensitive);

// Returns true if str ends with search, or false otherwise.
BASE_API bool EndsWith(const std::string& str,
                       const std::string& search,
                       bool case_sensitive);
BASE_API bool EndsWith(const std::wstring& str,
                       const std::wstring& search,
                       bool case_sensitive);
BASE_API bool EndsWith(const string16& str,
                       const string16& search,
                       bool case_sensitive);


// Determines the type of ASCII character, independent of locale (the C
// library versions will change based on locale).
template <typename Char>
inline bool IsAsciiWhitespace(Char c) {
  return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}
template <typename Char>
inline bool IsAsciiAlpha(Char c) {
  return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'));
}
template <typename Char>
inline bool IsAsciiDigit(Char c) {
  return c >= '0' && c <= '9';
}

template <typename Char>
inline bool IsHexDigit(Char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

template <typename Char>
inline Char HexDigitToInt(Char c) {
  DCHECK(IsHexDigit(c));
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return 0;
}

// Returns true if it's a whitespace character.
inline bool IsWhitespace(wchar_t c) {
  return wcschr(kWhitespaceWide, c) != NULL;
}

// Return a byte string in human-readable format with a unit suffix. Not
// appropriate for use in any UI; use of FormatBytes and friends in ui/base is
// highly recommended instead. TODO(avi): Figure out how to get callers to use
// FormatBytes instead; remove this.
BASE_API string16 FormatBytesUnlocalized(int64 bytes);

// Starting at |start_offset| (usually 0), replace the first instance of
// |find_this| with |replace_with|.
BASE_API void ReplaceFirstSubstringAfterOffset(string16* str,
                                               string16::size_type start_offset,
                                               const string16& find_this,
                                               const string16& replace_with);
BASE_API void ReplaceFirstSubstringAfterOffset(
    std::string* str,
    std::string::size_type start_offset,
    const std::string& find_this,
    const std::string& replace_with);

// Starting at |start_offset| (usually 0), look through |str| and replace all
// instances of |find_this| with |replace_with|.
//
// This does entire substrings; use std::replace in <algorithm> for single
// characters, for example:
//   std::replace(str.begin(), str.end(), 'a', 'b');
BASE_API void ReplaceSubstringsAfterOffset(string16* str,
                                           string16::size_type start_offset,
                                           const string16& find_this,
                                           const string16& replace_with);
BASE_API void ReplaceSubstringsAfterOffset(std::string* str,
                                           std::string::size_type start_offset,
                                           const std::string& find_this,
                                           const std::string& replace_with);

// This is mpcomplete's pattern for saving a string copy when dealing with
// a function that writes results into a wchar_t[] and wanting the result to
// end up in a std::wstring.  It ensures that the std::wstring's internal
// buffer has enough room to store the characters to be written into it, and
// sets its .length() attribute to the right value.
//
// The reserve() call allocates the memory required to hold the string
// plus a terminating null.  This is done because resize() isn't
// guaranteed to reserve space for the null.  The resize() call is
// simply the only way to change the string's 'length' member.
//
// XXX-performance: the call to wide.resize() takes linear time, since it fills
// the string's buffer with nulls.  I call it to change the length of the
// string (needed because writing directly to the buffer doesn't do this).
// Perhaps there's a constant-time way to change the string's length.
template <class string_type>
inline typename string_type::value_type* WriteInto(string_type* str,
                                                   size_t length_with_null) {
  str->reserve(length_with_null);
  str->resize(length_with_null - 1);
  return &((*str)[0]);
}

//-----------------------------------------------------------------------------

// Splits a string into its fields delimited by any of the characters in
// |delimiters|.  Each field is added to the |tokens| vector.  Returns the
// number of tokens found.
BASE_API size_t Tokenize(const std::wstring& str,
                         const std::wstring& delimiters,
                         std::vector<std::wstring>* tokens);
BASE_API size_t Tokenize(const string16& str,
                         const string16& delimiters,
                         std::vector<string16>* tokens);
BASE_API size_t Tokenize(const std::string& str,
                         const std::string& delimiters,
                         std::vector<std::string>* tokens);
BASE_API size_t Tokenize(const base::StringPiece& str,
                         const base::StringPiece& delimiters,
                         std::vector<base::StringPiece>* tokens);

// Does the opposite of SplitString().
BASE_API string16 JoinString(const std::vector<string16>& parts, char16 s);
BASE_API std::string JoinString(const std::vector<std::string>& parts, char s);

// Replace $1-$2-$3..$9 in the format string with |a|-|b|-|c|..|i| respectively.
// Additionally, any number of consecutive '$' characters is replaced by that
// number less one. Eg $$->$, $$$->$$, etc. The offsets parameter here can be
// NULL. This only allows you to use up to nine replacements.
BASE_API string16 ReplaceStringPlaceholders(const string16& format_string,
                                            const std::vector<string16>& subst,
                                            std::vector<size_t>* offsets);

BASE_API std::string ReplaceStringPlaceholders(
    const base::StringPiece& format_string,
    const std::vector<std::string>& subst,
    std::vector<size_t>* offsets);

// Single-string shortcut for ReplaceStringHolders. |offset| may be NULL.
BASE_API string16 ReplaceStringPlaceholders(const string16& format_string,
                                            const string16& a,
                                            size_t* offset);

// Returns true if the string passed in matches the pattern. The pattern
// string can contain wildcards like * and ?
// The backslash character (\) is an escape character for * and ?
// We limit the patterns to having a max of 16 * or ? characters.
// ? matches 0 or 1 character, while * matches 0 or more characters.
BASE_API bool MatchPattern(const base::StringPiece& string,
                           const base::StringPiece& pattern);
BASE_API bool MatchPattern(const string16& string, const string16& pattern);

// Hack to convert any char-like type to its unsigned counterpart.
// For example, it will convert char, signed char and unsigned char to unsigned
// char.
template<typename T>
struct ToUnsigned {
  typedef T Unsigned;
};

template<>
struct ToUnsigned<char> {
  typedef unsigned char Unsigned;
};
template<>
struct ToUnsigned<signed char> {
  typedef unsigned char Unsigned;
};
template<>
struct ToUnsigned<wchar_t> {
#if defined(WCHAR_T_IS_UTF16)
  typedef unsigned short Unsigned;
#elif defined(WCHAR_T_IS_UTF32)
  typedef uint32 Unsigned;
#endif
};
template<>
struct ToUnsigned<short> {
  typedef unsigned short Unsigned;
};

#endif  // BASE_STRING_UTIL_H_
