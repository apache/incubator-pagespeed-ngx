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

// Copyright 2006 Google Inc. All Rights Reserved.
// Author: dpeng@google.com (Daniel Peng)

#include "webutil/css/parser.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"


namespace Css {

class ParserTest : public testing::Test {
 protected:
  // Accessor for private method.
  Value* ParseAny(Parser* p) {
    return p->ParseAny();
  }

  // The various Test* functions below check that parselen characters
  // are parsed.  Pass -1 to indicate that the entire string should be
  // parsed.

  // Checks that unescaping s returns value.
  void TestUnescape(const char* s, int parselen, char32 value) {
    SCOPED_TRACE(s);
    Parser a(s);
    if (parselen == -1) parselen = strlen(s);
    EXPECT_EQ(value, a.ParseEscape());
    EXPECT_EQ(parselen, a.getpos() - s);
  }

  // Checks that ParseIdent(s) returns utf8golden.
  void TestIdent(const char* s, int parselen, string utf8golden) {
    SCOPED_TRACE(s);
    Parser a(s);
    if (parselen == -1) parselen = strlen(s);
    EXPECT_EQ(utf8golden, UnicodeTextToUTF8(a.ParseIdent()));
    EXPECT_EQ(parselen, a.getpos() - s);
  }

  // Checks that ParseString<">(s) returns utf8golden.
  void TestDstring(const char* s, int parselen, string utf8golden) {
    SCOPED_TRACE(s);
    Parser a(s);
    if (parselen == -1) parselen = strlen(s);
    EXPECT_EQ(utf8golden, UnicodeTextToUTF8(a.ParseString<'"'>()));
    EXPECT_EQ(parselen, a.getpos() - s);
  }

  // Checks that ParseString<'>(s) returns utf8golden.
  void TestSstring(const char* s, int parselen, string utf8golden) {
    SCOPED_TRACE(s);
    Parser a(s);
    if (parselen == -1) parselen = strlen(s);
    EXPECT_EQ(utf8golden, UnicodeTextToUTF8(a.ParseString<'\''>()));
    EXPECT_EQ(parselen, a.getpos() - s);
  }

  // Checks that ParseAny(s) returns goldennum with goldenunit unit.
  void TestAnyNum(const char* s, int parselen, double goldennum,
                  Value::Unit goldenunit) {
    SCOPED_TRACE(s);
    Parser a(s);
    if (parselen == -1) parselen = strlen(s);
    scoped_ptr<Value> t(a.ParseAny());
    EXPECT_EQ(t->GetLexicalUnitType(), Value::NUMBER);
    EXPECT_EQ(t->GetDimension(), goldenunit);
    EXPECT_DOUBLE_EQ(t->GetFloatValue(), goldennum);
    EXPECT_EQ(parselen, a.getpos() - s);
  }

  // Checks that ParseAny(s) returns goldennum with OTHER unit (with
  // unit text goldenunit).
  void TestAnyNumOtherUnit(const char* s, int parselen, double goldennum,
                           string goldenunit) {
    SCOPED_TRACE(s);
    Parser a(s);
    if (parselen == -1) parselen = strlen(s);
    scoped_ptr<Value> t(a.ParseAny());
    EXPECT_EQ(t->GetLexicalUnitType(), Value::NUMBER);
    EXPECT_EQ(t->GetDimension(), Value::OTHER);
    EXPECT_EQ(t->GetDimensionUnitText(), goldenunit);
    EXPECT_EQ(parselen, a.getpos() - s);
  }

  // Checks that ParseAny(s) returns a string-type value with type goldenty
  // and value utf8golden.
  void TestAnyString(const char* s, int parselen, Value::ValueType goldenty,
                     string utf8golden) {
    SCOPED_TRACE(s);
    Parser a(s);
    if (parselen == -1) parselen = strlen(s);
    scoped_ptr<Value> t(a.ParseAny());
    EXPECT_EQ(goldenty, t->GetLexicalUnitType());
    EXPECT_EQ(utf8golden, UnicodeTextToUTF8(t->GetStringValue()));
    EXPECT_EQ(parselen, a.getpos() - s);
  }

  // Checks that ParseAny(s) returns a ident value with identifier goldenty
  // and value utf8golden.
  void TestAnyIdent(const char* s, int parselen, Identifier::Ident goldenty) {
    SCOPED_TRACE(s);
    Parser a(s);
    if (parselen == -1) parselen = strlen(s);
    scoped_ptr<Value> t(a.ParseAny());
    EXPECT_EQ(Value::IDENT, t->GetLexicalUnitType());
    EXPECT_EQ(goldenty, t->GetIdentifier().ident());
    EXPECT_EQ(parselen, a.getpos() - s);
  }

  // Checks that ParseAny(s) returns OTHER identifier (with text goldenident).
  void TestAnyOtherIdent(const char* s, int parselen,
                         const string& goldenident) {
    SCOPED_TRACE(s);
    Parser a(s);
    if (parselen == -1) parselen = strlen(s);
    scoped_ptr<Value> t(a.ParseAny());
    EXPECT_EQ(Value::IDENT, t->GetLexicalUnitType());
    EXPECT_EQ(Identifier::OTHER, t->GetIdentifier().ident());
    EXPECT_EQ(goldenident, UnicodeTextToUTF8(t->GetIdentifierText()));
    EXPECT_EQ(parselen, a.getpos() - s);
  }

  Declarations* ParseAndExpandBackground(const string& str,
                                         bool quirks_mode=true) {
    scoped_ptr<Parser> p(new Parser(str));
    p->set_quirks_mode(quirks_mode);
    scoped_ptr<Values> vals(p->ParseValues(Property::BACKGROUND));
    if (vals.get() == NULL || vals->size() == 0) {
      return NULL;
    }
    Declaration background(Property::BACKGROUND, vals.release(), false);
    scoped_ptr<Declarations> decls(new Declarations);
    Parser::ExpandBackground(background, decls.get());
    if (decls->size() == 0) {
      return NULL;
    }
    return decls.release();
  }

