// Copyright 2013 Google Inc. All Rights Reserved.
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
//
// Author: gee@google.com (Adam Gee)

#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

namespace {

class TestClass {
 public:
  TestClass() : x_(0) {}

  void Method1(int x) { x_ = x; }

  void Method1ConstArg(const int& x) { x_ = 2 * x; }

  int x() const { return x_; }

 private:
  int x_;
};

TEST(CallbackTest, MemberCallback0_1) {
  TestClass test_class;
  Callback1<int>* cb = NewCallback(&test_class, &TestClass::Method1);
  EXPECT_EQ(0, test_class.x());
  cb->Run(100);
  EXPECT_EQ(100, test_class.x());
}

TEST(CallbackTest, MemberCallback0_1ConstArg) {
  TestClass test_class;
  Callback1<const int&>* cb = NewCallback(&test_class,
                                   &TestClass::Method1ConstArg);
  EXPECT_EQ(0, test_class.x());
  cb->Run(100);
  EXPECT_EQ(200, test_class.x());
}

}  // namespace

}  // namespace net_instaweb
