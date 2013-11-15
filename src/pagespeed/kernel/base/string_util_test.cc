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

// Unit-test the string-splitter.

#include <locale.h>
#include <cstddef>
#include <set>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {

TEST(STATIC_STRLEN_Test, CorrectStaticStringLengths) {
  EXPECT_EQ(0, STATIC_STRLEN(""));
  EXPECT_EQ(1, STATIC_STRLEN("a"));
  EXPECT_EQ(1, STATIC_STRLEN("\n"));
  EXPECT_EQ(1, STATIC_STRLEN("\xff"));
  EXPECT_EQ(1, STATIC_STRLEN("\0"));
  EXPECT_EQ(2, STATIC_STRLEN("ab"));
  EXPECT_EQ(2, STATIC_STRLEN("\r\n"));
  EXPECT_EQ(2, STATIC_STRLEN("\xfe\xff"));
  EXPECT_EQ(2, STATIC_STRLEN("\0a"));
  EXPECT_EQ(14, STATIC_STRLEN("Testing string"));
  static const char ascii_lowercase[] = "abcdefghijklmnopqrstuvwxyz";
  EXPECT_EQ(26, STATIC_STRLEN(ascii_lowercase));
  const char digits[] = "0123456789";
  EXPECT_EQ(10, STATIC_STRLEN(digits));

  // This will fail at compile time:
  //   net/instaweb/util/string_util_test.cc:51:3: error: no matching function
  //     for call to 'ArraySizeHelper'
  //     STATIC_STRLEN(hello_world_ptr);
  //     ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  //   In file included from net/instaweb/util/string_util_test.cc:25:
  //   ./third_party/pagespeed/kernel/base/string_util.h:46:39: note:
  //       instantiated from:
  //   #define STATIC_STRLEN(static_string) (arraysize(static_string) - 1)
  //                                         ^
  //   In file included from net/instaweb/util/string_util_test.cc:25:
  //   In file included from
  //       ./third_party/pagespeed/kernel/base/string_util.h:34:
  //   In file included from ./strings/join.h:16:
  //   ./base/macros.h:143:34: note: instantiated from:
  //   #define arraysize(array) (sizeof(ArraySizeHelper(array)))
  //                                    ^~~~~~~~~~~~~~~
  //   ./base/macros.h:133:8: note: candidate template ignored: failed template
  //     argument deduction
  //   char (&ArraySizeHelper(T (&array)[N]))[N];
  //          ^
  //   ./base/macros.h:140:8: note: candidate template ignored: failed template
  //     argument deduction
  //   char (&ArraySizeHelper(const T (&array)[N]))[N];
  //          ^
  // TODO(sligocki): Find a way to actively test this:
  // GoogleString hello_world("Hello, world!");
  // const char* hello_world_ptr = hello_world.data();
  // STATIC_STRLEN(hello_world_ptr);
}

class IntegerToStringToIntTest : public testing::Test {
 protected:
  void ValidateIntegerToString(int i, GoogleString s) {
    EXPECT_EQ(s, IntegerToString(i));
    ValidateInteger64ToString(static_cast<int64>(i), s);
  }

  void ValidateStringToInt(GoogleString s, int i) {
    int i2;
    EXPECT_TRUE(StringToInt(s, &i2));
    EXPECT_EQ(i, i2);
    ValidateStringToInt64(s, static_cast<int64>(i));
  }

  void InvalidStringToInt(GoogleString s, int expected) {
    int i = -50;
    EXPECT_FALSE(StringToInt(s, &i));
    EXPECT_EQ(expected, i);
    InvalidStringToInt64(s);
  }

  void ValidateIntegerToStringToInt(int i) {
    ValidateStringToInt(IntegerToString(i), i);
  }

  // Second verse, same as the first, a little more bits...
  void ValidateInteger64ToString(int64 i, GoogleString s) {
    EXPECT_EQ(s, Integer64ToString(i));
  }

  void ValidateStringToInt64(GoogleString s, int64 i) {
    int64 i2;
    EXPECT_TRUE(StringToInt64(s, &i2));
    EXPECT_EQ(i, i2);
  }

  void InvalidStringToInt64(GoogleString s) {
    int64 i;
    EXPECT_FALSE(StringToInt64(s, &i));
  }