  void TestBackgroundPosition(const string& str, const string& x,
                              const string& y) {
    scoped_ptr<Declarations> decls(ParseAndExpandBackground(str));
    // Find and check position x and y values.
    bool found_x = false;
    bool found_y = false;
    for (Declarations::const_iterator iter = decls->begin();
         iter != decls->end(); ++iter) {
      Declaration* decl = *iter;
      switch (decl->prop()) {
        case Property::BACKGROUND_POSITION_X:
          EXPECT_EQ(x, decl->values()->get(0)->ToString());
          found_x = true;
          break;
        case Property::BACKGROUND_POSITION_Y:
          EXPECT_EQ(y, decl->values()->get(0)->ToString());
          found_y = true;
          break;
        default:
          break;
      }
    }
    EXPECT_TRUE(found_x);
    EXPECT_TRUE(found_y);
  }
};



TEST_F(ParserTest, ErrorNumber) {
  EXPECT_EQ(0, Parser::ErrorNumber(Parser::kUtf8Error));
  EXPECT_EQ(1, Parser::ErrorNumber(Parser::kDeclarationError));
  EXPECT_EQ(8, Parser::ErrorNumber(Parser::kRulesetError));
  EXPECT_EQ(14, Parser::ErrorNumber(Parser::kAtRuleError));
}

TEST_F(ParserTest, unescape) {
  // Invalid Unicode char.
  TestUnescape("\\abcdef aabc", 8, ' ');
  TestUnescape("\\A", 2, 0xA);
  TestUnescape("\\A0b5C\r\n", 8, 0xa0b5C);
  TestUnescape("\\AB ", 4, 0xAB);
}

TEST_F(ParserTest, ident) {
    // We're a little more forgiving than the standard:
    //
    // In CSS 2.1, identifiers (including element names, classes, and
    // IDs in selectors) can contain only the characters [A-Za-z0-9]
    // and ISO 10646 characters U+00A1 and higher, plus the hyphen (-)
    // and the underscore (_); they cannot start with a digit, or a
    // hyphen followed by a digit. Only properties, values, units,
    // pseudo-classes, pseudo-elements, and at-rules may start with a
    // hyphen (-); other identifiers (e.g. element names, classes, or
    // IDs) may not. Identifiers can also contain escaped characters
    // and any ISO 10646 character as a numeric code (see next
    // item). For instance, the identifier "B&W?" may be written as
    // "B\&W\?" or "B\26 W\3F".
  TestIdent("abcd rexo\n", 4, "abcd");
  TestIdent("台灣華語", 12, "台灣華語");
  TestIdent("\\41\\42 \\43 \\44", 14, "ABCD");
  TestIdent("\\41\\42 \\43 \\44g'r,'rcg.,',", 15, "ABCDg");
  TestIdent("\\41\\42 \\43 \\44\r\ng'r,'rcg.,',", 17, "ABCDg");
  TestIdent("-blah-_67", 9, "-blah-_67");
  TestIdent("\\!\\&\\^\\*\\\\e", 11, "!&^*\\e");
}

TEST_F(ParserTest, string) {
  TestSstring("'ab\\'aoe\"\\'eo灣'灣", 17, "ab'aoe\"'eo灣");
  TestDstring("\"ab'aoe\\\"'eo灣\"灣", 16, "ab'aoe\"'eo灣");
  TestSstring("'ab\naoeu", 3, "ab");
  TestDstring("\"ab\naoeu", 3, "ab");
  TestDstring("\"ab\\\naoeu\"", 10, "abaoeu");
}

TEST_F(ParserTest, anynum) {
  TestAnyNum("3.1415 4aone", 6, 3.1415, Value::NO_UNIT);
  TestAnyNum(".1415 4aone", 5, .1415, Value::NO_UNIT);
  TestAnyNum("5 4aone", 1, 5, Value::NO_UNIT);

  TestAnyNum("3.1415pt 4aone", 8, 3.1415, Value::PT);
  TestAnyNum(".1415pc 4aone", 7, .1415, Value::PC);
  TestAnyNum("5s 4aone", 2, 5, Value::S);

  TestAnyNumOtherUnit("5sacks 4aone", 6, 5, "sacks");
  TestAnyNumOtherUnit("5灣 4aone", 4, 5, "灣");
}

TEST_F(ParserTest, anystring) {
  TestAnyIdent("none b c d e", 4, Identifier::NONE);
  TestAnyIdent("none; b c d e", 4, Identifier::NONE);
  TestAnyIdent("none  ; b c d e", 4, Identifier::NONE);
  TestAnyOtherIdent("a b c d e", 1, "a");
  TestAnyOtherIdent("a; b c d e", 1, "a");
  TestAnyOtherIdent("a  ; b c d e", 1, "a");
  TestAnyString("'ab\\'aoe\"\\'eo灣'灣  ; b c d e", 17,
                Value::STRING, "ab'aoe\"'eo灣");
}

TEST_F(ParserTest, color) {
  // allowed in quirks mode
  scoped_ptr<Parser> a(new Parser("abCdEF brc.,aoek"));
  EXPECT_EQ(a->ParseColor().ToString(), "#abcdef");

  // not allowed in stanard compliant mode.
  a.reset(new Parser("abCdEF brc.,aoek"));
  a->set_quirks_mode(false);
  EXPECT_FALSE(a->ParseColor().IsDefined());

  // this is allowed
  a.reset(new Parser("#abCdEF brc.,aoek"));
  a->set_quirks_mode(false);
  EXPECT_EQ(a->ParseColor().ToString(), "#abcdef");

  a.reset(new Parser("abC btneo"));
  EXPECT_EQ(a->ParseColor().ToString(), "#aabbcc");

  // no longer allowed
  a.reset(new Parser("#white something"));
  EXPECT_FALSE(a->ParseColor().IsDefined());

  a.reset(new Parser("#white something"));
  a->set_quirks_mode(false);
  EXPECT_FALSE(a->ParseColor().IsDefined());

  // this is allowed
  a.reset(new Parser("white something"));
  EXPECT_EQ(a->ParseColor().ToString(), "#ffffff");

  a.reset(new Parser("white something"));
  a->set_quirks_mode(false);
  EXPECT_EQ(a->ParseColor().ToString(), "#ffffff");

  // system color
  a.reset(new Parser("buttonface something"));
  EXPECT_EQ(a->ParseColor().ToString(), "#ece9d8");

  // string patterns

  a.reset(new Parser("'abCdEF' brc.,aoek"));
  EXPECT_EQ(a->ParseColor().ToString(), "#abcdef");

  a.reset(new Parser("'abCdEF' brc.,aoek"));
  a->set_quirks_mode(false);
  EXPECT_FALSE(a->ParseColor().IsDefined());

  // this is not allowed since color values must end on string boundary
  a.reset(new Parser("'#abCdEF brc'.,aoek"));
  a->set_quirks_mode(false);
  EXPECT_FALSE(a->ParseColor().IsDefined());

  a.reset(new Parser("\"abC\" btneo"));
  EXPECT_EQ(a->ParseColor().ToString(), "#aabbcc");

  // no longer allowed
  a.reset(new Parser("'#white' something"));
  EXPECT_FALSE(a->ParseColor().IsDefined());

  a.reset(new Parser("'#white' something"));
  a->set_quirks_mode(false);
  EXPECT_FALSE(a->ParseColor().IsDefined());

  // this is allowed
  a.reset(new Parser("'white' something"));
  EXPECT_EQ(a->ParseColor().ToString(), "#ffffff");

  a.reset(new Parser("'white' something"));
  a->set_quirks_mode(false);
  EXPECT_EQ(a->ParseColor().ToString(), "#ffffff");

  // no longer allowed
  a.reset(new Parser("100%"));
  EXPECT_FALSE(a->ParseColor().IsDefined());

  // no longer allowed
  a.reset(new Parser("100px"));
  EXPECT_FALSE(a->ParseColor().IsDefined());

  // this is allowed
  a.reset(new Parser("100"));
  EXPECT_EQ(a->ParseColor().ToString(), "#110000");

  // should be parsed as a number
  a.reset(new Parser("100px"));
  scoped_ptr<Value> t(a->ParseAnyExpectingColor());
  EXPECT_EQ(Value::NUMBER, t->GetLexicalUnitType());
  EXPECT_EQ("100px", t->ToString());

  a.reset(new Parser("rgb(12,25,30)"));
  t.reset(a->ParseAny());
  EXPECT_EQ(t->GetColorValue().ToString(), "#0c191e");

  a.reset(new Parser("rgb( 12% , 25%, 30%)"));
  t.reset(a->ParseAny());
  EXPECT_EQ(t->GetColorValue().ToString(), "#1e3f4c");

  a.reset(new Parser("rgb( 12% , 25% 30%)"));
  t.reset(a->ParseAny());
  EXPECT_FALSE(t.get());
}

TEST_F(ParserTest, url) {
  scoped_ptr<Parser> a(new Parser("url(blah)"));
  scoped_ptr<Value> t(a->ParseAny());

  EXPECT_EQ(Value::URI, t->GetLexicalUnitType());
  EXPECT_EQ("blah", UnicodeTextToUTF8(t->GetStringValue()));

  a.reset(new Parser("url( blah )"));
  t.reset(a->ParseAny());

  EXPECT_EQ(Value::URI, t->GetLexicalUnitType());
  EXPECT_EQ("blah", UnicodeTextToUTF8(t->GetStringValue()));

  a.reset(new Parser("url( blah extra)"));
  t.reset(a->ParseAny());

  EXPECT_EQ(static_cast<Value *>(NULL), t.get());
}

TEST_F(ParserTest, rect) {
  // rect can be either comma or space delimited
  scoped_ptr<Parser> a(new Parser("rect( 12,  10,auto  200px)"));
  scoped_ptr<Value> t(a->ParseAny());

  EXPECT_EQ(Value::RECT, t->GetLexicalUnitType());
  ASSERT_EQ(4, t->GetParameters()->size());
  EXPECT_EQ(Value::NUMBER,
            t->GetParameters()->get(0)->GetLexicalUnitType());
  EXPECT_EQ(12, t->GetParameters()->get(0)->GetIntegerValue());
  EXPECT_EQ(Value::IDENT,
            t->GetParameters()->get(2)->GetLexicalUnitType());
  EXPECT_EQ(Identifier::AUTO,
            t->GetParameters()->get(2)->GetIdentifier().ident());

  a.reset(new Parser("rect(auto)"));
  t.reset(a->ParseAny());

  EXPECT_EQ(static_cast<Value *>(NULL), t.get());

  a.reset(new Parser("rect()"));
  t.reset(a->ParseAny());

  EXPECT_EQ(static_cast<Value *>(NULL), t.get());

  a.reset(new Parser("rect(13 10 auto 4)"));
  t.reset(a->ParseAny());

  EXPECT_EQ(13, t->GetParameters()->get(0)->GetIntegerValue());

  a.reset(new Parser("rect(14,10,1,2)"));
  t.reset(a->ParseAny());

  EXPECT_EQ(14, t->GetParameters()->get(0)->GetIntegerValue());

  a.reset(new Parser("rect(15 10 1)"));
  t.reset(a->ParseAny());

  EXPECT_EQ(static_cast<Value *>(NULL), t.get());

  a.reset(new Parser("rect(16 10 1 2 3)"));
  t.reset(a->ParseAny());

  EXPECT_EQ(static_cast<Value *>(NULL), t.get());
}

TEST_F(ParserTest, background) {
  scoped_ptr<Declarations> decls(ParseAndExpandBackground("#333"));

  EXPECT_EQ(6, decls->size());

  decls.reset(ParseAndExpandBackground("fff"));
  EXPECT_TRUE(decls.get());
  // Not valid for quirks_mode=false
  EXPECT_FALSE(ParseAndExpandBackground("fff", false));

  decls.reset(ParseAndExpandBackground("fff000"));
  EXPECT_TRUE(decls.get());
  // Not valid for quirks_mode=false
  EXPECT_FALSE(ParseAndExpandBackground("fff000", false));

  // This should now be parsed as background position instead of color.
  decls.reset(ParseAndExpandBackground("100%"));
  ASSERT_TRUE(decls.get());
  ASSERT_EQ(6, decls->size());
  EXPECT_EQ(Property::BACKGROUND_COLOR, decls->get(0)->prop());
  EXPECT_EQ(Identifier::TRANSPARENT,
            decls->get(0)->values()->get(0)->GetIdentifier().ident());
  EXPECT_EQ(Property::BACKGROUND_POSITION_X, decls->get(4)->prop());
  EXPECT_EQ("100%", decls->get(4)->values()->get(0)->ToString());

  EXPECT_FALSE(ParseAndExpandBackground(""));
  EXPECT_FALSE(ParseAndExpandBackground(";"));
  EXPECT_FALSE(ParseAndExpandBackground("\"string\""));
  EXPECT_FALSE(ParseAndExpandBackground("normal"));

  decls.reset(ParseAndExpandBackground("inherit"));
  ASSERT_EQ(6, decls->size());
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(Identifier::INHERIT,
              decls->get(i)->values()->get(0)->GetIdentifier().ident());
  }

  EXPECT_FALSE(ParseAndExpandBackground("inherit none"));
  EXPECT_FALSE(ParseAndExpandBackground("none inherit"));

  decls.reset(ParseAndExpandBackground("none"));
  EXPECT_EQ(Identifier::TRANSPARENT,
            decls->get(0)->values()->get(0)->GetIdentifier().ident());
  EXPECT_EQ(Identifier::NONE,
            decls->get(1)->values()->get(0)->GetIdentifier().ident());
  EXPECT_EQ(Identifier::REPEAT,
            decls->get(2)->values()->get(0)->GetIdentifier().ident());
  EXPECT_EQ(Identifier::SCROLL,
            decls->get(3)->values()->get(0)->GetIdentifier().ident());

  decls.reset(ParseAndExpandBackground("fixed"));
  EXPECT_EQ(Identifier::FIXED,
            decls->get(3)->values()->get(0)->GetIdentifier().ident());

  decls.reset(ParseAndExpandBackground("transparent"));
  EXPECT_EQ(Identifier::TRANSPARENT,
            decls->get(0)->values()->get(0)->GetIdentifier().ident());

  // IE specific. Firefox should bail out.
  decls.reset(ParseAndExpandBackground("none url(abc)"));
  EXPECT_EQ(Value::URI, decls->get(1)->values()->get(0)->GetLexicalUnitType());

