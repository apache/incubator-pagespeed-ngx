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

  void Method1ConstRefArg(const int& x) { x_ = 2 * x; }

  void Method2(int a, int b) { x_ = a + b; }

  void Method2ConstRefArg(const int& a, int b) { x_ = a + b; }

  int x() const { return x_; }

 private:
  int x_;
};

TEST(CallbackTest, MemberCallback_0_1) {
  TestClass test_class;
  Callback1<int>* cb = NewCallback(&test_class, &TestClass::Method1);
  EXPECT_EQ(0, test_class.x());
  cb->Run(100);
  EXPECT_EQ(100, test_class.x());
}

TEST(CallbackTest, MemberCallback_0_1_ConstRefArg) {
  TestClass test_class;
  Callback1<const int&>* cb = NewCallback(&test_class,
                                   &TestClass::Method1ConstRefArg);
  EXPECT_EQ(0, test_class.x());
  cb->Run(100);
  EXPECT_EQ(200, test_class.x());
}

TEST(CallbackTest, MemberCallback_1_1) {
  TestClass test_class;
  Callback1<int>* cb = NewCallback(&test_class, &TestClass::Method2, 1);
  EXPECT_EQ(0, test_class.x());
  cb->Run(2);
  EXPECT_EQ(3, test_class.x());
}

TEST(CallbackTest, MemberCallback_1_1ConstRefArg) {
  TestClass test_class;
  int arg = 1;
  Callback1<int>* cb = NewCallback(&test_class,
                                   &TestClass::Method2ConstRefArg,
                                   arg);
  // Increment x.
  ++arg;
  EXPECT_EQ(2, arg);
  EXPECT_EQ(0, test_class.x());
  cb->Run(2);
  // The callback should have the bound value of 1, even though it was passed
  // by reference.
  EXPECT_EQ(3, test_class.x());
}

}  // namespace

}  // namespace net_instaweb