  void ValidateInteger64ToStringToInt64(int64 i) {
    ValidateStringToInt64(Integer64ToString(i), i);
  }
};

TEST_F(IntegerToStringToIntTest, TestIntegerToString) {
  ValidateIntegerToString(0, "0");
  ValidateIntegerToString(1, "1");
  ValidateIntegerToString(10, "10");
  ValidateIntegerToString(-5, "-5");
  ValidateIntegerToString(123456789, "123456789");
  ValidateIntegerToString(-123456789, "-123456789");
  ValidateInteger64ToString(99123456789LL, "99123456789");
  ValidateInteger64ToString(-99123456789LL, "-99123456789");
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
  ValidateStringToInt("010", 10);
  ValidateStringToInt("-0000005", -5);
  ValidateStringToInt("-00089", -89);
  ValidateStringToInt64("-99123456789", -99123456789LL);
}

TEST_F(IntegerToStringToIntTest, TestInvalidString) {
  InvalidStringToInt("", 0);
  InvalidStringToInt("-", 0);
  InvalidStringToInt("+", 0);
  InvalidStringToInt("--1", 0);
  InvalidStringToInt("++1", 0);
  InvalidStringToInt("1-", 1);
  InvalidStringToInt("1+", 1);
  InvalidStringToInt("1 000", 1);
  InvalidStringToInt("a", 0);
  InvalidStringToInt("1e2", 1);
  InvalidStringToInt("10^3", 10);
  InvalidStringToInt("1+3", 1);
  InvalidStringToInt("0x6A7", 0);
  InvalidStringToInt("  45Junk", 45);
}

TEST_F(IntegerToStringToIntTest, TestIntegerToStringToInt) {
  int n = 1;
  for (int i = 0; i < 1000; ++i) {
    ValidateIntegerToStringToInt(n);
    n *= -3;  // This will overflow, that's fine, we just want a range of ints.
  }
  int64 n64 = 1LL;
  for (int i = 0; i < 1000; ++i) {
    ValidateInteger64ToStringToInt64(n64);
    n64 *= -3;  // This will overflow, that's fine, we just want a range of ints
  }
}

class SplitStringTest : public testing::Test {
};

TEST_F(SplitStringTest, TestSplitNoOmitTrailing) {
  StringPieceVector components;
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
  StringPieceVector components;
  SplitStringPieceToVector(".a.b..c", ".", &components, false);
  ASSERT_EQ(static_cast<size_t>(5), components.size());
  ASSERT_EQ("", components[0]);
  ASSERT_EQ("a", components[1]);
  ASSERT_EQ("b", components[2]);
  ASSERT_EQ("", components[3]);
  ASSERT_EQ("c", components[4]);
}

TEST_F(SplitStringTest, TestSplitNoOmitEmpty) {
  StringPieceVector components;
  SplitStringPieceToVector("", ".", &components, false);
  ASSERT_EQ(static_cast<size_t>(1), components.size());
  ASSERT_EQ("", components[0]);
}

TEST_F(SplitStringTest, TestSplitNoOmitOneDot) {
  StringPieceVector components;
  SplitStringPieceToVector(".", ".", &components, false);
  ASSERT_EQ(static_cast<size_t>(2), components.size());
  ASSERT_EQ("", components[0]);
  ASSERT_EQ("", components[1]);
}

TEST_F(SplitStringTest, TestSplitOmitTrailing) {
  StringPieceVector components;
  SplitStringPieceToVector(".a.b..c.", ".", &components, true);
  ASSERT_EQ(static_cast<size_t>(3), components.size());
  ASSERT_EQ("a", components[0]);
  ASSERT_EQ("b", components[1]);
  ASSERT_EQ("c", components[2]);
}

TEST_F(SplitStringTest, TestSplitOmitNoTrailing) {
  StringPieceVector components;
  SplitStringPieceToVector(".a.b..c", ".", &components, true);
  ASSERT_EQ(static_cast<size_t>(3), components.size());
  ASSERT_EQ("a", components[0]);
  ASSERT_EQ("b", components[1]);
  ASSERT_EQ("c", components[2]);
}