  decls.reset(ParseAndExpandBackground("none red fixed"));
  EXPECT_EQ("#ff0000",
            decls->get(0)->values()->get(0)->GetColorValue().ToString());
  EXPECT_EQ(Identifier::NONE,
            decls->get(1)->values()->get(0)->GetIdentifier().ident());
  EXPECT_EQ(Identifier::FIXED,
            decls->get(3)->values()->get(0)->GetIdentifier().ident());

  // The rest are position tests
  TestBackgroundPosition("none", "0%", "0%");
  TestBackgroundPosition("10", "10", "50%");
  TestBackgroundPosition("10 20%", "10", "20%");
  TestBackgroundPosition("10 100%", "10", "100%");
  TestBackgroundPosition("top left", "left", "top");
  TestBackgroundPosition("left top", "left", "top");
  TestBackgroundPosition("bottom", "50%", "bottom");
  TestBackgroundPosition("bottom center", "center", "bottom");
  TestBackgroundPosition("center bottom", "center", "bottom");
  TestBackgroundPosition("left", "left", "50%");
  TestBackgroundPosition("left center", "left", "center");
  TestBackgroundPosition("center left", "left", "center");
  TestBackgroundPosition("center", "center", "50%");
  TestBackgroundPosition("center center", "center", "center");
  TestBackgroundPosition("center 30%", "center", "30%");
  TestBackgroundPosition("30% center", "30%", "center");
  TestBackgroundPosition("30% bottom", "30%", "bottom");
  TestBackgroundPosition("left 30%", "left", "30%");
  TestBackgroundPosition("30% left", "left", "30%");
  // IE specific
  TestBackgroundPosition("30% 20% 50%", "30%", "20%");
  TestBackgroundPosition("bottom center right", "center", "bottom");
  TestBackgroundPosition("bottom right top", "right", "bottom");
  TestBackgroundPosition("bottom top right", "right", "top");
  TestBackgroundPosition("top right left", "right", "top");
  TestBackgroundPosition("right left top", "left", "top");
}

TEST_F(ParserTest, font_family) {
  scoped_ptr<Parser> a(new Parser(
      " Arial font 'Sans' system, menu new "));
  scoped_ptr<Values> t(new Values);

  EXPECT_TRUE(a->ParseFontFamily(t.get()));
  ASSERT_EQ(4, t->size());
  EXPECT_EQ(Value::IDENT, t->get(0)->GetLexicalUnitType());
  EXPECT_EQ("Arial font", UnicodeTextToUTF8(t->get(0)->GetIdentifierText()));
  EXPECT_EQ(Value::STRING, t->get(1)->GetLexicalUnitType());
  EXPECT_EQ("system", UnicodeTextToUTF8(t->get(2)->GetIdentifierText()));
  EXPECT_EQ("menu new", UnicodeTextToUTF8(t->get(3)->GetIdentifierText()));

  a.reset(new Parser("Verdana 3"));
  t.reset(new Values);
  EXPECT_FALSE(a->ParseFontFamily(t.get()));

  a.reset(new Parser("Verdana :"));
  t.reset(new Values);
  EXPECT_FALSE(a->ParseFontFamily(t.get()));

  a.reset(new Parser("Verdana ;"));
  t.reset(new Values);
  EXPECT_TRUE(a->ParseFontFamily(t.get()));
  ASSERT_EQ(1, t->size());
  EXPECT_EQ(Value::IDENT, t->get(0)->GetLexicalUnitType());
  EXPECT_EQ("Verdana", UnicodeTextToUTF8(t->get(0)->GetIdentifierText()));
}

TEST_F(ParserTest, font) {
  scoped_ptr<Parser> a(new Parser("font: caption"));
  scoped_ptr<Declarations> declarations(a->ParseDeclarations());
  const char expected_caption_expansion[] =
      "font: caption; "
      "font-style: normal; "
      "font-variant: normal; "
      "font-weight: normal; "
      "font-size: 10.6667px; "
      "line-height: normal; "
      "font-family: caption";
  EXPECT_EQ(expected_caption_expansion, declarations->ToString());

  a.reset(new Parser("font: inherit"));
  declarations.reset(a->ParseDeclarations());
  const char expected_inherit_expansion[] =
      "font: inherit; "
      "font-style: inherit; "
      "font-variant: inherit; "
      "font-weight: inherit; "
      "font-size: inherit; "
      "line-height: inherit; "
      "font-family: inherit";
  EXPECT_EQ(expected_inherit_expansion, declarations->ToString());

  a.reset(new Parser("normal 10px /120% Arial 'Sans'"));
  scoped_ptr<Values> t(a->ParseFont());
  ASSERT_EQ(7, t->size());
  EXPECT_DOUBLE_EQ(10, t->get(3)->GetFloatValue());
  EXPECT_EQ(Value::PERCENT, t->get(4)->GetDimension());

  a.reset(new Parser("italic 10px Arial, Sans"));
  t.reset(a->ParseFont());
  ASSERT_EQ(7, t->size());
  EXPECT_DOUBLE_EQ(10, t->get(3)->GetFloatValue());
  EXPECT_EQ(Identifier::NORMAL, t->get(4)->GetIdentifier().ident());

  a.reset(new Parser("SMALL-caps normal x-large Arial"));
  t.reset(a->ParseFont());
  ASSERT_EQ(6, t->size());
  EXPECT_EQ(Identifier::NORMAL, t->get(0)->GetIdentifier().ident());
  EXPECT_EQ(Identifier::SMALL_CAPS, t->get(1)->GetIdentifier().ident());
  EXPECT_EQ(Identifier::X_LARGE, t->get(3)->GetIdentifier().ident());
  EXPECT_EQ(Identifier::NORMAL, t->get(4)->GetIdentifier().ident());

  a.reset(new Parser("bolder 100 120 Arial"));
  t.reset(a->ParseFont());
  ASSERT_EQ(6, t->size());
  EXPECT_EQ(100, t->get(2)->GetIntegerValue());
  EXPECT_EQ(120, t->get(3)->GetIntegerValue());
  EXPECT_EQ(Identifier::NORMAL, t->get(4)->GetIdentifier().ident());

  a.reset(new Parser("10px normal"));
  t.reset(a->ParseFont());
  ASSERT_EQ(6, t->size());
  EXPECT_EQ(10, t->get(3)->GetIntegerValue());
  EXPECT_EQ(Identifier::NORMAL, t->get(5)->GetIdentifier().ident());

  a.reset(new Parser("normal 10px "));
  t.reset(a->ParseFont());
  EXPECT_EQ(5, t->size()) << "missing font-family should be allowed";

  a.reset(new Parser("10px/12pt "));
  t.reset(a->ParseFont());
  EXPECT_EQ(5, t->size()) << "missing font-family should be allowed";

  a.reset(new Parser("menu 10px"));
  t.reset(a->ParseFont());
  EXPECT_EQ(static_cast<Values *>(NULL), t.get())
      << "system font with extra value";

  a.reset(new Parser("Arial, menu "));
  t.reset(a->ParseFont());
  EXPECT_EQ(static_cast<Values *>(NULL), t.get()) << "missing font-size";

  a.reset(new Parser("transparent 10px "));
  t.reset(a->ParseFont());
  EXPECT_EQ(static_cast<Values *>(NULL), t.get()) << "unknown property";

  a.reset(new Parser("normal / 10px Arial"));
  t.reset(a->ParseFont());
  EXPECT_EQ(static_cast<Values *>(NULL), t.get())
      << "line-height without font-size";

  a.reset(new Parser("normal 10px/ Arial"));
  t.reset(a->ParseFont());
  EXPECT_EQ(static_cast<Values *>(NULL), t.get())
      << "slash without line-height";

  a.reset(new Parser("normal 10px Arial #333"));
  t.reset(a->ParseFont());
  EXPECT_EQ(static_cast<Values *>(NULL), t.get()) << "invalid type";
}

TEST_F(ParserTest, values) {
  scoped_ptr<Parser> a(new Parser(
      "rgb(12,25,30) url(blah) url('blah.png') 12% !important 'arial'"));
  scoped_ptr<Values> t(a->ParseValues(Property::OTHER));

  ASSERT_EQ(4, t->size());
  EXPECT_EQ(Value::COLOR, t->get(0)->GetLexicalUnitType());
  EXPECT_EQ(Value::URI, t->get(1)->GetLexicalUnitType());
  EXPECT_EQ(Value::URI, t->get(2)->GetLexicalUnitType());
  EXPECT_EQ(Value::NUMBER, t->get(3)->GetLexicalUnitType());
  EXPECT_EQ(Value::PERCENT, t->get(3)->GetDimension());

  a.reset(new Parser("rgb( 12,  25,30) @ignored  url( blah  )"
                " rect(12 10 auto 200px)"
                " { should be {nested }discarded } ident;"));
  t.reset(a->ParseValues(Property::OTHER));

  ASSERT_EQ(4, t->size());
  EXPECT_EQ(Value::COLOR, t->get(0)->GetLexicalUnitType());
  EXPECT_EQ(Value::URI, t->get(1)->GetLexicalUnitType());
  EXPECT_EQ(Value::RECT, t->get(2)->GetLexicalUnitType());
  EXPECT_EQ(Value::IDENT, t->get(3)->GetLexicalUnitType());
  EXPECT_EQ("ident", UnicodeTextToUTF8(t->get(3)->GetIdentifierText()));

  // test value copy constructor.
  scoped_ptr<Value> val(new Value(*(t->get(2))));
  EXPECT_EQ(Value::RECT, val->GetLexicalUnitType());
  ASSERT_EQ(4, val->GetParameters()->size());
  EXPECT_EQ(Value::NUMBER,
            val->GetParameters()->get(0)->GetLexicalUnitType());
  EXPECT_EQ(12, val->GetParameters()->get(0)->GetIntegerValue());
  EXPECT_EQ(Value::IDENT,
            val->GetParameters()->get(2)->GetLexicalUnitType());
  EXPECT_EQ("auto", UnicodeTextToUTF8(val->GetParameters()->get(2)
                                      ->GetIdentifierText()));
}

void ParseFontFamily(Parser* parser) {
  Values values;
  parser->ParseFontFamily(&values);
}

