// Copyright 2010 and onwards Google Inc.
// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the HTML escaper.

#include "net/instaweb/htmlparse/public/html_keywords.h"
#include "net/instaweb/util/public/gtest.h"

namespace net_instaweb {

class HtmlKeywordsTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    HtmlKeywords::Init();
  }

  // In general HtmlKeywords is not reversible, but it is in
  // specific cases.
  void BiTest(const GoogleString& escaped, const GoogleString& unescaped) {
    GoogleString buf;
    EXPECT_EQ(escaped, HtmlKeywords::Escape(unescaped, &buf));
    EXPECT_EQ(unescaped, HtmlKeywords::Unescape(escaped, &buf));
  }

  void TestEscape(const GoogleString& symbolic_code, char value) {
    GoogleString symbolic_escaped = StrCat("&", symbolic_code, ";");
    GoogleString numeric_escaped = StringPrintf(
        "&#%02d;", static_cast<unsigned char>(value));
    GoogleString unescaped(&value, 1), buf;
    BiTest(symbolic_escaped, unescaped);
    EXPECT_EQ(unescaped, HtmlKeywords::Unescape(numeric_escaped, &buf));
  }
};

TEST_F(HtmlKeywordsTest, Keywords) {
  EXPECT_TRUE(HtmlKeywords::KeywordToString(HtmlName::kNotAKeyword) == NULL);
  for (int i = 0; i < HtmlName::num_keywords(); ++i) {
    HtmlName::Keyword keyword = static_cast<HtmlName::Keyword>(i);
    const char* name = HtmlKeywords::KeywordToString(keyword);
    EXPECT_EQ(keyword, HtmlName::Lookup(name));
  }
}

TEST_F(HtmlKeywordsTest, Bidirectional) {
  BiTest("a&amp;b", "a&b");

  // octal 200 is decimal 128, and lacks symbolic representation
  BiTest("a&#128;&#07;b", "a\200\007b");

  GoogleString buf;
  EXPECT_EQ("'", HtmlKeywords::Unescape("&#39;", &buf));
  EXPECT_EQ("(", HtmlKeywords::Unescape("&#40;", &buf));
  EXPECT_EQ(")", HtmlKeywords::Unescape("&#41;", &buf));
}

TEST_F(HtmlKeywordsTest, Hex) {
  GoogleString buf;
  EXPECT_EQ("'", HtmlKeywords::Unescape("&#x27;", &buf));
  EXPECT_EQ("(", HtmlKeywords::Unescape("&#x28;", &buf));
  EXPECT_EQ(")", HtmlKeywords::Unescape("&#x29;", &buf));
}

