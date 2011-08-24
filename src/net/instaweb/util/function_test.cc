// Copyright 2011 Google Inc.
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
// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/function.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class FunctionTest : public testing::Test {
 public:
  FunctionTest() {
    Clear();
  }

  void Clear() {
    char_ = '\0';
    int_ = 0;
    double_ = 0.0;
    was_run_ = false;
    was_cancelled_ = false;
  }

  void Run0() {
    was_run_ = true;
  }

  void Run1(char c) {
    char_ = c;
    was_run_ = true;
  }

  void Run2(char c, int i) {
    char_ = c;
    int_ = i;
    was_run_ = true;
  }

  void Run3(char c, int i, double d) {
    char_ = c;
    int_ = i;
    double_ = d;
    was_run_ = true;
  }

  void Cancel() {
    was_cancelled_ = true;
  }

  bool Matches(char c, int i, double d) const {
    return ((c == char_) && (i == int_) && (d == double_));
  }

 protected:
  char char_;
  int int_;
  double double_;
  bool was_run_;
  bool was_cancelled_;
};

TEST_F(FunctionTest, Run0NoCancel) {
  FunctionTest* function_test = this;
  MemberFunction0<FunctionTest> f(&FunctionTest::Run0, function_test);
  f.Run();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0));
}

TEST_F(FunctionTest, Run0WithCancel) {
  FunctionTest* function_test = this;
  MemberFunction0<FunctionTest> f(&FunctionTest::Run0,
                                  &FunctionTest::Cancel,
                                  function_test);
  f.Run();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0));

  Clear();
  f.Cancel();
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0));
}

TEST_F(FunctionTest, Run1NoCancel) {
  FunctionTest* function_test = this;
  MemberFunction1<FunctionTest, char> f(&FunctionTest::Run1,
                                        function_test, 'x');
  f.Run();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('x', 0, 0.0));
}

TEST_F(FunctionTest, Run1WithCancel) {
  FunctionTest* function_test = this;
  MemberFunction1<FunctionTest, char> f(&FunctionTest::Run1,
                                        &FunctionTest::Cancel,
                                        function_test, 'x');
  f.Run();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('x', 0, 0.0));

  Clear();
  f.Cancel();
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0));
}

TEST_F(FunctionTest, Run2NoCancel) {
  FunctionTest* function_test = this;
  MemberFunction2<FunctionTest, char, int> f(&FunctionTest::Run2,
                                             function_test, 'x', 42);
  f.Run();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('x', 42, 0.0));
}

TEST_F(FunctionTest, Run2WithCancel) {
  FunctionTest* function_test = this;
  MemberFunction2<FunctionTest, char, int> f(&FunctionTest::Run2,
                                             &FunctionTest::Cancel,
                                             function_test, 'x', 42);
  f.Run();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('x', 42, 0.0));

  Clear();
  f.Cancel();
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0));
}

TEST_F(FunctionTest, Run3NoCancel) {
  FunctionTest* function_test = this;
  MemberFunction3<FunctionTest, char, int, double> f(&FunctionTest::Run3,
                                                    function_test, 'x', 43,
                                                    5.5);
  f.Run();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('x', 43, 5.5));
}

TEST_F(FunctionTest, Run3WithCancel) {
  FunctionTest* function_test = this;
  MemberFunction3<FunctionTest, char, int, double> f(&FunctionTest::Run3,
                                                     &FunctionTest::Cancel,
                                                     function_test, 'x', 43,
                                                     5.5);
  f.Run();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('x', 43, 5.5));

  Clear();
  f.Cancel();
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0));
}

}  // namespace net_instaweb