TEST_F(ParserTest, ParseBlock) {
  static const char* truetestcases[] = {
    "{{{{}}}} serif",
    "{ {  { {  }    }   }    } serif",  // whitespace
    "{@ident1{{ @ident {}}}} serif",    // @-idents
    "{{}} @ident {{}} @ident serif",    // multiple blocks
    "{{ident{{}ident2}}} serif",        // idents
  };
  for (int i = 0; i < arraysize(truetestcases); ++i) {
    SCOPED_TRACE(truetestcases[i]);
    Values values;
    EXPECT_TRUE(Parser(truetestcases[i]).ParseFontFamily(&values));
    ASSERT_EQ(1, values.size());
    EXPECT_EQ(Value::IDENT, values.get(0)->GetLexicalUnitType());
    EXPECT_EQ("serif", UnicodeTextToUTF8(values.get(0)->GetIdentifierText()));
  }

  static const char* falsetestcases[] = {
    "{{{{}}} serif",  // too many opens
    "{{{{}}}}} serif",  // too many closes
    "{{{{}}}}}",  // no tokenxs
  };
  for (int i = 0; i < arraysize(falsetestcases); ++i) {
    SCOPED_TRACE(falsetestcases[i]);
    Values values;
    Parser(falsetestcases[i]).ParseFontFamily(&values);
    EXPECT_EQ(0, values.size());
  }

}


TEST_F(ParserTest, declarations) {
  scoped_ptr<Parser> a(new Parser(
      "color: #333; line-height: 1.3;"
      "text-align: justify; font-family: \"Gill Sans MT\","
      "\"Gill Sans\", GillSans, Arial, Helvetica, sans-serif"));
  scoped_ptr<Declarations> t(a->ParseDeclarations());

  // Declarations is a vector of Declaration, and we go through them:
  ASSERT_EQ(4, t->size());
  EXPECT_EQ(Property::COLOR, t->get(0)->prop());
  EXPECT_EQ(Property::LINE_HEIGHT, t->get(1)->prop());
  EXPECT_EQ(Property::TEXT_ALIGN, t->get(2)->prop());
  EXPECT_EQ(Property::FONT_FAMILY, t->get(3)->prop());

  ASSERT_EQ(1, t->get(0)->values()->size());
  EXPECT_EQ(Value::COLOR, t->get(0)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ("#333333", t->get(0)->values()->get(0)->GetColorValue().ToString());

  ASSERT_EQ(6, t->get(3)->values()->size());
  EXPECT_EQ(Value::STRING,
            t->get(3)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ("Gill Sans MT",
            UnicodeTextToUTF8(t->get(3)->values()->get(0)->GetStringValue()));

  a.reset(new Parser(
      "background-color: 333; color: \"abcdef\";"
      "background-color: #red; color: \"white\";"
      "background-color: rgb(255, 10%, 10)"));
  t.reset(a->ParseDeclarations());

  ASSERT_EQ(4, t->size()) << "#red is not valid";
  EXPECT_EQ(Property::BACKGROUND_COLOR, t->get(0)->prop());
  EXPECT_EQ(Property::COLOR, t->get(1)->prop());
  EXPECT_EQ(Property::COLOR, t->get(2)->prop());
  EXPECT_EQ(Property::BACKGROUND_COLOR, t->get(3)->prop());
  ASSERT_EQ(1, t->get(0)->values()->size());
  EXPECT_EQ(Value::COLOR, t->get(0)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ("#333333", t->get(0)->values()->get(0)->GetColorValue().ToString());
  ASSERT_EQ(1, t->get(1)->values()->size());
  EXPECT_EQ(Value::COLOR, t->get(1)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ("#abcdef", t->get(1)->values()->get(0)->GetColorValue().ToString());
  ASSERT_EQ(1, t->get(2)->values()->size());
  EXPECT_EQ(Value::COLOR, t->get(2)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ("#ffffff", t->get(2)->values()->get(0)->GetColorValue().ToString());
  ASSERT_EQ(1, t->get(3)->values()->size());
  EXPECT_EQ(Value::COLOR, t->get(3)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ("#ff190a", t->get(3)->values()->get(0)->GetColorValue().ToString());

  // expand background
  a.reset(new Parser("background: #333 fixed no-repeat; "));
  t.reset(a->ParseDeclarations());
  ASSERT_EQ(7, t->size());
  EXPECT_EQ(Property::BACKGROUND, t->get(0)->prop());
  EXPECT_EQ(3, t->get(0)->values()->size());
  EXPECT_EQ(Property::BACKGROUND_COLOR, t->get(1)->prop());
  EXPECT_EQ(1, t->get(1)->values()->size());
  EXPECT_EQ(Property::BACKGROUND_IMAGE, t->get(2)->prop());
  EXPECT_EQ(1, t->get(2)->values()->size());
  EXPECT_EQ(Property::BACKGROUND_REPEAT, t->get(3)->prop());
  EXPECT_EQ(1, t->get(3)->values()->size());
  EXPECT_EQ(Property::BACKGROUND_ATTACHMENT, t->get(4)->prop());
  EXPECT_EQ(1, t->get(4)->values()->size());
  EXPECT_EQ(Property::BACKGROUND_POSITION_X, t->get(5)->prop());
  EXPECT_EQ(1, t->get(5)->values()->size());
  EXPECT_EQ(Property::BACKGROUND_POSITION_Y, t->get(6)->prop());
  EXPECT_EQ(1, t->get(6)->values()->size());

  // expand font
  a.reset(new Parser("font: small-caps 24px Arial 'Sans', monospace; "));
  t.reset(a->ParseDeclarations());
  ASSERT_EQ(7, t->size());
  EXPECT_EQ(Property::FONT, t->get(0)->prop());
  EXPECT_EQ(8, t->get(0)->values()->size());
  EXPECT_EQ(Property::FONT_STYLE, t->get(1)->prop());
  EXPECT_EQ(Property::FONT_VARIANT, t->get(2)->prop());
  EXPECT_EQ(Property::FONT_WEIGHT, t->get(3)->prop());
  EXPECT_EQ(Property::FONT_SIZE, t->get(4)->prop());
  EXPECT_EQ(Property::LINE_HEIGHT, t->get(5)->prop());
  EXPECT_EQ(Property::FONT_FAMILY, t->get(6)->prop());
  ASSERT_EQ(3, t->get(6)->values()->size());
  EXPECT_EQ("monospace", UnicodeTextToUTF8(t->get(6)->values()->get(2)->
                                           GetIdentifierText()));

  a.reset(new Parser(
      "{font-size: #333; color:red"));
  t.reset(a->ParseDeclarations());
  ASSERT_EQ(1, t->size());
  EXPECT_EQ(Property::COLOR, t->get(0)->prop());

  a.reset(new Parser(
      "{font-size: #333; color:red"));
  a->set_quirks_mode(false);
  t.reset(a->ParseDeclarations());
  EXPECT_EQ(0, t->size());

  a.reset(new Parser(
      "font-size {background: #333; color:red"));
  t.reset(a->ParseDeclarations());
  ASSERT_EQ(1, t->size());
  EXPECT_EQ(Property::COLOR, t->get(0)->prop());

  a.reset(new Parser(
      "font-size {background: #333; color:red"));
  a->set_quirks_mode(false);
  t.reset(a->ParseDeclarations());
  EXPECT_EQ(0, t->size());

  a.reset(new Parser(
      "font-size }background: #333; color:red"));
  t.reset(a->ParseDeclarations());
  EXPECT_EQ(0, t->size());

  a.reset(new Parser(
      "font-size }background: #333; color:red"));
  a->set_quirks_mode(false);
  t.reset(a->ParseDeclarations());
  EXPECT_EQ(0, t->size());

  a.reset(new Parser(
      "top:1px; {font-size: #333; color:red}"));
  t.reset(a->ParseDeclarations());
  ASSERT_EQ(2, t->size());
  EXPECT_EQ(Property::TOP, t->get(0)->prop());
  EXPECT_EQ(Property::COLOR, t->get(1)->prop());

  a.reset(new Parser(
      "top:1px; {font-size: #333; color:red}"));
  a->set_quirks_mode(false);
  t.reset(a->ParseDeclarations());
  ASSERT_EQ(1, t->size());
  EXPECT_EQ(Property::TOP, t->get(0)->prop());
}

TEST_F(ParserTest, illegal_constructs) {
  scoped_ptr<Parser> a(new Parser("width: {$width}"));
  scoped_ptr<Declarations> t(a->ParseDeclarations());

  // From CSS2.1 spec http://www.w3.org/TR/CSS2/syndata.html#parsing-errors:
  //   User agents must ignore a declaration with an illegal value.
  EXPECT_EQ(0, t->size());

  a.reset(new Parser("font-family: \"Gill Sans MT;"));
  t.reset(a->ParseDeclarations());

  ASSERT_EQ(1, t->size());
  EXPECT_EQ(Property::FONT_FAMILY, t->get(0)->prop());
  ASSERT_EQ(1, t->get(0)->values()->size());
  EXPECT_EQ(Value::STRING, t->get(0)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ("Gill Sans MT;",
            UnicodeTextToUTF8(t->get(0)->values()->get(0)->GetStringValue()));

  a.reset(new Parser("font-family: 'Gill Sans MT"));
  t.reset(a->ParseDeclarations());

  ASSERT_EQ(1, t->size());
  EXPECT_EQ(Property::FONT_FAMILY, t->get(0)->prop());
  ASSERT_EQ(1, t->get(0)->values()->size());
  EXPECT_EQ(Value::STRING, t->get(0)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ("Gill Sans MT",
            UnicodeTextToUTF8(t->get(0)->values()->get(0)->GetStringValue()));
}

TEST_F(ParserTest, value_validation) {
  scoped_ptr<Parser> a(new Parser("width: {$width}"));
  scoped_ptr<Declarations> t(a->ParseDeclarations());

  // Let's take border-color as an example. It only accepts color and the
  // transparent keyword in particular (and inherit is a common one).
  a.reset(new Parser(
      "border-color: \"string\"; "
      "border-color: url(\"abc\"); "
      "border-color: 12345; "
      "border-color: none; "
      "border-color: inherited; "
      "border-color: red; "
      "border-color: #123456; "
      "border-color: transparent; "
      "border-color: inherit; "
      "border-color: unknown; "
      ));
  t.reset(a->ParseDeclarations());

  ASSERT_EQ(4, t->size());
  EXPECT_EQ(Value::COLOR, t->get(0)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ(Value::COLOR, t->get(1)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ(Value::IDENT, t->get(2)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ(Identifier::TRANSPARENT,
            t->get(2)->values()->get(0)->GetIdentifier().ident());
  EXPECT_EQ(Value::IDENT, t->get(3)->values()->get(0)->GetLexicalUnitType());
  EXPECT_EQ(Identifier::INHERIT,
            t->get(3)->values()->get(0)->GetIdentifier().ident());
}

TEST_F(ParserTest, universalselector) {
  Parser p("*");
  scoped_ptr<SimpleSelectors> t(p.ParseSimpleSelectors(false));

  EXPECT_EQ(SimpleSelectors::NONE, t->combinator());
  ASSERT_EQ(1, t->size());
  EXPECT_EQ(SimpleSelector::UNIVERSAL, t->get(0)->type());
}

TEST_F(ParserTest, universalselectorcondition) {
  scoped_ptr<Parser> a(new Parser(" *[foo=bar]"));
  scoped_ptr<SimpleSelectors> t(a->ParseSimpleSelectors(true));

  EXPECT_EQ(SimpleSelectors::DESCENDANT, t->combinator());
  ASSERT_EQ(2, t->size());
  EXPECT_EQ(SimpleSelector::UNIVERSAL, t->get(0)->type());
  EXPECT_EQ(SimpleSelector::EXACT_ATTRIBUTE, t->get(1)->type());

  a.reset(new Parser(" *[foo="));
  t.reset(a->ParseSimpleSelectors(true));

  // This is not a valid selector.
  EXPECT_FALSE(t.get());
}

TEST_F(ParserTest, comment_breaking_descendant_combinator) {
  Parser p(" a b/*foo*/c /*foo*/d/*foo*/ e { }");
  scoped_ptr<Ruleset> t(p.ParseRuleset());

  ASSERT_EQ(1, t->selectors().size());
  const Selector* s = t->selectors().get(0);
  ASSERT_EQ(5, s->size());
  EXPECT_EQ(SimpleSelectors::NONE, s->get(0)->combinator());
  EXPECT_EQ(SimpleSelectors::DESCENDANT, s->get(1)->combinator());
  EXPECT_EQ(SimpleSelectors::DESCENDANT, s->get(2)->combinator());
  EXPECT_EQ(SimpleSelectors::DESCENDANT, s->get(3)->combinator());
  EXPECT_EQ(SimpleSelectors::DESCENDANT, s->get(4)->combinator());
}

TEST_F(ParserTest, comment_breaking_child_combinator) {
  Parser p(" a >b/*f>oo*/>c /*fo>o*/>d/*f>oo*/ > e>f { }");
  scoped_ptr<Ruleset> t(p.ParseRuleset());

  ASSERT_EQ(1, t->selectors().size());
  const Selector* s = t->selectors().get(0);
  ASSERT_EQ(6, s->size());
  EXPECT_EQ(SimpleSelectors::NONE, s->get(0)->combinator());
  EXPECT_EQ(SimpleSelectors::CHILD, s->get(1)->combinator());
  EXPECT_EQ(SimpleSelectors::CHILD, s->get(2)->combinator());
  EXPECT_EQ(SimpleSelectors::CHILD, s->get(3)->combinator());
  EXPECT_EQ(SimpleSelectors::CHILD, s->get(4)->combinator());
  EXPECT_EQ(SimpleSelectors::CHILD, s->get(5)->combinator());
}

TEST_F(ParserTest, ruleset_starts_with_combinator) {
  Parser p(" >a { }");
  scoped_ptr<Ruleset> t(p.ParseRuleset());

  // This is not a valid selector.
  EXPECT_FALSE(t.get());
}


TEST_F(ParserTest, simple_selectors) {
  // First, a basic case
  scoped_ptr<Parser> a(new Parser("*[lang|=fr]"));
  scoped_ptr<SimpleSelectors> t(a->ParseSimpleSelectors(false));

  EXPECT_EQ(SimpleSelectors::NONE, t->combinator());

  {
    const SimpleSelector* c = t->get(0);
    ASSERT_TRUE(c != NULL);
    ASSERT_EQ(SimpleSelector::UNIVERSAL, c->type());
  }
  {
    const SimpleSelector* c = t->get(1);
    ASSERT_TRUE(c != NULL);
    ASSERT_EQ(SimpleSelector::BEGIN_HYPHEN_ATTRIBUTE, c->type());
    EXPECT_EQ("lang", UnicodeTextToUTF8(c->attribute()));
    EXPECT_EQ("fr", UnicodeTextToUTF8(c->value()));
  }

  // Now, a very complex one.
  a.reset(new Parser(
      "> P:first_child:hover[class~='hidden'][width]#content"
      "[id*=logo][id^=logo][id$=\"logo\"]"
      "[lang=en].anotherclass.moreclass #next"));
  t.reset(a->ParseSimpleSelectors(true));

  EXPECT_EQ(SimpleSelectors::CHILD, t->combinator());

  // We're going to go through the conditions in reverse order, for kicks.
  SimpleSelectors::const_reverse_iterator it = t->rbegin();

  // .moreclass
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::CLASS, c->type());
    EXPECT_EQ("moreclass", UnicodeTextToUTF8(c->value()));
  }

  // .anotherclass
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::CLASS, c->type());
    EXPECT_EQ("anotherclass", UnicodeTextToUTF8(c->value()));
  }

  // EXACT_ATTRIBUTE [lang=en]
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::EXACT_ATTRIBUTE, c->type());
    EXPECT_EQ("lang", UnicodeTextToUTF8(c->attribute()));
    EXPECT_EQ("en", UnicodeTextToUTF8(c->value()));
  }

  // END_WITH_ATTRIBUTE [id$="logo"]
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::END_WITH_ATTRIBUTE, c->type());
    EXPECT_EQ("id", UnicodeTextToUTF8(c->attribute()));
    EXPECT_EQ("logo", UnicodeTextToUTF8(c->value()));
  }

  // BEGIN_WITH_ATTRIBUTE [id^=logo]
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::BEGIN_WITH_ATTRIBUTE, c->type());
    EXPECT_EQ("id", UnicodeTextToUTF8(c->attribute()));
    EXPECT_EQ("logo", UnicodeTextToUTF8(c->value()));
  }

  // SUBSTRING_ATTRIBUTE [id*=logo]
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::SUBSTRING_ATTRIBUTE, c->type());
    EXPECT_EQ("id", UnicodeTextToUTF8(c->attribute()));
    EXPECT_EQ("logo", UnicodeTextToUTF8(c->value()));
  }

  // ID #content
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::ID, c->type());
    EXPECT_EQ("content", UnicodeTextToUTF8(c->value()));
  }

  // EXIST_ATTRIBUTE [width]
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::EXIST_ATTRIBUTE, c->type());
    EXPECT_EQ("width", UnicodeTextToUTF8(c->attribute()));
  }

  // ONE_OF_ATTRIBUTE [class~=hidden]
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::ONE_OF_ATTRIBUTE, c->type());
    EXPECT_EQ("class", UnicodeTextToUTF8(c->attribute()));
    EXPECT_EQ("hidden", UnicodeTextToUTF8(c->value()));
  }

  // PSEUDOCLASS :hover
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::PSEUDOCLASS, c->type());
    EXPECT_EQ("hover", UnicodeTextToUTF8(c->pseudoclass()));
  }

  // PSEUDOCLASS :first_child
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::PSEUDOCLASS, c->type());
    EXPECT_EQ("first_child", UnicodeTextToUTF8(c->pseudoclass()));
  }

  // P
  {
    SimpleSelector* c = *it++;
    ASSERT_EQ(SimpleSelector::ELEMENT_TYPE, c->type());
    EXPECT_EQ("P", UnicodeTextToUTF8(c->element_text()));
    EXPECT_EQ(kHtmlTagP, c->element_type());
  }


  ASSERT_TRUE(it == t->rend());
}

