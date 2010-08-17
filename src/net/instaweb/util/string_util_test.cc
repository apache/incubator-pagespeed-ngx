/**
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

// Unit-test the string-splitter.

#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class IntegerToStringToIntTest : public testing::Test {
 protected:
  void ValidateIntegerToString(int i, std::string s) {
    EXPECT_EQ(s, IntegerToString(i));
  }

  void ValidateStringToInt(std::string s, int i) {
    int i2;
    EXPECT_TRUE(StringToInt(s, &i2));
    EXPECT_EQ(i, i2);
  }

  void InvalidStringToInt(std::string s) {
    int i;
    EXPECT_FALSE(StringToInt(s, &i));
  }

  void ValidateIntegerToStringToInt(int i) {
    ValidateStringToInt(IntegerToString(i), i);
  }
};

TEST_F(IntegerToStringToIntTest, TestIntegerToString) {
  ValidateIntegerToString(0, "0");
  ValidateIntegerToString(1, "1");
  ValidateIntegerToString(10, "10");
  ValidateIntegerToString(-5, "-5");
  ValidateIntegerToString(123456789, "123456789");
  ValidateIntegerToString(-123456789, "-123456789");
}

TEST_F(IntegerToStringToIntTest, TestStringToInt) {
  ValidateStringToInt("0", 0);
  ValidateStringToInt("1", 1);
  ValidateStringToInt("10", 10);
  ValidateStringToInt("-5", -5);
  ValidateStringToInt("+5", 5);
  ValidateStringToInt("123456789", 123456789);
  ValidateStringToInt("-123456789", -123456789);
  ValidateStringToInt("00000", 0);
  ValidateStringToInt("0001", 1);
  ValidateStringToInt("-0000005", -5);
  ValidateStringToInt("-0005", -5);
}

TEST_F(IntegerToStringToIntTest, TestInvalidString) {
  InvalidStringToInt("");
  InvalidStringToInt("-");
  InvalidStringToInt("+");
  InvalidStringToInt("--1");
  InvalidStringToInt("++1");
  InvalidStringToInt("1-");
  InvalidStringToInt("1+");
  InvalidStringToInt("1 000");
  InvalidStringToInt("a");
  InvalidStringToInt("1e2");
  InvalidStringToInt("10^3");
  InvalidStringToInt("1+3");
}

TEST_F(IntegerToStringToIntTest, TestIntegerToStringToInt) {
  int n = 1;
  for (int i = 0; i < 1000; ++i) {
    ValidateIntegerToStringToInt(i);
    n *= -3;  // This will overflow, that's fine, we just want a range of ints.
  }
}

class SplitStringTest : public testing::Test {
};

TEST_F(SplitStringTest, TestSplitNoOmitTrailing) {
  std::vector<StringPiece> components;
  SplitStringPieceToVector(".a.b..c.", ".", &components, false);
  ASSERT_EQ(static_cast<size_t>(6), components.size());
  ASSERT_EQ("", components[0]);
  ASSERT_EQ("a", components[1]);
  ASSERT_EQ("b", components[2]);
  ASSERT_EQ("", components[3]);
  ASSERT_EQ("c", components[4]);
  ASSERT_EQ("", components[5]);
}

TEST_F(SplitStringTest, TestSplitNoOmitNoTrailing) {
  std::vector<StringPiece> components;
  SplitStringPieceToVector(".a.b..c", ".", &components, false);
  ASSERT_EQ(static_cast<size_t>(5), components.size());
  ASSERT_EQ("", components[0]);
  ASSERT_EQ("a", components[1]);
  ASSERT_EQ("b", components[2]);
  ASSERT_EQ("", components[3]);
  ASSERT_EQ("c", components[4]);
}

TEST_F(SplitStringTest, TestSplitNoOmitEmpty) {
  std::vector<StringPiece> components;
  SplitStringPieceToVector("", ".", &components, false);
  ASSERT_EQ(static_cast<size_t>(1), components.size());
  ASSERT_EQ("", components[0]);
}

TEST_F(SplitStringTest, TestSplitNoOmitOneDot) {
  std::vector<StringPiece> components;
  SplitStringPieceToVector(".", ".", &components, false);
  ASSERT_EQ(static_cast<size_t>(2), components.size());
  ASSERT_EQ("", components[0]);
  ASSERT_EQ("", components[1]);
}

TEST_F(SplitStringTest, TestSplitOmitTrailing) {
  std::vector<StringPiece> components;
  SplitStringPieceToVector(".a.b..c.", ".", &components, true);
  ASSERT_EQ(static_cast<size_t>(3), components.size());
  ASSERT_EQ("a", components[0]);
  ASSERT_EQ("b", components[1]);
  ASSERT_EQ("c", components[2]);
}

TEST_F(SplitStringTest, TestSplitOmitNoTrailing) {
  std::vector<StringPiece> components;
  SplitStringPieceToVector(".a.b..c", ".", &components, true);
  ASSERT_EQ(static_cast<size_t>(3), components.size());
  ASSERT_EQ("a", components[0]);
  ASSERT_EQ("b", components[1]);
  ASSERT_EQ("c", components[2]);
}

TEST_F(SplitStringTest, TestSplitOmitEmpty) {
  std::vector<StringPiece> components;
  SplitStringPieceToVector("", ".", &components, true);
  ASSERT_EQ(static_cast<size_t>(0), components.size());
}

TEST_F(SplitStringTest, TestSplitOmitOneDot) {
  std::vector<StringPiece> components;
  SplitStringPieceToVector(".", ".", &components, true);
  ASSERT_EQ(static_cast<size_t>(0), components.size());
}

}  // namespace net_instaweb
