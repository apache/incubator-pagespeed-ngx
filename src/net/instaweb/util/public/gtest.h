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

#ifndef NET_INSTAWEB_UTIL_PUBLIC_GTEST_H_
#define NET_INSTAWEB_UTIL_PUBLIC_GTEST_H_

#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "gtest/gtest.h"

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

}  // namespace internal
}  // namespace testing

#endif  // NET_INSTAWEB_UTIL_PUBLIC_GTEST_H_