TEST_F(ParserTest, bad_simple_selectors) {
  scoped_ptr<Parser> a;
  scoped_ptr<SimpleSelectors> t;

  // valid or not?
  a.reset(new Parser(""));
  t.reset(a->ParseSimpleSelectors(false));
  EXPECT_FALSE(t.get());

  a.reset(new Parser("{}"));
  t.reset(a->ParseSimpleSelectors(false));
  EXPECT_FALSE(t.get());

  a.reset(new Parser("#"));
  t.reset(a->ParseSimpleSelectors(false));
  EXPECT_FALSE(t.get());

  a.reset(new Parser("# {"));
  t.reset(a->ParseSimpleSelectors(false));
  EXPECT_FALSE(t.get());

  a.reset(new Parser("#{"));
  t.reset(a->ParseSimpleSelectors(false));
  EXPECT_FALSE(t.get());

  a.reset(new Parser("##"));
  t.reset(a->ParseSimpleSelectors(false));
  EXPECT_FALSE(t.get());

  a.reset(new Parser("*[class="));
  t.reset(a->ParseSimpleSelectors(false));
  EXPECT_FALSE(t.get());

  a.reset(new Parser("*[class=hidden];"));
  t.reset(a->ParseSimpleSelectors(false));
  EXPECT_FALSE(t.get());

  a.reset(new Parser("*[class=hidden].;"));
  t.reset(a->ParseSimpleSelectors(false));
  EXPECT_FALSE(t.get());

  a.reset(new Parser("#a {"));
  t.reset(a->ParseSimpleSelectors(false));
  EXPECT_TRUE(t.get());
}

