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

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class WildcardTest : public testing::Test {
 protected:
  // Match a wildcard whose spec should remain unchanged.
  bool WildcardMatch(const StringPiece& spec, const StringPiece& str) {
    Wildcard wildcard(spec);
    EXPECT_EQ(spec, wildcard.spec());
    return CheckMatch(spec, wildcard, str);
  }
  // Match a wildcard whose spec will alter after canonicalization.
  bool WildcardMatchNonCanonical(
      const StringPiece& spec, const StringPiece& str) {
    Wildcard wildcard(spec);
    return CheckMatch(spec, wildcard, str);
  }

 private:
  bool CheckMatch(const StringPiece& spec, const Wildcard& wildcard,
                  const StringPiece& str) {
    scoped_ptr<Wildcard> duplicate(wildcard.Duplicate());
    bool is_simple = spec.find_first_of("*?") == StringPiece::npos;
    EXPECT_EQ(is_simple, wildcard.IsSimple());
    bool result = wildcard.Match(str);
    EXPECT_EQ(wildcard.spec(), duplicate->spec());
    EXPECT_EQ(wildcard.IsSimple(), duplicate->IsSimple());
    EXPECT_EQ(result, duplicate->Match(str));
    return result;
  }
};

TEST_F(WildcardTest, Identity) {
  EXPECT_TRUE(WildcardMatch("Hello", "Hello"));
}

TEST_F(WildcardTest, IdentityExtra) {
  EXPECT_FALSE(WildcardMatch("Hello", "xHello"));
  EXPECT_FALSE(WildcardMatch("Hello", "HelloxX"));
}

TEST_F(WildcardTest, OneStar) {
  EXPECT_TRUE(WildcardMatch("mis*spell", "mistily spell"));
  EXPECT_TRUE(WildcardMatch("mis*spell", "misspell"));
  EXPECT_FALSE(WildcardMatch("mis*spell", "mispell"));
}

TEST_F(WildcardTest, MidDup) {
  EXPECT_TRUE(WildcardMatch("mis*spell*ed", "mistily spell Fred"));
  EXPECT_TRUE(WildcardMatch("mis*spell*ed", "misspell Fred"));
  EXPECT_FALSE(WildcardMatch("mis*spell*ed", "mispell Fred"));
}

TEST_F(WildcardTest, EndDup) {
  EXPECT_TRUE(WildcardMatch("m*is*spell", "mistily spell"));
  EXPECT_TRUE(WildcardMatch("m*is*spell", "misspell"));
  EXPECT_FALSE(WildcardMatch("m*is*spell", "mispell"));
}

TEST_F(WildcardTest, OneQuestion) {
  EXPECT_TRUE(WildcardMatch("H?llo", "Hello"));
}

TEST_F(WildcardTest, TwoQuestionSplit) {
  EXPECT_TRUE(WildcardMatch("H?l?o", "Hello"));
}

TEST_F(WildcardTest, ThreeQuestionAdjacent) {
  EXPECT_TRUE(WildcardMatch("H???o", "Hello"));
}

TEST_F(WildcardTest, SimpleMismatch) {
  EXPECT_FALSE(WildcardMatch("Hello", "Goodbye"));
}

TEST_F(WildcardTest, GreedyTrap1) {
  EXPECT_TRUE(WildcardMatch("*abcd", "abcabcabcabcabcd"));
}

TEST_F(WildcardTest, GreedyTrap2) {
  EXPECT_FALSE(WildcardMatch("*abcd?", "abcabcabcabcabcd"));
  EXPECT_TRUE(WildcardMatch("*abcd*", "abcabcabcabcabcd"));
}

TEST_F(WildcardTest, GreedyTrap3) {
  EXPECT_TRUE(WildcardMatch("*abcd?", "abcabcabcabcabcdabcde"));
}

TEST_F(WildcardTest, GreedyTrap4) {
  EXPECT_TRUE(WildcardMatchNonCanonical("**goo?le*", "ogoodgooglers"));
}

TEST_F(WildcardTest, StarAtBeginning) {
  EXPECT_TRUE(WildcardMatch("*Hello", "Hello"));
  EXPECT_TRUE(WildcardMatch("*ello", "Hello"));
}

TEST_F(WildcardTest, StarAtEnd) {
  EXPECT_TRUE(WildcardMatch("Hello*", "Hello"));
  EXPECT_TRUE(WildcardMatch("Hell*", "Hello"));
}

TEST_F(WildcardTest, QuestionAtBeginning) {
  EXPECT_FALSE(WildcardMatch("?Hello", "Hello"));
  EXPECT_TRUE(WildcardMatch("?ello", "Hello"));
}

TEST_F(WildcardTest, QuestionAtEnd) {
  EXPECT_FALSE(WildcardMatch("Hello?", "Hello"));
  EXPECT_TRUE(WildcardMatch("Hell?", "Hello"));
}

TEST_F(WildcardTest, Empty) {
  EXPECT_TRUE(WildcardMatch("", ""));
  EXPECT_FALSE(WildcardMatch("", "x"));
  EXPECT_TRUE(WildcardMatch("*", ""));
  EXPECT_FALSE(WildcardMatch("?", ""));
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
  EXPECT_FALSE(WildcardMatch("??*", "a"));
  EXPECT_TRUE(WildcardMatch("??*", "aa"));
  EXPECT_TRUE(WildcardMatch("??*", "aaa"));
  EXPECT_FALSE(WildcardMatchNonCanonical("*??", "a"));
  EXPECT_TRUE(WildcardMatchNonCanonical("*??", "aa"));
  EXPECT_TRUE(WildcardMatchNonCanonical("*??", "aaa"));
  EXPECT_FALSE(WildcardMatchNonCanonical("*??*", "a"));
  EXPECT_TRUE(WildcardMatchNonCanonical("*??*", "aa"));
  EXPECT_TRUE(WildcardMatchNonCanonical("*??*", "aaa"));
  EXPECT_FALSE(WildcardMatchNonCanonical("?*?*", "a"));
  EXPECT_TRUE(WildcardMatchNonCanonical("?*?*", "aa"));
  EXPECT_TRUE(WildcardMatchNonCanonical("?*?*", "aaa"));
  EXPECT_FALSE(WildcardMatchNonCanonical("*?*?", "a"));
  EXPECT_TRUE(WildcardMatchNonCanonical("*?*?", "aa"));
  EXPECT_TRUE(WildcardMatchNonCanonical("*?*?", "aaa"));
  EXPECT_FALSE(WildcardMatchNonCanonical("*?*?*", "a"));
  EXPECT_TRUE(WildcardMatchNonCanonical("*?*?*", "aa"));
  EXPECT_TRUE(WildcardMatchNonCanonical("*?*?*", "aaa"));
}

}  // namespace net_instaweb