TEST_F(SplitStringTest, TestSplitOmitEmpty) {
  StringPieceVector components;
  SplitStringPieceToVector("", ".", &components, true);
  ASSERT_EQ(static_cast<size_t>(0), components.size());
}

TEST_F(SplitStringTest, TestSplitOmitOneDot) {
  StringPieceVector components;
  SplitStringPieceToVector(".", ".", &components, true);
  ASSERT_EQ(static_cast<size_t>(0), components.size());
}

TEST_F(SplitStringTest, TestSplitMultiSeparator) {
  StringPieceVector components;
  SplitStringPieceToVector("a/b c;d,", " /;", &components, true);
  ASSERT_EQ(static_cast<size_t>(4), components.size());
  ASSERT_EQ("a", components[0]);
  ASSERT_EQ("b", components[1]);
  ASSERT_EQ("c", components[2]);
  ASSERT_EQ("d,", components[3]);
}

TEST_F(SplitStringTest, TestPieceAfterEquals) {
  GoogleString piece("Test=1");
  ASSERT_EQ("1", PieceAfterEquals(piece));
  GoogleString piece2("TestingWithNoEquals");
  ASSERT_EQ("", PieceAfterEquals(piece2));
  GoogleString piece3("    TestingWithSpace =     45     ");
  ASSERT_EQ("45", PieceAfterEquals(piece3));
  GoogleString piece4("Test1=1;Test2=2");
  ASSERT_EQ("1;Test2=2", PieceAfterEquals(piece4));
}

TEST(StringCaseTest, TestStringCaseEqual) {
  EXPECT_FALSE(StringCaseEqual("foobar", "fobar"));
  EXPECT_TRUE(StringCaseEqual("foobar", "foobar"));
  EXPECT_TRUE(StringCaseEqual("foobar", "FOOBAR"));
  EXPECT_TRUE(StringCaseEqual("FOOBAR", "foobar"));
  EXPECT_TRUE(StringCaseEqual("fOoBaR", "FoObAr"));
}

TEST(StringCaseTest, TestStringCaseCompare) {
  EXPECT_GT(0, StringCaseCompare("a", "aa"));
  EXPECT_LT(0, StringCaseCompare("aa", "a"));
  EXPECT_EQ(0, StringCaseCompare("a", "a"));
  EXPECT_EQ(0, StringCaseCompare("a", "A"));
  EXPECT_EQ(0, StringCaseCompare("A", "a"));
  EXPECT_GT(0, StringCaseCompare("A", "b"));
  EXPECT_GT(0, StringCaseCompare("a", "B"));
  EXPECT_LT(0, StringCaseCompare("b", "A"));
  EXPECT_LT(0, StringCaseCompare("B", "a"));
}

TEST(StringCaseTest, TestStringCaseStartsWith) {
  EXPECT_FALSE(StringCaseStartsWith("foobar", "fob"));
  EXPECT_TRUE(StringCaseStartsWith("foobar", "foobar"));
  EXPECT_TRUE(StringCaseStartsWith("foobar", "foo"));
  EXPECT_TRUE(StringCaseStartsWith("foobar", "FOO"));
  EXPECT_TRUE(StringCaseStartsWith("FOOBAR", "foo"));
  EXPECT_TRUE(StringCaseStartsWith("fOoBaR", "FoO"));
  EXPECT_FALSE(StringCaseStartsWith("zzz", "zzzz"));
}

TEST(StringCaseTest, TestStringCaseEndsWith) {
  EXPECT_FALSE(StringCaseEndsWith("foobar", "baar"));
  EXPECT_TRUE(StringCaseEndsWith("foobar", "foobar"));
  EXPECT_TRUE(StringCaseEndsWith("foobar", "bar"));
  EXPECT_TRUE(StringCaseEndsWith("foobar", "BAR"));
  EXPECT_TRUE(StringCaseEndsWith("FOOBAR", "bar"));
  EXPECT_TRUE(StringCaseEndsWith("fOoBaR", "bAr"));
  EXPECT_FALSE(StringCaseEndsWith("zzz", "zzzz"));
}