TEST_F(ParserTest, selectors) {
  scoped_ptr<Parser> a(new Parser("h1 p #id {"));
  scoped_ptr<Selectors> t(a->ParseSelectors());
  EXPECT_TRUE(t.get());
  ASSERT_EQ(1, t->size());
  EXPECT_EQ(3, (*t)[0]->size());
  EXPECT_EQ('{', *a->getpos());

  a.reset(new Parser(" h1 p #id , div.p > h2 > div.t #id"));
  t.reset(a->ParseSelectors());
  EXPECT_TRUE(t.get());
  ASSERT_EQ(2, t->size());
  EXPECT_EQ(3, (*t)[0]->size());
  EXPECT_EQ(4, (*t)[1]->size());
  EXPECT_TRUE(a->Done());

  a.reset(new Parser("/*c*/h1 p #id/*c*/,/*c*/div.p > h2 > div.t #id/*c*/"));
  t.reset(a->ParseSelectors());
  EXPECT_TRUE(t.get());
  ASSERT_EQ(2, t->size());
  EXPECT_EQ(3, (*t)[0]->size());
  EXPECT_EQ(4, (*t)[1]->size());
  EXPECT_TRUE(a->Done());

  a.reset(new Parser("{}"));
  t.reset(a->ParseSelectors());
  EXPECT_FALSE(t.get());
  EXPECT_EQ('{', *a->getpos());

  a.reset(new Parser(""));
  t.reset(a->ParseSelectors());
  EXPECT_FALSE(t.get());
  EXPECT_TRUE(a->Done());

  a.reset(new Parser("  ,h1 p #id {"));
  t.reset(a->ParseSelectors());
  EXPECT_FALSE(t.get());
  EXPECT_EQ('{', *a->getpos());

  a.reset(new Parser("  , {"));
  t.reset(a->ParseSelectors());
  EXPECT_FALSE(t.get());
  EXPECT_EQ('{', *a->getpos());

  a.reset(new Parser("h1 p #id, {"));
  t.reset(a->ParseSelectors());
  EXPECT_FALSE(t.get());
  EXPECT_EQ('{', *a->getpos());

  a.reset(new Parser("h1 p #id, {"));
  t.reset(a->ParseSelectors());
  EXPECT_FALSE(t.get());
  EXPECT_EQ('{', *a->getpos());

  a.reset(new Parser("h1 p #id;"));
  t.reset(a->ParseSelectors());
  EXPECT_FALSE(t.get());
  EXPECT_TRUE(a->Done());

  a.reset(new Parser(" h1 p[class=/*{*/ #id , h2 #id"));
  t.reset(a->ParseSelectors());
  EXPECT_FALSE(t.get());
  EXPECT_TRUE(a->Done());

  a.reset(new Parser(" h1 #id. , h2 #id"));
  t.reset(a->ParseSelectors());
  EXPECT_FALSE(t.get());
  EXPECT_TRUE(a->Done());
}

TEST_F(ParserTest, rulesets) {
  scoped_ptr<Parser> a(new Parser("h1 p #id ;"));
  scoped_ptr<Ruleset> t(a->ParseRuleset());

  EXPECT_EQ(static_cast<Ruleset *>(NULL), t.get());

  a.reset(new Parser(", { }"));
  t.reset(a->ParseRuleset());

  EXPECT_FALSE(t.get());

  a.reset(new Parser(", h1 p #id, { };"));
  t.reset(a->ParseRuleset());

  EXPECT_FALSE(t.get());

  a.reset(new Parser(
      "h1 p + #id { font-size: 7px; width:10pt !important;}"));
  t.reset(a->ParseRuleset());

  ASSERT_EQ(1, t->selectors().size());
  ASSERT_EQ(3, t->selector(0).size());
  EXPECT_EQ(SimpleSelectors::SIBLING,
            t->selectors()[0]->at(2)->combinator());
  ASSERT_EQ(2, t->declarations().size());
  EXPECT_EQ(false, t->declarations()[0]->IsImportant());
  EXPECT_EQ(Property::WIDTH, t->declarations()[1]->prop());
  EXPECT_EQ(true, t->declarations()[1]->IsImportant());

  a.reset(new Parser("h1 p + #id , h1:first_child { font-size: 10px; }"));
  t.reset(a->ParseRuleset());

  ASSERT_EQ(2, t->selectors().size());
  ASSERT_EQ(3, t->selector(0).size());
  ASSERT_EQ(1, t->selector(1).size());
  EXPECT_EQ(SimpleSelectors::SIBLING,
            t->selectors()[0]->at(2)->combinator());
  ASSERT_EQ(1, t->declarations().size());
  EXPECT_EQ(false, t->declarations()[0]->IsImportant());
}

TEST_F(ParserTest, atrules) {
  scoped_ptr<Parser> a(new Parser(
      "@IMPORT url(assets/style.css) screen,printer"));
  scoped_ptr<Stylesheet> t(new Stylesheet());
  a->ParseAtRule(t.get());

  ASSERT_EQ(1, t->imports().size());
  EXPECT_EQ("assets/style.css", UnicodeTextToUTF8(t->import(0).link));
  EXPECT_EQ(true, a->Done());

  a.reset(new Parser("@charset \"ISO-8859-1\" ;"));
  t.reset(new Stylesheet());
  a->ParseAtRule(t.get());

  EXPECT_EQ(true, a->Done());

  a.reset(new Parser(
      "@media print,screen {\n\tbody { font-size: 10pt }\n}"));
  t.reset(new Stylesheet());
  a->ParseAtRule(t.get());

  ASSERT_EQ(1, t->rulesets().size());
  ASSERT_EQ(1, t->ruleset(0).selectors().size());
  ASSERT_EQ(2, t->ruleset(0).media().size());
  EXPECT_EQ("print", UnicodeTextToUTF8(t->ruleset(0).medium(0)));
  EXPECT_EQ("screen", UnicodeTextToUTF8(t->ruleset(0).medium(1)));
  ASSERT_EQ(1, t->ruleset(0).selectors()[0]->size());
  EXPECT_EQ(kHtmlTagBody,
            t->ruleset(0).selector(0)[0]->get(0)->element_type());
  ASSERT_EQ(1, t->ruleset(0).declarations().size());
  EXPECT_EQ(Property::FONT_SIZE,
            t->ruleset(0).declarations()[0]->prop());
  EXPECT_EQ(true, a->Done());

  a.reset(new Parser(
      "@page :left { margin-left: 4cm; margin-right: 3cm; }"));
  t.reset(new Stylesheet());
  a->ParseAtRule(t.get());

  EXPECT_EQ(0, t->rulesets().size());
  EXPECT_EQ(true, a->Done());

  // Make sure media strings can be shared between multiple rulesets.
  a.reset(new Parser(
      "@media print { a { color: red; }  p { color: blue; } }"));
  t.reset(new Stylesheet());
  a->ParseAtRule(t.get());

  ASSERT_EQ(2, t->rulesets().size());
  EXPECT_EQ("print", UnicodeTextToUTF8(t->ruleset(0).medium(0)));
  EXPECT_EQ("print", UnicodeTextToUTF8(t->ruleset(1).medium(0)));
  t->ToString(); // Make sure it can be written as a string.
}

TEST_F(ParserTest, stylesheets) {
  scoped_ptr<Parser> a(new Parser("\n"
    "\t@import \"mystyle.css\" all; "
    "@import url(\"mystyle.css\" );\n"
    "\tBODY {\n"
    "color:black !important; \n"
    "background: white !important; }\n"
    "* {\n"
    "\tcolor: inherit !important;\n"
    "background: transparent;\n"
    "}\n"
    "\n"
    "<!-- html comments * { font-size: 1 } -->\n"
    "H1 + *[REL-up] {}"));

  scoped_ptr<Stylesheet> t(a->ParseStylesheet());
  EXPECT_EQ(Parser::kNoError, a->errors_seen_mask());
  ASSERT_EQ(2, t->imports().size());
  EXPECT_EQ("mystyle.css", UnicodeTextToUTF8(t->import(0).link));
  ASSERT_EQ(1, t->import(0).media.size());
  EXPECT_EQ("all", UnicodeTextToUTF8(t->import(0).media[0]));
  EXPECT_EQ("mystyle.css", UnicodeTextToUTF8(t->import(1).link));
  // html-style comment should NOT work
  EXPECT_EQ(4, t->rulesets().size());
  EXPECT_TRUE(a->Done());
}

TEST_F(ParserTest, ParseRawStylesheetDoesNotExpand) {
  {
    Parser p("a { background: none; }");
    scoped_ptr<Stylesheet> stylesheet(p.ParseRawStylesheet());
    ASSERT_EQ(1, stylesheet->rulesets().size());
    ASSERT_EQ(1, stylesheet->ruleset(0).declarations().size());
    ASSERT_EQ(1, stylesheet->ruleset(0).declaration(0).values()->size());
    EXPECT_TRUE(p.Done());
  }
  {
    Parser p("a { font: 12px verdana; }");
    scoped_ptr<Stylesheet> stylesheet(p.ParseRawStylesheet());
    ASSERT_EQ(1, stylesheet->rulesets().size());
    ASSERT_EQ(1, stylesheet->ruleset(0).declarations().size());
    const Values& values = *stylesheet->ruleset(0).declaration(0).values();
    ASSERT_EQ(6, values.size());
    // ParseRaw will expand the values out to:
    // font: normal normal normal 12px/normal verdana
    // But it will not expand out the six other declarations.
    // TODO(sligocki): There has got to be a nicer way to test this.
    EXPECT_EQ(Identifier::NORMAL, values[0]->GetIdentifier().ident());
    EXPECT_EQ(Identifier::NORMAL, values[1]->GetIdentifier().ident());
    EXPECT_EQ(Identifier::NORMAL, values[2]->GetIdentifier().ident());
    EXPECT_DOUBLE_EQ(12.0, values[3]->GetFloatValue());
    EXPECT_EQ(Value::PX, values[3]->GetDimension());
    EXPECT_EQ(Identifier::NORMAL, values[4]->GetIdentifier().ident());
    EXPECT_EQ("verdana", UnicodeTextToUTF8(values[5]->GetIdentifierText()));
    EXPECT_TRUE(p.Done());
  }
}

