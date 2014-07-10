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
//

#ifndef PAGESPEED_KERNEL_BASE_GTEST_H_
#define PAGESPEED_KERNEL_BASE_GTEST_H_

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net_instaweb {

GoogleString GTestSrcDir();
GoogleString GTestTempDir();

}  // namespace net_instaweb

namespace testing {
namespace internal {

// Allows EXPECT_STREQ to be used on StringPiece.
inline GTEST_API_ AssertionResult CmpHelperSTREQ(
    const char* expected_expression,
    const char* actual_expression,
    const StringPiece& expected,
    const StringPiece& actual) {
  return CmpHelperSTREQ(expected_expression, actual_expression,
                        expected.as_string().c_str(),
                        actual.as_string().c_str());
}

// Allows EXPECT_STRNE to be used on StringPiece.
inline GTEST_API_ AssertionResult CmpHelperSTRNE(
    const char* expected_expression,
    const char* actual_expression,
    const StringPiece& expected,
    const StringPiece& actual) {
  return CmpHelperSTRNE(expected_expression, actual_expression,
                        expected.as_string().c_str(),
                        actual.as_string().c_str());
}

// EXPECT_SUBSTR and EXPECT_SUBSTR_NE allows a simple way to search for a
// substring. Works on StringPiece, char* and GoogleString.
#define EXPECT_HAS_SUBSTR(needle, haystack) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperSUBSTR, needle, haystack)

#define EXPECT_HAS_SUBSTR_NE(needle, haystack) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperSUBSTRNE, needle, haystack)

template <typename StringType>
inline GTEST_API_ AssertionResult CmpHelperSUBSTR(
    const char* haystack_expression,
    const char* needle_expression,
    const StringType haystack,
    const StringPiece& needle) {
  return ::testing::IsSubstring(haystack_expression, needle_expression,
                                haystack, needle.as_string());
}

template <typename StringType1, typename StringType2>
inline GTEST_API_ AssertionResult CmpHelperSUBSTR(
    const char* haystack_expression,
    const char* needle_expression,
    const StringType1 haystack,
    const StringType2 needle) {
  return ::testing::IsSubstring(haystack_expression, needle_expression,
                                haystack, needle);
}

template <typename StringType>
inline GTEST_API_ AssertionResult CmpHelperSUBSTRNE(
    const char* haystack_expression,
    const char* needle_expression,
    const StringType haystack,
    const StringPiece& needle) {
  return ::testing::IsNotSubstring(haystack_expression, needle_expression,
                                   haystack, needle.as_string());
}

template <typename StringType1, typename StringType2>
inline GTEST_API_ AssertionResult CmpHelperSUBSTRNE(
    const char* haystack_expression,
    const char* needle_expression,
    const StringType1 haystack,
    const StringType2 needle) {
  return ::testing::IsNotSubstring(haystack_expression, needle_expression,
                                   haystack, needle);
}

}  // namespace internal
}  // namespace testing

#endif  // PAGESPEED_KERNEL_BASE_GTEST_H_
