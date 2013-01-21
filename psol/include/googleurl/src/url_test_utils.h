// Copyright 2007 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Convenience functions for string conversions.
// These are mostly intended for use in unit tests.

#ifndef GOOGLEURL_SRC_URL_TEST_UTILS_H__
#define GOOGLEURL_SRC_URL_TEST_UTILS_H__

#include <string>

#include "base/string16.h"
#include "googleurl/src/url_canon_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace url_test_utils {

// Converts a UTF-16 string from native wchar_t format to char16, by
// truncating the high 32 bits.  This is not meant to handle true UTF-32
// encoded strings.
inline string16 WStringToUTF16(const wchar_t* src) {
  string16 str;
  int length = static_cast<int>(wcslen(src));
  for (int i = 0; i < length; ++i) {
    str.push_back(static_cast<char16>(src[i]));
  }
  return str;
}

// Converts a string from UTF-8 to UTF-16
inline string16 ConvertUTF8ToUTF16(const std::string& src) {
  int length = static_cast<int>(src.length());
  EXPECT_LT(length, 1024);
  url_canon::RawCanonOutputW<1024> output;
  EXPECT_TRUE(url_canon::ConvertUTF8ToUTF16(src.data(), length, &output));
  return string16(output.data(), output.length());
}

// Converts a string from UTF-16 to UTF-8
inline std::string ConvertUTF16ToUTF8(const string16& src) {
  std::string str;
  url_canon::StdStringCanonOutput output(&str);
  EXPECT_TRUE(url_canon::ConvertUTF16ToUTF8(src.data(),
                                            static_cast<int>(src.length()),
                                            &output));
  output.Complete();
  return str;
}

}  // namespace url_test_utils

#endif  // GOOGLEURL_SRC_URL_TEST_UTILS_H__