TEST(StringCaseTest, TestStringEqualConcat) {
  EXPECT_TRUE(StringEqualConcat("foobar", "foobar", ""));
  EXPECT_TRUE(StringEqualConcat("foobar", "fooba", "r"));
  EXPECT_TRUE(StringEqualConcat("foobar", "", "foobar"));
  EXPECT_TRUE(StringEqualConcat("fOobAr", "fO", "obAr"));
  EXPECT_FALSE(StringEqualConcat("fOobAr", "fo", "obAr"));
  EXPECT_FALSE(StringEqualConcat("foobar", "FO", "OBAR"));
  EXPECT_FALSE(StringEqualConcat("foobar", "foo", "obar"));
}

TEST(StringCaseTest, FindIgnoreCase) {
  EXPECT_EQ(0, FindIgnoreCase("abc", "aBC"));
  EXPECT_EQ(1, FindIgnoreCase("abc", "BC"));
  EXPECT_EQ(1, FindIgnoreCase("abcbc", "BC"));
  EXPECT_EQ(2, FindIgnoreCase("abCbc", "cB"));
  EXPECT_EQ(StringPiece::npos, FindIgnoreCase("abc", "bcd"));
  EXPECT_EQ(StringPiece::npos, FindIgnoreCase("abc", "abcd"));
}

TEST(StringCaseTest, Locale) {
  // This will fail if the locale is available and StringCaseEqual is
  // built using strcasecmp.  Note that the locale will generally be
  // available from mod_pagespeed development environments, or it
  // can be installed via:
  //    sudo apt-get install language-pack-tr-base
  if (setlocale(LC_ALL, "tr_TR.utf8") != NULL) {
    EXPECT_TRUE(StringCaseEqual("div", "DIV"));

    // TODO(jmarantz): Leaving the locale set as above is certainly
    // an exciting way to see what other tests fail.  Two such tests
    // are:
    //   [  FAILED  ] CssFilterTest.RewriteVariousCss
    //   [  FAILED  ] CssFilterTest.ComplexCssTest
    // In the meantime we'll set the locale back.
    setlocale(LC_ALL, "");
  }
}

TEST(ParseShellLikeStringTest, TestParse) {
  std::vector<GoogleString> parts;
  ParseShellLikeString("a b \"c d\" e 'f g'", &parts);
  ASSERT_EQ(5, parts.size());
  EXPECT_EQ("a",   parts[0]);
  EXPECT_EQ("b",   parts[1]);
  EXPECT_EQ("c d", parts[2]);
  EXPECT_EQ("e",   parts[3]);
  EXPECT_EQ("f g", parts[4]);
}

TEST(ParseShellLikeStringTest, Backslash) {
  std::vector<GoogleString> parts;
  ParseShellLikeString(" \"a\\\"b\" 'c\\'d' ", &parts);
  ASSERT_EQ(2, parts.size());
  EXPECT_EQ("a\"b", parts[0]);
  EXPECT_EQ("c'd",  parts[1]);
}

TEST(ParseShellLikeStringTest, UnclosedQuote) {
  std::vector<GoogleString> parts;
  ParseShellLikeString("'a b", &parts);
  ASSERT_EQ(1, parts.size());
  EXPECT_EQ("a b", parts[0]);
}

TEST(ParseShellLikeStringTest, UnclosedQuoteAndBackslash) {
  std::vector<GoogleString> parts;
  ParseShellLikeString("'a b\\", &parts);
  ASSERT_EQ(1, parts.size());
  EXPECT_EQ("a b", parts[0]);
}

class BasicUtilsTest : public testing::Test {
};

TEST(BasicUtilsTest, TrimLeadingWhitespaceTest) {
  StringPiece trimmed("Mary had a little lamb.  ");
  // Whitespace at both ends
  StringPiece test_piece1("\r\n\f\t Mary had a little lamb.  ");
  EXPECT_TRUE(TrimLeadingWhitespace(&test_piece1));
  EXPECT_EQ(trimmed, test_piece1);
  // No whitespace to trim
  StringPiece test_piece2(trimmed);
  EXPECT_FALSE(TrimLeadingWhitespace(&test_piece2));
  EXPECT_EQ(trimmed, test_piece2);
}

