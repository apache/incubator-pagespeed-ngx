// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRING_NUMBER_CONVERSIONS_H_
#define BASE_STRING_NUMBER_CONVERSIONS_H_

#include <string>
#include <vector>

#include "base/base_api.h"
#include "base/basictypes.h"
#include "base/string16.h"

// ----------------------------------------------------------------------------
// IMPORTANT MESSAGE FROM YOUR SPONSOR
//
// This file contains no "wstring" variants. New code should use string16. If
// you need to make old code work, use the UTF8 version and convert. Please do
// not add wstring variants.
//
// Please do not add "convenience" functions for converting strings to integers
// that return the value and ignore success/failure. That encourages people to
// write code that doesn't properly handle the error conditions.
// ----------------------------------------------------------------------------

namespace base {

// Number -> string conversions ------------------------------------------------

BASE_API std::string IntToString(int value);
BASE_API string16 IntToString16(int value);

BASE_API std::string UintToString(unsigned value);
BASE_API string16 UintToString16(unsigned value);

BASE_API std::string Int64ToString(int64 value);
BASE_API string16 Int64ToString16(int64 value);

BASE_API std::string Uint64ToString(uint64 value);
BASE_API string16 Uint64ToString16(uint64 value);

// DoubleToString converts the double to a string format that ignores the
// locale. If you want to use locale specific formatting, use ICU.
BASE_API std::string DoubleToString(double value);

// String -> number conversions ------------------------------------------------

// Perform a best-effort conversion of the input string to a numeric type,
// setting |*output| to the result of the conversion.  Returns true for
// "perfect" conversions; returns false in the following cases:
//  - Overflow/underflow.  |*output| will be set to the maximum value supported
//    by the data type.
//  - Trailing characters in the string after parsing the number.  |*output|
//    will be set to the value of the number that was parsed.
//  - Leading whitespace in the string before parsing the number. |*output| will
//    be set to the value of the number that was parsed.
//  - No characters parseable as a number at the beginning of the string.
//    |*output| will be set to 0.
//  - Empty string.  |*output| will be set to 0.
BASE_API bool StringToInt(const std::string& input, int* output);
BASE_API bool StringToInt(std::string::const_iterator begin,
                          std::string::const_iterator end,
                          int* output);
BASE_API bool StringToInt(const char* begin, const char* end, int* output);

BASE_API bool StringToInt(const string16& input, int* output);
BASE_API bool StringToInt(string16::const_iterator begin,
                          string16::const_iterator end,
                          int* output);
BASE_API bool StringToInt(const char16* begin, const char16* end, int* output);

BASE_API bool StringToInt64(const std::string& input, int64* output);
BASE_API bool StringToInt64(std::string::const_iterator begin,
                            std::string::const_iterator end,
                            int64* output);
BASE_API bool StringToInt64(const char* begin, const char* end, int64* output);

BASE_API bool StringToInt64(const string16& input, int64* output);
BASE_API bool StringToInt64(string16::const_iterator begin,
                            string16::const_iterator end,
                            int64* output);
BASE_API bool StringToInt64(const char16* begin, const char16* end,
                            int64* output);

// For floating-point conversions, only conversions of input strings in decimal
// form are defined to work.  Behavior with strings representing floating-point
// numbers in hexadecimal, and strings representing non-fininte values (such as
// NaN and inf) is undefined.  Otherwise, these behave the same as the integral
// variants.  This expects the input string to NOT be specific to the locale.
// If your input is locale specific, use ICU to read the number.
BASE_API bool StringToDouble(const std::string& input, double* output);

// Hex encoding ----------------------------------------------------------------

// Returns a hex string representation of a binary buffer. The returned hex
// string will be in upper case. This function does not check if |size| is
// within reasonable limits since it's written with trusted data in mind.  If
// you suspect that the data you want to format might be large, the absolute
// max size for |size| should be is
//   std::numeric_limits<size_t>::max() / 2
BASE_API std::string HexEncode(const void* bytes, size_t size);

// Best effort conversion, see StringToInt above for restrictions.
BASE_API bool HexStringToInt(const std::string& input, int* output);
BASE_API bool HexStringToInt(std::string::const_iterator begin,
                             std::string::const_iterator end,
                             int* output);
BASE_API bool HexStringToInt(const char* begin, const char* end, int* output);

// Similar to the previous functions, except that output is a vector of bytes.
// |*output| will contain as many bytes as were successfully parsed prior to the
// error.  There is no overflow, but input.size() must be evenly divisible by 2.
// Leading 0x or +/- are not allowed.
BASE_API bool HexStringToBytes(const std::string& input,
                               std::vector<uint8>* output);

}  // namespace base

#endif  // BASE_STRING_NUMBER_CONVERSIONS_H_

