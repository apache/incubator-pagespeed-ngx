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


// Extracted from newer GoogleTest version, until we can upgrade
template <typename T>
class WithParamInterface {
 public:
  typedef T ParamType;
  virtual ~WithParamInterface() {}

  // The current parameter value. Is also available in the test fixture's
  // constructor. This member function is non-static, even though it only
  // references static data, to reduce the opportunity for incorrect uses
  // like writing 'WithParamInterface<bool>::GetParam()' for a test that
  // uses a fixture whose parameter type is int.
  const ParamType& GetParam() const { return *parameter_; }

 private:
  // Sets parameter value. The caller is responsible for making sure the value
  // remains alive and unchanged throughout the current test.
  static void SetParam(const ParamType* parameter) {
    parameter_ = parameter;
  }

  // Static value used for accessing parameter during a test lifetime.
  static const ParamType* parameter_;

  // TestClass must be a subclass of WithParamInterface<T> and Test.
  template <class TestClass> friend class internal::ParameterizedTestFactory;
};

template <typename T>
const T* WithParamInterface<T>::parameter_ = NULL;


// Allows EXPECT_STREQ to be used on StringPiece.
namespace internal {
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