TEST(BasicUtilsTest, TrimTrailingWhitespaceTest) {
  StringPiece trimmed("  Mary had a little lamb.");
  // Whitespace at both ends
  StringPiece test_piece1("  Mary had a little lamb.  \r\n\f\t");
  EXPECT_TRUE(TrimTrailingWhitespace(&test_piece1));
  EXPECT_EQ(trimmed, test_piece1);
  // No whitespace to trim
  StringPiece test_piece2(trimmed);
  EXPECT_FALSE(TrimTrailingWhitespace(&test_piece2));
  EXPECT_EQ(trimmed, test_piece2);
}

TEST(BasicUtilsTest, TrimWhitespaceTest) {
  StringPiece trimmed("Mary had a little lamb.");
  // Whitespace at both ends
  StringPiece test_piece1("\t Mary had a little lamb.\n \r ");
  EXPECT_TRUE(TrimWhitespace(&test_piece1));
  EXPECT_EQ(trimmed, test_piece1);
  // No whitespace to trim
  StringPiece test_piece2(trimmed);
  EXPECT_FALSE(TrimWhitespace(&test_piece2));
  EXPECT_EQ(trimmed, test_piece2);
  // Whitespace to left
  StringPiece test_piece3("\f Mary had a little lamb.");
  EXPECT_TRUE(TrimWhitespace(&test_piece3));
  EXPECT_EQ(trimmed, test_piece3);
  // Whitespace to right
  StringPiece test_piece4("Mary had a little lamb.\r\n");
  EXPECT_TRUE(TrimWhitespace(&test_piece4));
  EXPECT_EQ(trimmed, test_piece4);
}

TEST(BasicUtilsTest, CountSubstringTest) {
  StringPiece text1("This sentence contains twice twice.");
  StringPiece e("e");
  StringPiece twice("twice");
  StringPiece en("en");
  EXPECT_EQ(5, CountSubstring(text1, e));
  EXPECT_EQ(2, CountSubstring(text1, twice));
  EXPECT_EQ(2, CountSubstring(text1, en));
  StringPiece text2("Finished files are the result\nof years of scientific "
                    "study\ncombined with the experience\nof years...");
  StringPiece f("f");
  StringPiece of("of");
  EXPECT_EQ(5, CountSubstring(text2, f));
  EXPECT_EQ(3, CountSubstring(text2, of));
  StringPiece text3("abababab");
  StringPiece ab("ab");
  StringPiece abab("abab");
  EXPECT_EQ(4, CountSubstring(text3, ab));
  EXPECT_EQ(3, CountSubstring(text3, abab));

  EXPECT_EQ(3, CountSubstring("aaaaa", "aaa"));
}

TEST(BasicUtilsTest, JoinStringStar) {
  const GoogleString foo = "foo";
  const GoogleString bar = "bar";
  const GoogleString empty = "";
  const GoogleString symbols = "# , #";

  ConstStringStarVector nothing, single, foobar, barfoobar, blah;
  EXPECT_STREQ("", JoinStringStar(nothing, ""));
  EXPECT_STREQ("", JoinStringStar(nothing, ", "));

  single.push_back(&foo);
  EXPECT_STREQ("foo", JoinStringStar(single, ""));
  EXPECT_STREQ("foo", JoinStringStar(single, ", "));

  foobar.push_back(&foo);
  foobar.push_back(&bar);
  EXPECT_STREQ("foobar", JoinStringStar(foobar, ""));
  EXPECT_STREQ("foo, bar", JoinStringStar(foobar, ", "));

  barfoobar.push_back(&bar);
  barfoobar.push_back(&foo);
  barfoobar.push_back(&bar);
  EXPECT_STREQ("barfoobar", JoinStringStar(barfoobar, ""));
  EXPECT_STREQ("bar##foo##bar", JoinStringStar(barfoobar, "##"));

  blah.push_back(&bar);
  blah.push_back(&empty);
  blah.push_back(&symbols);
  blah.push_back(&empty);
  EXPECT_STREQ("bar# , #", JoinStringStar(blah, ""));
  EXPECT_STREQ("bar, , # , #, ", JoinStringStar(blah, ", "));
}

