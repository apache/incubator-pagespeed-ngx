// Copyright 2010 Google Inc.
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
// Author: jmarantz@google.com (Joshua Marantz),
//         jmaessen@google.com (Jan-Willem Maessen)

#include "net/instaweb/util/public/wildcard.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class WildcardTest : public testing::Test {
};

TEST_F(WildcardTest, Identity) {
  EXPECT_TRUE(Wildcard("Hello").Match("Hello"));
}

TEST_F(WildcardTest, IdentityExtra) {
  EXPECT_FALSE(Wildcard("Hello").Match("xHello"));
  EXPECT_FALSE(Wildcard("Hello").Match("HelloxX"));
}

TEST_F(WildcardTest, OneStar) {
  EXPECT_TRUE(Wildcard("mis*spell").Match("mistily spell"));
  EXPECT_TRUE(Wildcard("mis*spell").Match("misspell"));
  EXPECT_FALSE(Wildcard("mis*spell").Match("mispell"));
}

TEST_F(WildcardTest, MidDup) {
  EXPECT_TRUE(Wildcard("mis*spell*ed").Match("mistily spell Fred"));
  EXPECT_TRUE(Wildcard("mis*spell*ed").Match("misspell Fred"));
  EXPECT_FALSE(Wildcard("mis*spell*ed").Match("mispell Fred"));
}

TEST_F(WildcardTest, EndDup) {
  EXPECT_TRUE(Wildcard("m*is*spell").Match("mistily spell"));
  EXPECT_TRUE(Wildcard("m*is*spell").Match("misspell"));
  EXPECT_FALSE(Wildcard("m*is*spell").Match("mispell"));
}

TEST_F(WildcardTest, OneQuestion) {
  EXPECT_TRUE(Wildcard("H?llo").Match("Hello"));
}

TEST_F(WildcardTest, TwoQuestionSplit) {
  EXPECT_TRUE(Wildcard("H?l?o").Match("Hello"));
}

TEST_F(WildcardTest, ThreeQuestionAdjacent) {
  EXPECT_TRUE(Wildcard("H???o").Match("Hello"));
}

TEST_F(WildcardTest, SimpleMismatch) {
  EXPECT_FALSE(Wildcard("Hello").Match("Goodbye"));
}

TEST_F(WildcardTest, GreedyTrap1) {
  EXPECT_TRUE(Wildcard("*abcd").Match("abcabcabcabcabcd"));
}

TEST_F(WildcardTest, GreedyTrap2) {
  EXPECT_FALSE(Wildcard("*abcd?").Match("abcabcabcabcabcd"));
  EXPECT_TRUE(Wildcard("*abcd*").Match("abcabcabcabcabcd"));
}

TEST_F(WildcardTest, GreedyTrap3) {
  EXPECT_TRUE(Wildcard("*abcd?").Match("abcabcabcabcabcdabcde"));
}

TEST_F(WildcardTest, GreedyTrap4) {
  EXPECT_TRUE(Wildcard("**goo?le*").Match("ogoodgooglers"));
}

TEST_F(WildcardTest, StarAtBeginning) {
  EXPECT_TRUE(Wildcard("*Hello").Match("Hello"));
  EXPECT_TRUE(Wildcard("*ello").Match("Hello"));
}

TEST_F(WildcardTest, StarAtEnd) {
  EXPECT_TRUE(Wildcard("Hello*").Match("Hello"));
  EXPECT_TRUE(Wildcard("Hell*").Match("Hello"));
}

TEST_F(WildcardTest, QuestionAtBeginning) {
  EXPECT_FALSE(Wildcard("?Hello").Match("Hello"));
  EXPECT_TRUE(Wildcard("?ello").Match("Hello"));
}

TEST_F(WildcardTest, QuestionAtEnd) {
  EXPECT_FALSE(Wildcard("Hello?").Match("Hello"));
  EXPECT_TRUE(Wildcard("Hell?").Match("Hello"));
}

TEST_F(WildcardTest, Empty) {
  EXPECT_TRUE(Wildcard("").Match(""));
  EXPECT_FALSE(Wildcard("").Match("x"));
  EXPECT_TRUE(Wildcard("*").Match(""));
  EXPECT_FALSE(Wildcard("?").Match(""));
}

TEST_F(WildcardTest, Simple) {
  EXPECT_FALSE(Wildcard("H*o").IsSimple());
  EXPECT_TRUE(Wildcard("Hello").IsSimple());
  EXPECT_TRUE(Wildcard("").IsSimple());
  EXPECT_FALSE(Wildcard("*").IsSimple());
  EXPECT_FALSE(Wildcard("?").IsSimple());
}

TEST_F(WildcardTest, LengthAtLeastTwo) {
  // Lots of different ways to write this, make sure they all behave.
  EXPECT_FALSE(Wildcard("??*").Match("a"));
  EXPECT_TRUE(Wildcard("??*").Match("aa"));
  EXPECT_TRUE(Wildcard("??*").Match("aaa"));
  EXPECT_FALSE(Wildcard("*??").Match("a"));
  EXPECT_TRUE(Wildcard("*??").Match("aa"));
  EXPECT_TRUE(Wildcard("*??").Match("aaa"));
  EXPECT_FALSE(Wildcard("*??*").Match("a"));
  EXPECT_TRUE(Wildcard("*??*").Match("aa"));
  EXPECT_TRUE(Wildcard("*??*").Match("aaa"));
  EXPECT_FALSE(Wildcard("?*?*").Match("a"));
  EXPECT_TRUE(Wildcard("?*?*").Match("aa"));
  EXPECT_TRUE(Wildcard("?*?*").Match("aaa"));
  EXPECT_FALSE(Wildcard("*?*?").Match("a"));
  EXPECT_TRUE(Wildcard("*?*?").Match("aa"));
  EXPECT_TRUE(Wildcard("*?*?").Match("aaa"));
  EXPECT_FALSE(Wildcard("*?*?*").Match("a"));
  EXPECT_TRUE(Wildcard("*?*?*").Match("aa"));
  EXPECT_TRUE(Wildcard("*?*?*").Match("aaa"));
}

}  // namespace net_instaweb