TEST_F(ParserTest, ParseStylesheetDoesExpand) {
  {
    Parser p("a { background: none; }");
    scoped_ptr<Stylesheet> stylesheet(p.ParseStylesheet());
    ASSERT_EQ(1, stylesheet->rulesets().size());
    const Declarations& declarations = stylesheet->ruleset(0).declarations();
    ASSERT_EQ(7, declarations.size());
    EXPECT_EQ(Property::BACKGROUND, declarations[0]->prop());
    EXPECT_EQ(Property::BACKGROUND_COLOR, declarations[1]->prop());
    EXPECT_EQ(Property::BACKGROUND_IMAGE, declarations[2]->prop());
    EXPECT_EQ(Property::BACKGROUND_REPEAT, declarations[3]->prop());
    EXPECT_EQ(Property::BACKGROUND_ATTACHMENT, declarations[4]->prop());
    EXPECT_EQ(Property::BACKGROUND_POSITION_X, declarations[5]->prop());
    EXPECT_EQ(Property::BACKGROUND_POSITION_Y, declarations[6]->prop());
    EXPECT_TRUE(p.Done());
  }
  {
    Parser p("a { font: 12px verdana; }");
    scoped_ptr<Stylesheet> stylesheet(p.ParseStylesheet());
    ASSERT_EQ(1, stylesheet->rulesets().size());
    const Declarations& declarations = stylesheet->ruleset(0).declarations();
    ASSERT_EQ(7, declarations.size());
    EXPECT_EQ(Property::FONT, declarations[0]->prop());
    EXPECT_EQ(6, declarations[0]->values()->size());
    EXPECT_EQ(Property::FONT_STYLE, declarations[1]->prop());
    EXPECT_EQ(Property::FONT_VARIANT, declarations[2]->prop());
    EXPECT_EQ(Property::FONT_WEIGHT, declarations[3]->prop());
    EXPECT_EQ(Property::FONT_SIZE, declarations[4]->prop());
    EXPECT_EQ(Property::LINE_HEIGHT, declarations[5]->prop());
    EXPECT_EQ(Property::FONT_FAMILY, declarations[6]->prop());
  }
}

TEST_F(ParserTest, percentage_colors) {
  Value hundred(100.0, Value::PERCENT);
  EXPECT_EQ(255, Parser::ValueToRGB(&hundred));
  Value zero(0.0, Value::PERCENT);
  EXPECT_EQ(0, Parser::ValueToRGB(&zero));
}

TEST_F(ParserTest, value_equality) {
  Value hundred(100.0, Value::PERCENT);
  Value hundred2(100.0, Value::PERCENT);
  Value zero(0.0, Value::PERCENT);
  Identifier auto_ident(Identifier::AUTO);
  Value ident(auto_ident);
  EXPECT_TRUE(hundred.Equals(hundred2));
  EXPECT_FALSE(hundred.Equals(zero));
  EXPECT_FALSE(hundred.Equals(ident));
}

TEST_F(ParserTest, Utf8Error) {
  Parser p("font-family: \"\xCB\xCE\xCC\xE5\"");
  scoped_ptr<Declarations> declarations(p.ParseDeclarations());
  EXPECT_EQ(1, declarations->size());
  EXPECT_EQ(Parser::kUtf8Error, p.errors_seen_mask());
}

TEST_F(ParserTest, DeclarationError) {
  Parser p("font-family ; ");
  scoped_ptr<Declarations> declarations(p.ParseDeclarations());
  EXPECT_EQ(0, declarations->size());
  EXPECT_EQ(Parser::kDeclarationError, p.errors_seen_mask());
}