TEST(BasicUtilsTest, JoinCollection) {
  const GoogleString foo = "foo";
  const GoogleString bar = "bar";
  const GoogleString empty = "";
  const GoogleString symbols = "# , #";

  StringPieceVector nothing, single, foobar, barfoobar, blah;
  EXPECT_STREQ("", JoinCollection(nothing, ""));
  EXPECT_STREQ("", JoinCollection(nothing, ", "));

  single.push_back(foo);
  EXPECT_STREQ("foo", JoinCollection(single, ""));
  EXPECT_STREQ("foo", JoinCollection(single, ", "));

  foobar.push_back(foo);
  foobar.push_back(bar);
  EXPECT_STREQ("foobar", JoinCollection(foobar, ""));
  EXPECT_STREQ("foo, bar", JoinCollection(foobar, ", "));

  barfoobar.push_back(bar);
  barfoobar.push_back(foo);
  barfoobar.push_back(bar);
  EXPECT_STREQ("barfoobar", JoinCollection(barfoobar, ""));
  EXPECT_STREQ("bar##foo##bar", JoinCollection(barfoobar, "##"));

  blah.push_back(bar);
  blah.push_back(empty);
  blah.push_back(symbols);
  blah.push_back(empty);
  EXPECT_STREQ("bar# , #", JoinCollection(blah, ""));
  EXPECT_STREQ("bar, , # , #, ", JoinCollection(blah, ", "));
}

TEST(BasicUtilsTest, CEscape) {
  EXPECT_EQ("Hello,\\n\\tWorld.\\n", CEscape("Hello,\n\tWorld.\n"));

  char not_ascii_1 = 30;
  char not_ascii_2 = 200;
  EXPECT_EQ("abc\\036\\310",
            CEscape(GoogleString("abc") + not_ascii_1 + not_ascii_2));
}

TEST(BasicUtilsTest, SplitStringUsingSubstr1) {
  StringPieceVector components;
  net_instaweb::SplitStringUsingSubstr(
      "word1abword2abword3", "ab", &components);
  EXPECT_EQ(3, components.size());
  EXPECT_EQ("word1", components[0]);
  EXPECT_EQ("word2", components[1]);
  EXPECT_EQ("word3", components[2]);
}

TEST(BasicUtilsTest, SplitStringUsingSubstr2) {
  StringPieceVector components;
  net_instaweb::SplitStringUsingSubstr("word1ababword3", "ab", &components);
  EXPECT_EQ(2, components.size());
  EXPECT_EQ("word1", components[0]);
  EXPECT_EQ("word3", components[1]);
}

TEST(BasicUtilsTest, SplitStringUsingSubstr3) {
  StringPieceVector components;
  net_instaweb::SplitStringUsingSubstr("abaaac", "aa", &components);
  EXPECT_EQ(2, components.size());
  EXPECT_EQ("ab", components[0]);
  EXPECT_EQ("ac", components[1]);
}

TEST(BasicUtilsTest, StringPieceFindWithNull) {
  StringPiece null_piece(NULL, 0);
  EXPECT_EQ(StringPiece::npos, null_piece.find("not found"));
}