TEST_F(HtmlKeywordsTest, AllCodes) {
  TestEscape("AElig", 0xC6);
  TestEscape("Aacute", 0xC1);
  TestEscape("Acirc", 0xC2);
  TestEscape("Agrave", 0xC0);
  TestEscape("Aring", 0xC5);
  TestEscape("Atilde", 0xC3);
  TestEscape("Auml", 0xC4);
  TestEscape("Ccedil", 0xC7);
  TestEscape("ETH", 0xD0);
  TestEscape("Eacute", 0xC9);
  TestEscape("Ecirc", 0xCA);
  TestEscape("Egrave", 0xC8);
  TestEscape("Euml", 0xCB);
  TestEscape("Iacute", 0xCD);
  TestEscape("Icirc", 0xCE);
  TestEscape("Igrave", 0xCC);
  TestEscape("Iuml", 0xCF);
  TestEscape("Ntilde", 0xD1);
  TestEscape("Oacute", 0xD3);
  TestEscape("Ocirc", 0xD4);
  TestEscape("Ograve", 0xD2);
  TestEscape("Oslash", 0xD8);
  TestEscape("Otilde", 0xD5);
  TestEscape("Ouml", 0xD6);
  TestEscape("THORN", 0xDE);
  TestEscape("Uacute", 0xDA);
  TestEscape("Ucirc", 0xDB);
  TestEscape("Ugrave", 0xD9);
  TestEscape("Uuml", 0xDC);
  TestEscape("Yacute", 0xDD);
  TestEscape("aacute", 0xE1);
  TestEscape("acirc", 0xE2);
  TestEscape("acute", 0xB4);
  TestEscape("aelig", 0xE6);
  TestEscape("agrave", 0xE0);
  TestEscape("amp", 0x26);
  TestEscape("aring", 0xE5);
  TestEscape("atilde", 0xE3);
  TestEscape("auml", 0xE4);
  TestEscape("brvbar", 0xA6);
  TestEscape("ccedil", 0xE7);
  TestEscape("cedil", 0xB8);
  TestEscape("cent", 0xA2);
  TestEscape("copy", 0xA9);
  TestEscape("curren", 0xA4);
  TestEscape("deg", 0xB0);
  TestEscape("divide", 0xF7);
  TestEscape("eacute", 0xE9);
  TestEscape("ecirc", 0xEA);
  TestEscape("egrave", 0xE8);
  TestEscape("eth", 0xF0);
  TestEscape("euml", 0xEB);
  TestEscape("frac12", 0xBD);
  TestEscape("frac14", 0xBC);
  TestEscape("frac34", 0xBE);
  TestEscape("gt", 0x3E);
  TestEscape("iacute", 0xED);
  TestEscape("icirc", 0xEE);
  TestEscape("iexcl", 0xA1);
  TestEscape("igrave", 0xEC);
  TestEscape("iquest", 0xBF);
  TestEscape("iuml", 0xEF);
  TestEscape("laquo", 0xAB);
  TestEscape("lt", 0x3C);
  TestEscape("macr", 0xAF);
  TestEscape("micro", 0xB5);
  TestEscape("middot", 0xB7);
  TestEscape("nbsp", 0xA0);
  TestEscape("not", 0xAC);
  TestEscape("ntilde", 0xF1);
  TestEscape("oacute", 0xF3);
  TestEscape("ocirc", 0xF4);
  TestEscape("ograve", 0xF2);
  TestEscape("ordf", 0xAA);
  TestEscape("ordm", 0xBA);
  TestEscape("oslash", 0xF8);
  TestEscape("otilde", 0xF5);
  TestEscape("ouml", 0xF6);
  TestEscape("para", 0xB6);
  TestEscape("plusmn", 0xB1);
  TestEscape("pound", 0xA3);
  TestEscape("quot", 0x22);
  TestEscape("raquo", 0xBB);
  TestEscape("reg", 0xAE);
  TestEscape("sect", 0xA7);
  TestEscape("shy", 0xAD);
  TestEscape("sup1", 0xB9);
  TestEscape("sup2", 0xB2);
  TestEscape("sup3", 0xB3);
  TestEscape("szlig", 0xDF);
  TestEscape("thorn", 0xFE);
  TestEscape("times", 0xD7);
  TestEscape("uacute", 0xFA);
  TestEscape("ucirc", 0xFB);
  TestEscape("ugrave", 0xF9);
  TestEscape("uml", 0xA8);
  TestEscape("uuml", 0xFC);
  TestEscape("yacute", 0xFD);
  TestEscape("yen", 0xA5);
  TestEscape("yuml", 0xFF);
}

/*
 * TODO(jmarantz): fix this.
 * TEST_F(HtmlKeywordsTest, Unescape) {
 *   GoogleString buf;
 *   EXPECT_EQ("a&b", HtmlKeywords::Unescape("a&#26;b", &buf));
 *   GoogleString expected;
 *   expected += 'a';
 *   expected += 0x03;
 *   expected += 0xa7;
 *   expected += 'b';
 *   EXPECT_EQ(expected, HtmlKeywords::Unescape("a&chi;b", &buf));
 *   EXPECT_EQ(GoogleString("a&#03;&#a7;b"), HtmlKeywords::Escape(expected, &buf));
 * }
 */

}  // namespace net_instaweb