TEST_F(ParserTest, SelectorError) {
  Parser p(".bold: { font-weight: bold }");
  scoped_ptr<Stylesheet> stylesheet(p.ParseStylesheet());
  EXPECT_EQ(0, stylesheet->rulesets().size());
  EXPECT_EQ(Parser::kSelectorError, p.errors_seen_mask());

  Parser p2("div:nth-child(1n) { color: red; }");
  stylesheet.reset(p2.ParseStylesheet());
  EXPECT_EQ(Parser::kSelectorError, p2.errors_seen_mask());
  // Note: We fail to parse the (1n). If this is fixed, this test should be
  // updated accordingly.
  EXPECT_EQ("/* AUTHOR */\n\n\ndiv:nth-child {color: #ff0000}\n",
            stylesheet->ToString());

  Parser p3("}}");
  stylesheet.reset(p3.ParseStylesheet());
  EXPECT_EQ(0, stylesheet->rulesets().size());
  EXPECT_EQ(Parser::kSelectorError, p3.errors_seen_mask());

  Parser p4("div[too=many=equals] { color: red; }");
  stylesheet.reset(p4.ParseStylesheet());
  EXPECT_EQ(Parser::kSelectorError, p4.errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n\n\ndiv[too=many] {color: #ff0000}\n",
            stylesheet->ToString());
}

TEST_F(ParserTest, MediaError) {
  Parser p("@media screen and (max-width: 290px) {}");
  scoped_ptr<Stylesheet> stylesheet(p.ParseStylesheet());
  EXPECT_EQ(0, stylesheet->rulesets().size());
  EXPECT_TRUE(Parser::kMediaError & p.errors_seen_mask());
}

TEST_F(ParserTest, HtmlCommentError) {
  Parser good("<!-- a { color: red } -->");
  scoped_ptr<Stylesheet> stylesheet(good.ParseStylesheet());
  EXPECT_EQ(Parser::kNoError, good.errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n\n\na {color: #ff0000}\n", stylesheet->ToString());

  const char* bad_strings[] = {
    "<    a { color: red } -->",
    "<!   a { color: red } -->",
    "<!-  a { color: red } -->",
    "<!-- a { color: red } --",
    "<!-- a { color: red } ->",
    "<!-- a { color: red } -",
    "<>a { color: red }",
    };

  for (int i = 0; i < arraysize(bad_strings); ++i) {
    Parser bad(bad_strings[i]);
    stylesheet.reset(bad.ParseStylesheet());
    EXPECT_TRUE(Parser::kHtmlCommentError & bad.errors_seen_mask());
  }
}

TEST_F(ParserTest, ValueError) {
  Parser p("(12)");
  scoped_ptr<Value> value(p.ParseAny());
  EXPECT_TRUE(Parser::kValueError & p.errors_seen_mask());
  EXPECT_TRUE(NULL == value.get());
}

TEST_F(ParserTest, SkippedTokenError) {
  Parser p("12pt @foo Arial");
  scoped_ptr<Values> values(p.ParseValues(Property::FONT));
  EXPECT_TRUE(Parser::kSkippedTokenError & p.errors_seen_mask());
  EXPECT_EQ("12pt Arial", values->ToString());
}

TEST_F(ParserTest, CharsetError) {
  Parser p("@charset \"UTF-8\";");
  scoped_ptr<Stylesheet> stylesheet(p.ParseStylesheet());
  EXPECT_EQ(Parser::kNoError, p.errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n@charset \"UTF-8\";\n\n\n", stylesheet->ToString());

  Parser p2("@charset foobar;");
  stylesheet.reset(p2.ParseStylesheet());
  EXPECT_EQ(Parser::kCharsetError, p2.errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n@charset \"\";\n\n\n", stylesheet->ToString());

  Parser p3("@charset \"UTF-8\" \"or 9\";");
  stylesheet.reset(p3.ParseStylesheet());
  EXPECT_EQ(Parser::kCharsetError, p3.errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n@charset \"UTF-8\";\n\n\n", stylesheet->ToString());
}

TEST_F(ParserTest, AcceptCorrectValues) {
  // http://code.google.com/p/modpagespeed/issues/detail?id=128
  Parser p("list-style-type: none");
  scoped_ptr<Declarations> declarations(p.ParseDeclarations());
  EXPECT_EQ(1, declarations->size());
  EXPECT_EQ(Parser::kNoError, p.errors_seen_mask());
  EXPECT_EQ("list-style-type: none", declarations->ToString());
}

TEST_F(ParserTest, AcceptAllValues) {
  Parser p("display: -moz-inline-box");
  p.set_preservation_mode(true);
  scoped_ptr<Declarations> declarations(p.ParseDeclarations());
  EXPECT_EQ(Parser::kNoError, p.errors_seen_mask());
  ASSERT_EQ(1, declarations->size());
  ASSERT_EQ(1, declarations->at(0)->values()->size());
  const Value* value = declarations->at(0)->values()->at(0);
  EXPECT_EQ(Value::IDENT, value->GetLexicalUnitType());
  EXPECT_EQ(Identifier::OTHER, value->GetIdentifier().ident());
  EXPECT_EQ("-moz-inline-box",
            UnicodeTextToUTF8(value->GetIdentifier().ident_text()));
  EXPECT_EQ("display: -moz-inline-box", declarations->ToString());

  Parser p2("display: -moz-inline-box");
  p2.set_preservation_mode(false);
  declarations.reset(p2.ParseDeclarations());
  EXPECT_EQ(Parser::kDeclarationError, p2.errors_seen_mask());
  EXPECT_EQ(0, declarations->size());
  EXPECT_EQ("", declarations->ToString());
}

TEST_F(ParserTest, VerbatimDeclarations) {
  Parser p("color: red; z-i ndex: 42; width: 1px");
  p.set_preservation_mode(false);
  scoped_ptr<Declarations> declarations(p.ParseDeclarations());
  EXPECT_EQ(Parser::kDeclarationError, p.errors_seen_mask());
  ASSERT_EQ(2, declarations->size());
  EXPECT_EQ(Property::COLOR, declarations->at(0)->prop());
  EXPECT_EQ(Property::WIDTH, declarations->at(1)->prop());
  // Unparsed declartion is ignored.
  EXPECT_EQ("color: #ff0000; width: 1px", declarations->ToString());

  Parser p2("color: red; z-i ndex: 42; width: 1px");
  p2.set_preservation_mode(true);
  declarations.reset(p2.ParseDeclarations());
  EXPECT_EQ(Parser::kNoError, p2.errors_seen_mask());
  EXPECT_EQ(Parser::kDeclarationError, p2.unparseable_sections_seen_mask());
  ASSERT_EQ(3, declarations->size());
  EXPECT_EQ(Property::COLOR, declarations->at(0)->prop());
  EXPECT_EQ(Property::UNPARSEABLE, declarations->at(1)->prop());
  EXPECT_EQ("z-i ndex: 42", declarations->at(1)->bytes_in_original_buffer());
  EXPECT_EQ(Property::WIDTH, declarations->at(2)->prop());
  EXPECT_EQ("color: #ff0000; /* Unparsed declaration: */ z-i ndex: 42; "
            "width: 1px", declarations->ToString());
}

TEST_F(ParserTest, CssHacks) {
  Parser p("*border: 0px");
  scoped_ptr<Declarations> declarations(p.ParseDeclarations());
  EXPECT_EQ(Parser::kNoError, p.errors_seen_mask());
  ASSERT_EQ(1, declarations->size());
  EXPECT_EQ(Property::OTHER, declarations->at(0)->prop());
  EXPECT_EQ("*border", declarations->at(0)->prop_text());
  EXPECT_EQ("*border: 0px", declarations->ToString());

  Parser p2("width: 1px; _width: 3px;");
  declarations.reset(p2.ParseDeclarations());
  EXPECT_EQ(Parser::kNoError, p.errors_seen_mask());
  ASSERT_EQ(2, declarations->size());
  EXPECT_EQ(Property::WIDTH, declarations->at(0)->prop());
  EXPECT_EQ(Property::OTHER, declarations->at(1)->prop());
  EXPECT_EQ("_width", declarations->at(1)->prop_text());
  EXPECT_EQ("width: 1px; _width: 3px", declarations->ToString());
}

TEST_F(ParserTest, Function) {
  Parser p("box-shadow: -1px -2px 2px rgba(0, 13, 255, .15)");
  scoped_ptr<Declarations> declarations(p.ParseDeclarations());
  EXPECT_EQ(Parser::kNoError, p.errors_seen_mask());
  ASSERT_EQ(1, declarations->size());
  EXPECT_EQ(4, declarations->at(0)->values()->size());
  const Value* val = declarations->at(0)->values()->at(3);
  EXPECT_EQ(Value::FUNCTION, val->GetLexicalUnitType());
  EXPECT_EQ(UTF8ToUnicodeText("rgba"), val->GetFunctionName());
  const Values& params = *val->GetParameters();
  ASSERT_EQ(4, params.size());
  EXPECT_EQ(Value::NUMBER, params[0]->GetLexicalUnitType());
  EXPECT_EQ(0, params[0]->GetIntegerValue());
  EXPECT_EQ(Value::NUMBER, params[1]->GetLexicalUnitType());
  EXPECT_EQ(13, params[1]->GetIntegerValue());
  EXPECT_EQ(Value::NUMBER, params[2]->GetLexicalUnitType());
  EXPECT_EQ(255, params[2]->GetIntegerValue());
  EXPECT_EQ(Value::NUMBER, params[3]->GetLexicalUnitType());
  EXPECT_DOUBLE_EQ(0.15, params[3]->GetFloatValue());

  EXPECT_EQ("box-shadow: -1px -2px 2px rgba(0, 13, 255, 0.15)",
            declarations->ToString());
}

// Functions inside functions and mixed use of commas and spaces, oh my.
TEST_F(ParserTest, ComplexFunction) {
  Parser p(
      "-webkit-gradient(linear, left top, left bottom, from(#ccc), to(#ddd))");
  scoped_ptr<Value> val(ParseAny(&p));
  EXPECT_EQ("-webkit-gradient(linear, left top, left bottom, "
            "from(#cccccc), to(#dddddd))", val->ToString());
}

TEST_F(ParserTest, Counter) {
  Parser p("content: \"Section \" counter(section)");
  scoped_ptr<Declarations> declarations(p.ParseDeclarations());
  EXPECT_EQ(Parser::kNoError, p.errors_seen_mask());
  ASSERT_EQ(1, declarations->size());
  ASSERT_EQ(2, declarations->at(0)->values()->size());
  const Value* val = declarations->at(0)->values()->at(1);
  EXPECT_EQ(Value::FUNCTION, val->GetLexicalUnitType());
  EXPECT_EQ(UTF8ToUnicodeText("counter"), val->GetFunctionName());
  const Values& params = *val->GetParameters();
  ASSERT_EQ(1, params.size());
  EXPECT_EQ(Value::IDENT, params[0]->GetLexicalUnitType());
  EXPECT_EQ(UTF8ToUnicodeText("section"), params[0]->GetIdentifierText());

  EXPECT_EQ("content: \"Section \" counter(section)",
            declarations->ToString());
}

TEST_F(ParserTest, ParseSingleImport) {
  scoped_ptr<Parser> parser(new Parser(
      "@IMPORT url(assets/style.css) screen,printer"));
  scoped_ptr<Import> import(parser->ParseAsSingleImport());
  EXPECT_TRUE(import.get() != NULL);
  if (import.get() != NULL) {
    EXPECT_EQ("assets/style.css", UnicodeTextToUTF8(import->link));
  }

  parser.reset(new Parser("\n\t@import \"mystyle.css\" all; \n"));
  import.reset(parser->ParseAsSingleImport());
  EXPECT_TRUE(import.get() != NULL);
  if (import.get() != NULL) {
    EXPECT_EQ("mystyle.css", UnicodeTextToUTF8(import->link));
  }

  parser.reset(new Parser("\n\t@import url(\"mystyle.css\"); \n"));
  import.reset(parser->ParseAsSingleImport());
  EXPECT_TRUE(import.get() != NULL);
  if (import.get() != NULL) {
    EXPECT_EQ("mystyle.css", UnicodeTextToUTF8(import->link));
  }

  parser.reset(new Parser("*border: 0px"));
  import.reset(parser->ParseAsSingleImport());
  EXPECT_TRUE(import.get() == NULL);

  parser.reset(new Parser("@import \"mystyle.css\" all;\n"
                          "@import url(\"mystyle.css\" );\n"));
  import.reset(parser->ParseAsSingleImport());
  EXPECT_TRUE(import.get() == NULL);

  parser.reset(new Parser("@charset \"ISO-8859-1\";\n"
                          "@import \"mystyle.css\" all;"));
  import.reset(parser->ParseAsSingleImport());
  EXPECT_TRUE(import.get() == NULL);
}

TEST_F(ParserTest, ExtractCharset) {
  scoped_ptr<Parser> parser;
  UnicodeText charset;

  parser.reset(new Parser("@charset \"ISO-8859-1\" ;"));
  charset = parser->ExtractCharset();
  EXPECT_EQ(Parser::kNoError, parser->errors_seen_mask());
  EXPECT_EQ("ISO-8859-1", UnicodeTextToUTF8(charset));

  parser.reset(new Parser("@charset foobar;"));
  charset = parser->ExtractCharset();
  EXPECT_EQ(Parser::kCharsetError, parser->errors_seen_mask());
  EXPECT_EQ("", UnicodeTextToUTF8(charset));

  parser.reset(new Parser("@charset \"UTF-8\" \"or 9\";"));
  charset = parser->ExtractCharset();
  EXPECT_EQ(Parser::kCharsetError, parser->errors_seen_mask());
  EXPECT_EQ("UTF-8", UnicodeTextToUTF8(charset));

  parser.reset(new Parser("@charsets \"UTF-8\" and \"ISO-8859-1\";"));
  charset = parser->ExtractCharset();
  EXPECT_EQ(Parser::kNoError, parser->errors_seen_mask());
  EXPECT_EQ("", UnicodeTextToUTF8(charset));

  parser.reset(new Parser("@IMPORT url(assets/style.css) screen,printer"));
  charset = parser->ExtractCharset();
  EXPECT_EQ(Parser::kNoError, parser->errors_seen_mask());
  EXPECT_EQ("", UnicodeTextToUTF8(charset));

  parser.reset(new Parser("wotcha!"));
  charset = parser->ExtractCharset();
  EXPECT_EQ(Parser::kNoError, parser->errors_seen_mask());
  EXPECT_EQ("", UnicodeTextToUTF8(charset));
}

TEST_F(ParserTest, UnexpectedAtRule) {
  scoped_ptr<Parser> parser;
  scoped_ptr<Stylesheet> stylesheet;

  // Unexpected at-rule with block.
  parser.reset(new Parser(
      "@font-face{font-family:'Ubuntu';font-style:normal}"
      ".foo { width: 1px; }"));
  stylesheet.reset(parser->ParseStylesheet());
  EXPECT_TRUE(Parser::kAtRuleError & parser->errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n\n\n.foo {width: 1px}\n", stylesheet->ToString());

  // ... and with extra selectors.
  parser.reset(new Parser(
      "@page :first { margin-top: 8cm; }\n"
      ".foo { width: 1px; }"));
  stylesheet.reset(parser->ParseStylesheet());
  EXPECT_TRUE(Parser::kAtRuleError & parser->errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n\n\n.foo {width: 1px}\n", stylesheet->ToString());

  // ... and with subblocks inside block.
  parser.reset(new Parser(
      "@keyframes wiggle {\n"
      "  0% {transform:rotate(6deg);}\n"
      "  50% {transform:rotate(6deg);}\n"
      "  100% {transform:rotate(6deg);}\n"
      "}\n"
      "@-webkit-keyframes wiggle {\n"
      "  0% {transform:rotate(6deg);}\n"
      "  50% {transform:rotate(6deg);}\n"
      "  100% {transform:rotate(6deg);}\n"
      "}\n"
      ".foo { width: 1px; }"));
  stylesheet.reset(parser->ParseStylesheet());
  EXPECT_TRUE(Parser::kAtRuleError & parser->errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n\n\n.foo {width: 1px}\n", stylesheet->ToString());

  // Unexpected at-rule ending in ';'.
  parser.reset(new Parser(
      "@namespace foo \"http://example.com/ns/foo\";\n"
      ".foo { width: 1px; }"));
  stylesheet.reset(parser->ParseStylesheet());
  EXPECT_TRUE(Parser::kAtRuleError & parser->errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n\n\n.foo {width: 1px}\n", stylesheet->ToString());

  // Unexpected at-rule with nothing else to parse before ';'.
  parser.reset(new Parser(
      "@use-klingon;\n"
      ".foo { width: 1px; }"));
  stylesheet.reset(parser->ParseStylesheet());
  EXPECT_TRUE(Parser::kAtRuleError & parser->errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n\n\n.foo {width: 1px}\n", stylesheet->ToString());

  // Unexpected at-keyword in a block.
  parser.reset(new Parser(
      "@media screen {\n"
      "  .bar { height: 2px; on-hover: @use-klingon}\n"
      "  .baz { height: 4px }\n"
      "}\n"
      ".foo {\n"
      "  three-dee: @three-dee { @background-lighting { azimuth: 30deg; } };\n"
      "  width: 1px;\n"
      "}\n"));
  stylesheet.reset(parser->ParseStylesheet());
  // Note: These don't actually call the at-rule parsing code because they
  // are not full at-rules, but just at-keywords and are skipped by
  // SkipToNextAny().
  EXPECT_NE(Parser::kNoError, parser->errors_seen_mask());
  EXPECT_EQ("/* AUTHOR */\n\n\n"
            "@media screen { .bar {height: 2px} }\n"
            "@media screen { .baz {height: 4px} }\n"
            ".foo {width: 1px}\n", stylesheet->ToString());
}

}  // namespace