class JoinCollectionTest : public testing::Test {
 public:
  JoinCollectionTest() { }
  virtual ~JoinCollectionTest() { }
 protected:
  template <typename C>
  void CheckAppendJoinCollection(
      const StringPiece expected,
      const C& collection,
      const StringPiece sep) {
    const char kJoinInit[] = "= ";
    GoogleString join_expected = StrCat(kJoinInit, expected);
    GoogleString join_result(kJoinInit);
    AppendJoinCollection(&join_result, collection, sep);
    EXPECT_STREQ(join_expected, join_result);
    EXPECT_STREQ(expected, JoinCollection(collection, sep));
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(JoinCollectionTest);
};

TEST_F(JoinCollectionTest, BasicSequence) {
  // For set we rely on the fact that the following is already sorted.  If set's
  // iterator isn't lexicographically sorted that's a bug with set!
  const char* kInputs[] = { "", "a", "b", "c", "duck", "elephant" };
  const char kExpected[] = ", a, b, c, duck, elephant";
  StringVector string_vector;
  StringPieceVector stringpiece_vector;
  StringSet string_set;
  for (int i = 0; i < arraysize(kInputs); ++i) {
    string_vector.push_back(kInputs[i]);
    stringpiece_vector.push_back(kInputs[i]);
    string_set.insert(kInputs[i]);
  }
  CheckAppendJoinCollection(kExpected, string_vector, ", ");
  CheckAppendJoinCollection(kExpected, stringpiece_vector, ", ");
  CheckAppendJoinCollection(kExpected, string_set, ", ");
}

TEST_F(JoinCollectionTest, Empty) {
  StringVector string_vector;
  StringPieceVector stringpiece_vector;
  StringSet string_set;
  CheckAppendJoinCollection("", string_vector, ", ");
  CheckAppendJoinCollection("", stringpiece_vector, ", ");
  CheckAppendJoinCollection("", string_set, ", ");
}

TEST_F(JoinCollectionTest, SingletonEmpty) {
  StringVector string_vector;
  StringPieceVector stringpiece_vector;
  StringSet string_set;
  string_vector.push_back("");
  stringpiece_vector.push_back("");
  string_set.insert("");
  CheckAppendJoinCollection("", string_vector, ", ");
  CheckAppendJoinCollection("", stringpiece_vector, ", ");
  CheckAppendJoinCollection("", string_set, ", ");
}

class TrimQuoteTest : public testing::Test {
 protected:
  static StringPiece RemoveQuote(StringPiece str) {
    TrimQuote(&str);
    return str;
  }
  static StringPiece RemoveUrlQuotes(StringPiece str) {
    TrimUrlQuotes(&str);
    return str;
  }
};

TEST_F(TrimQuoteTest, TrimQuoteTestAll) {
  EXPECT_STREQ("one", RemoveQuote(" \"one\""));
  EXPECT_STREQ("one", RemoveQuote(" \'one \"  "));
  EXPECT_STREQ("one", RemoveQuote(" \"one \'"));
  EXPECT_STREQ("one", RemoveQuote(" \'one\'"));
  EXPECT_STREQ("one two", RemoveQuote("\"one two\""));
}

TEST_F(TrimQuoteTest, TrimUrlQuoteTestAll) {
  EXPECT_STREQ("one", RemoveUrlQuotes(" \"one\""));
  EXPECT_STREQ("one", RemoveUrlQuotes(" \'one \"  "));
  EXPECT_STREQ("one", RemoveUrlQuotes(" \"one \'"));
  EXPECT_STREQ("one", RemoveUrlQuotes(" \'one\'"));
  EXPECT_STREQ("one two", RemoveUrlQuotes("\"one two\""));
  EXPECT_STREQ("one", RemoveUrlQuotes(" %27one%27"));
  EXPECT_STREQ("one", RemoveUrlQuotes(" %22one %22  "));
  EXPECT_STREQ("one", RemoveUrlQuotes(" %5c%27one %5c%27"));
  EXPECT_STREQ("one", RemoveUrlQuotes(" %5c%22one%5c%22"));
  EXPECT_STREQ("one two", RemoveUrlQuotes("%5C%27'one two'%5C%27"));
}

TEST(SplitStringPieceToIntegerVectorTest, SplitStringPieceToIntegerVector) {
  std::vector<int> ints;

  EXPECT_TRUE(SplitStringPieceToIntegerVector("30,40,50", ",", &ints));
  EXPECT_EQ(3, ints.size());
  EXPECT_EQ(30, ints[0]);
  EXPECT_EQ(40, ints[1]);
  EXPECT_EQ(50, ints[2]);

  EXPECT_TRUE(SplitStringPieceToIntegerVector("", ",", &ints));
  EXPECT_EQ(0, ints.size());

  EXPECT_TRUE(SplitStringPieceToIntegerVector("30#, 50.", "#, .", &ints));
  EXPECT_EQ(2, ints.size());
  EXPECT_EQ(30, ints[0]);
  EXPECT_EQ(50, ints[1]);

  EXPECT_FALSE(SplitStringPieceToIntegerVector("30,xyz,", ",", &ints));
  EXPECT_EQ(0, ints.size());

  EXPECT_FALSE(SplitStringPieceToIntegerVector("30x", ",", &ints));
  EXPECT_EQ(0, ints.size());

  EXPECT_FALSE(SplitStringPieceToIntegerVector("x30,", ",", &ints));
  EXPECT_EQ(0, ints.size());

  EXPECT_FALSE(SplitStringPieceToIntegerVector("30.30, ", ",", &ints));
  EXPECT_EQ(0, ints.size());
}

}  // namespace

}  // namespace net_instaweb
