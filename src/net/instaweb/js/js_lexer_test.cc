/*
 * Copyright 2011 Google Inc.
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

// Unit-test the js lexer with a few tricky cases borrowed from
// mdsteele's third_party/libpagespeed/src/pagespeed/js/js_minify_test.cc

#include "net/instaweb/js/public/js_lexer.h"

#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// This sample code comes from Douglas Crockford's jsmin example.
const char kJsMinExample[] =
  "// is.js\n"
  "\n"
  "// (c) 2001 Douglas Crockford\n"
  "// 2001 June 3\n"
  "\n"
  "\n"
  "// is\n"
  "\n"
  "// The -is- object is used to identify the browser.  "
  "Every browser edition\n"
  "// identifies itself, but there is no standard way of doing it, "
  "and some of\n"
  "// the identification is deceptive. This is because the authors of web\n"
  "// browsers are liars. For example, Microsoft's IE browsers claim to be\n"
  "// Mozilla 4. Netscape 6 claims to be version 5.\n"
  "\n"
  "var is = {\n"
  "    ie:      navigator.appName == 'Microsoft Internet Explorer',\n"
  "    java:    navigator.javaEnabled(),\n"
  "    ns:      navigator.appName == 'Netscape',\n"
  "    ua:      navigator.userAgent.toLowerCase(),\n"
  "    version: parseFloat(navigator.appVersion.substr(21)) ||\n"
  "             parseFloat(navigator.appVersion),\n"
  "    win:     navigator.platform == 'Win32'\n"
  "}\n"
  "is.mac = is.ua.indexOf('mac') >= 0;\n"
  "if (is.ua.indexOf('opera') >= 0) {\n"
  "    is.ie = is.ns = false;\n"
  "    is.opera = true;\n"
  "}\n"
  "if (is.ua.indexOf('gecko') >= 0) {\n"
  "    is.ie = is.ns = false;\n"
  "    is.gecko = true;\n"
  "}\n";

const char* kJsMinExampleTokens[] =  {
  "Comment: // is.js",
  "Whitespace: \n\n",
  "Comment: // (c) 2001 Douglas Crockford",
  "Whitespace: \n",
  "Comment: // 2001 June 3",
  "Whitespace: \n\n\n",
  "Comment: // is",
  "Whitespace: \n\n",
  "Comment: // The -is- object is used to identify the browser.  Every browser"
  " edition",
  "Whitespace: \n",
  "Comment: // identifies itself, but there is no standard way of doing it, an"
  "d some of",
  "Whitespace: \n",
  "Comment: // the identification is deceptive. This is because the authors of"
  " web",
  "Whitespace: \n",
  "Comment: // browsers are liars. For example, Microsoft's IE browsers claim "
  "to be",
  "Whitespace: \n",
  "Comment: // Mozilla 4. Netscape 6 claims to be version 5.",
  "Whitespace: \n\n",
  "Keyword: var",
  "Whitespace:  ",
  "Identifier: is",
  "Whitespace:  ",
  "Operator: =",
  "Whitespace:  ",
  "Operator: {",
  "Whitespace: \n    ",
  "Identifier: ie",
  "Operator: :",
  "Whitespace:       ",
  "Identifier: navigator",
  "Operator: .",
  "Identifier: appName",
  "Whitespace:  ",
  "Operator: =",
  "Operator: =",
  "Whitespace:  ",
  "StringLiteral: 'Microsoft Internet Explorer'",
  "Operator: ,",
  "Whitespace: \n    ",
  "Identifier: java",
  "Operator: :",
  "Whitespace:     ",
  "Identifier: navigator",
  "Operator: .",
  "Identifier: javaEnabled",
  "Operator: (",
  "Operator: )",
  "Operator: ,",
  "Whitespace: \n    ",
  "Identifier: ns",
  "Operator: :",
  "Whitespace:       ",
  "Identifier: navigator",
  "Operator: .",
  "Identifier: appName",
  "Whitespace:  ",
  "Operator: =",
  "Operator: =",
  "Whitespace:  ",
  "StringLiteral: 'Netscape'",
  "Operator: ,",
  "Whitespace: \n    ",
  "Identifier: ua",
  "Operator: :",
  "Whitespace:       ",
  "Identifier: navigator",
  "Operator: .",
  "Identifier: userAgent",
  "Operator: .",
  "Identifier: toLowerCase",
  "Operator: (",
  "Operator: )",
  "Operator: ,",
  "Whitespace: \n    ",
  "Identifier: version",
  "Operator: :",
  "Whitespace:  ",
  "Identifier: parseFloat",
  "Operator: (",
  "Identifier: navigator",
  "Operator: .",
  "Identifier: appVersion",
  "Operator: .",
  "Identifier: substr",
  "Operator: (",
  "Number: 21",
  "Operator: )",
  "Operator: )",
  "Whitespace:  ",
  "Operator: |",
  "Operator: |",
  "Whitespace: \n             ",
  "Identifier: parseFloat",
  "Operator: (",
  "Identifier: navigator",
  "Operator: .",
  "Identifier: appVersion",
  "Operator: )",
  "Operator: ,",
  "Whitespace: \n    ",
  "Identifier: win",
  "Operator: :",
  "Whitespace:      ",
  "Identifier: navigator",
  "Operator: .",
  "Identifier: platform",
  "Whitespace:  ",
  "Operator: =",
  "Operator: =",
  "Whitespace:  ",
  "StringLiteral: 'Win32'",
  "Whitespace: \n",
  "Operator: }",
  "Whitespace: \n",
  "Identifier: is",
  "Operator: .",
  "Identifier: mac",
  "Whitespace:  ",
  "Operator: =",
  "Whitespace:  ",
  "Identifier: is",
  "Operator: .",
  "Identifier: ua",
  "Operator: .",
  "Identifier: indexOf",
  "Operator: (",
  "StringLiteral: 'mac'",
  "Operator: )",
  "Whitespace:  ",
  "Operator: >",
  "Operator: =",
  "Whitespace:  ",
  "Number: 0",
  "Operator: ;",
  "Whitespace: \n",
  "Keyword: if",
  "Whitespace:  ",
  "Operator: (",
  "Identifier: is",
  "Operator: .",
  "Identifier: ua",
  "Operator: .",
  "Identifier: indexOf",
  "Operator: (",
  "StringLiteral: 'opera'",
  "Operator: )",
  "Whitespace:  ",
  "Operator: >",
  "Operator: =",
  "Whitespace:  ",
  "Number: 0",
  "Operator: )",
  "Whitespace:  ",
  "Operator: {",
  "Whitespace: \n    ",
  "Identifier: is",
  "Operator: .",
  "Identifier: ie",
  "Whitespace:  ",
  "Operator: =",
  "Whitespace:  ",
  "Identifier: is",
  "Operator: .",
  "Identifier: ns",
  "Whitespace:  ",
  "Operator: =",
  "Whitespace:  ",
  "Keyword: false",
  "Operator: ;",
  "Whitespace: \n    ",
  "Identifier: is",
  "Operator: .",
  "Identifier: opera",
  "Whitespace:  ",
  "Operator: =",
  "Whitespace:  ",
  "Keyword: true",
  "Operator: ;",
  "Whitespace: \n",
  "Operator: }",
  "Whitespace: \n",
  "Keyword: if",
  "Whitespace:  ",
  "Operator: (",
  "Identifier: is",
  "Operator: .",
  "Identifier: ua",
  "Operator: .",
  "Identifier: indexOf",
  "Operator: (",
  "StringLiteral: 'gecko'",
  "Operator: )",
  "Whitespace:  ",
  "Operator: >",
  "Operator: =",
  "Whitespace:  ",
  "Number: 0",
  "Operator: )",
  "Whitespace:  ",
  "Operator: {",
  "Whitespace: \n    ",
  "Identifier: is",
  "Operator: .",
  "Identifier: ie",
  "Whitespace:  ",
  "Operator: =",
  "Whitespace:  ",
  "Identifier: is",
  "Operator: .",
  "Identifier: ns",
  "Whitespace:  ",
  "Operator: =",
  "Whitespace:  ",
  "Keyword: false",
  "Operator: ;",
  "Whitespace: \n    ",
  "Identifier: is",
  "Operator: .",
  "Identifier: gecko",
  "Whitespace:  ",
  "Operator: =",
  "Whitespace:  ",
  "Keyword: true",
  "Operator: ;",
  "Whitespace: \n",
  "Operator: }",
  "Whitespace: \n",
  NULL
};

// Class to be used to receive Lexer events and record them in a vector
// for comparison against an expected gold array.
class LexerSpew : public JsLexer::Callback {
 public:
  explicit LexerSpew(JsLexer* lexer) : lexer_(lexer) {}
  ~LexerSpew() {}
  virtual void Keyword(JsKeywords::Keyword keyword) {
    output_.push_back(StrCat("Keyword: ", lexer_->keyword_string(keyword)));
  }
  virtual void Comment(const StringPiece& comment) {
    output_.push_back(StrCat("Comment: ", comment));
  }
  virtual void Whitespace(const StringPiece& whitespace) {
    output_.push_back(StrCat("Whitespace: ", whitespace));
  }
  virtual void Regex(const StringPiece& regex) {
    output_.push_back(StrCat("Regex: ", regex));
  }
  virtual void StringLiteral(const StringPiece& string_literal) {
    output_.push_back(StrCat("StringLiteral: ", string_literal));
  }
  virtual void Number(const StringPiece& number) {
    output_.push_back(StrCat("Number: ", number));
  }
  virtual void Operator(const StringPiece& op) {
    output_.push_back(StrCat("Operator: ", op));
  }
  virtual void Identifier(const StringPiece& identifier) {
    output_.push_back(StrCat("Identifier: ", identifier));
  }

  int num_tokens() const { return output_.size(); }
  const GoogleString token(int i) { return output_[i]; }
  void Clear() {
    output_.clear();
    Callback::Clear();
  }

 private:
  StringVector output_;
  JsLexer* lexer_;
};

class JsLexerTest : public testing::Test {
 public:
  JsLexerTest() : spew_(&lexer_) {}
  bool TestTokens(const char** expected_tokens, const char* js_input) {
    spew_.Clear();
    bool ret = lexer_.Lex(js_input, &spew_);

    int num_expected_tokens = 0;
    if (expected_tokens == NULL) {
      expected_tokens = &js_input;
      num_expected_tokens = 1;
    } else {
      for (const char** p = expected_tokens; *p != NULL; ++p) {
        ++num_expected_tokens;
      }
    }
    EXPECT_EQ(num_expected_tokens, spew_.num_tokens());
    for (int i = 0, n = std::max(num_expected_tokens, spew_.num_tokens());
         i < n; ++i) {
      const GoogleString& actual = (i < spew_.num_tokens())
          ? spew_.token(i)
          : "(null)";
      const char* expected = (i < num_expected_tokens)
          ? expected_tokens[i]
          : "(null)";
      EXPECT_STREQ(expected, actual) << ": index=" << i;
    }
    return ret;
  }

  bool TestSingleToken(const char* type, const char* js_input) {
    const char* kTokens[2];
    GoogleString buf = StrCat(type, ": ", js_input);
    kTokens[0] = buf.c_str();
    kTokens[1] = NULL;
    return TestTokens(kTokens, js_input);
  }

  JsLexer lexer_;
  LexerSpew spew_;
};

TEST_F(JsLexerTest, Basic) {
  static const char* tokens[] = {
    "Identifier: alert",
    "Operator: (",
    "StringLiteral: 'hello, world!'",
    "Operator: )",
    "Operator: ;",
    NULL
  };
  EXPECT_TRUE(TestTokens(tokens, "alert('hello, world!');"));
}

TEST_F(JsLexerTest, JsMinExample) {
  EXPECT_TRUE(TestTokens(kJsMinExampleTokens, kJsMinExample));
}

TEST_F(JsLexerTest, UnclosedComment) {
  static const char* kComment[] = {
    "Comment: /* not valid javascript",
    NULL
  };
  EXPECT_FALSE(TestTokens(kComment, "/* not valid javascript"));
}

TEST_F(JsLexerTest, ErrorUnclosedString) {
  static const char* kString[] = {
    "StringLiteral: \"not valid javascript",
    NULL
  };
  EXPECT_FALSE(TestTokens(kString, "\"not valid javascript"));
}

TEST_F(JsLexerTest, ErrorUnclosedRegex) {
  static const char* kRegex[] = {
    "Regex: /not valid javascript",
    NULL
  };
  EXPECT_FALSE(TestTokens(kRegex, "/not valid javascript"));
}

TEST_F(JsLexerTest, ErrorRegexNewline) {
  static const char* kRegex[] = {
    "Regex: /not valid\n",
    NULL
  };
  EXPECT_FALSE(TestTokens(kRegex, "/not valid\njavascript"));
}

TEST_F(JsLexerTest, EightBitCharsInIdentifier) {
  static const char* kEightBitChars[] = {
    "Identifier: \200\201\277",
    NULL
  };
  EXPECT_TRUE(TestTokens(kEightBitChars, "\200\201\277"));
}

TEST_F(JsLexerTest, BackslashesInIdentifier) {
  static const char* kSlashIdent[] = {
    "Identifier: a\u03c0b",
    NULL
  };
  EXPECT_TRUE(TestTokens(kSlashIdent, "a\u03c0b"));
}

TEST_F(JsLexerTest, BackslashesInString) {
  static const char* kSlashString[] = {
    "StringLiteral: \"a\\\"b\"",
    NULL
  };
  EXPECT_TRUE(TestTokens(kSlashString, "\"a\\\"b\""));
}

TEST_F(JsLexerTest, EmptyInput) {
  static const char* kEmpty[] = {
    NULL
  };
  EXPECT_TRUE(TestTokens(kEmpty, ""));
}

TEST_F(JsLexerTest, CombinePluses) {
  static const char* kPluses[] = {
    "Identifier: a",
    "Operator: ++",
    "Operator: +",
    "Identifier: b",
    NULL
  };
  EXPECT_TRUE(TestTokens(kPluses, "a+++b"));
}

TEST_F(JsLexerTest, CombinePluses2) {
  static const char* kPluses[] = {
    "Identifier: a",
    "Operator: +",
    "Whitespace:  ",
    "Operator: ++",
    "Identifier: b",
    NULL
  };
  EXPECT_TRUE(TestTokens(kPluses, "a+ ++b"));
}

TEST_F(JsLexerTest, CombinePlusesSpace) {
  static const char* kPluses[] = {
    "Identifier: a",
    "Operator: +",
    "Whitespace:  ",
    "Operator: +",
    "Identifier: b",
    NULL
  };
  EXPECT_TRUE(TestTokens(kPluses, "a+ +b"));
}

TEST_F(JsLexerTest, CombineMinuses) {
  static const char* kMinuses[] = {
    "Identifier: a",
    "Operator: --",
    "Operator: -",
    "Identifier: b",
    NULL
  };
  EXPECT_TRUE(TestTokens(kMinuses, "a---b"));
}

TEST_F(JsLexerTest, CombineMixed1) {
  static const char* kMixed[] = {
    "Identifier: a",
    "Operator: --",
    "Operator: +",
    "Identifier: b",
    NULL
  };
  EXPECT_TRUE(TestTokens(kMixed, "a--+b"));
}

TEST_F(JsLexerTest, CombineMixed2) {
  static const char* kMixed[] = {
    "Identifier: a",
    "Operator: -",
    "Operator: ++",
    "Identifier: b",
    NULL
  };
  EXPECT_TRUE(TestTokens(kMixed, "a-++b"));
}

TEST_F(JsLexerTest, CombineBangs) {
  static const char* kBangs[] = {
    "Operator: !",
    "Operator: !",
    "Identifier: b",
    NULL
  };
  EXPECT_TRUE(TestTokens(kBangs, "!!b"));
}

TEST_F(JsLexerTest, Equals) {
  EXPECT_TRUE(TestSingleToken("Operator", "*="));
  EXPECT_TRUE(TestSingleToken("Operator", "+="));
  EXPECT_TRUE(TestSingleToken("Operator", "-="));
  EXPECT_TRUE(TestSingleToken("Operator", "="));

  // "/=" won't be lexed as an opeartor by itself; it'll be lexed
  // as a regexp.  Do force it to be parsed as an operator, we
  // must precede it with an expression.
  static const char* kDivideEqual[] = {
    "Identifier: a",
    "Operator: /=",
    "Identifier: b",
    NULL
  };
  EXPECT_TRUE(TestTokens(kDivideEqual, "a/=b"));
}

TEST_F(JsLexerTest, SgmlComments) {
  EXPECT_TRUE(TestSingleToken("Comment", "<!--"));
  EXPECT_TRUE(TestSingleToken("Comment", "<!-->"));
  EXPECT_TRUE(TestSingleToken("Comment", "<!--->"));
  EXPECT_TRUE(TestSingleToken("Comment", "<!---->"));
  EXPECT_TRUE(TestSingleToken("Comment", "<!--X-->"));

  static const char* kComment[] = {
    "Comment: <!--/*Hello*/ ",
    "Whitespace: \n",
    NULL
  };
  EXPECT_TRUE(TestTokens(kComment, "<!--/*Hello*/ \n"));
}

TEST_F(JsLexerTest, TrickyRegexLiteral) {
  // The first assignment is two divisions; the second assignment is a regex
  // literal.  JSMin gets this wrong (it removes whitespace from the regex).
  static const char* kRegex[] = {
    "Keyword: var",
    "Whitespace:  ",
    "Identifier: x",
    "Whitespace:  ",
    "Operator: =",
    "Whitespace:  ",
    "Identifier: a",
    "Operator: [",
    "Number: 0",
    "Operator: ]",
    "Whitespace:  ",
    "Regex: / b /",
    "Identifier: i",
    "Operator: ;",
    "Whitespace: \n ",
    "Keyword: var",
    "Whitespace:  ",
    "Identifier: y",
    "Whitespace:  ",
    "Operator: =",
    "Whitespace:  ",
    "Identifier: a",
    "Operator: [",
    "Number: 0",
    "Operator: ]",
    "Whitespace:  ",
    "Operator: +",
    "Whitespace:  ",
    "Regex: / b /",
    "Identifier: i",
    "Operator: ;",
    NULL
  };
  TestTokens(kRegex, "var x = a[0] / b /i;\n var y = a[0] + / b /i;");
}

// See http://code.google.com/p/modpagespeed/issues/detail?id=327
TEST_F(JsLexerTest, RegexLiteralWithBrackets1) {
  // The first assignment is two divisions; the second assignment is a regex
  // literal.  JSMin gets this wrong (it removes whitespace from the regex).
  static const char* kRegex[] = {
    "Keyword: var",
    "Whitespace:  ",
    "Identifier: x",
    "Whitespace:  ",
    "Operator: =",
    "Whitespace:  ",
    "Regex: /http:\\/\\/[^/]+\\//",
    "Operator: ,",
    "Whitespace:  ",
    "Identifier: y",
    "Whitespace:  ",
    "Operator: =",
    "Whitespace:  ",
    "Number: 3",
    "Operator: ;",
    NULL
  };

  // The / in [^/] doesn't end the regex, so the // is not a comment.
  TestTokens(kRegex, "var x = /http:\\/\\/[^/]+\\//, y = 3;");
}

TEST_F(JsLexerTest, RegexLiteralWithBrackets2) {
  // The first ] is escaped and doesn't close the [, so the following / doesn't
  // close the regex, so the following space is still in the regex and must be
  // preserved.
  static const char* kRegex[] = {
    "Keyword: var",
    "Whitespace:  ",
    "Identifier: x",
    "Whitespace:  ",
    "Operator: =",
    "Whitespace:  ",
    "Regex: /z[\\]/ ]/",
    "Operator: ,",
    "Whitespace:  ",
    "Identifier: y",
    "Whitespace:  ",
    "Operator: =",
    "Whitespace:  ",
    "Number: 3",
    "Operator: ;",
    NULL
  };
  TestTokens(kRegex, "var x = /z[\\]/ ]/, y = 3;");
}

TEST_F(JsLexerTest, ReturnRegex1) {
  static const char* kRegex[] = {
    "Keyword: return",
    "Whitespace:  ",
    "Regex: / x /",
    "Identifier: g",
    "Operator: ;",
    NULL
  };
  // Make sure we understand that this is not division; "return" is not an
  // identifier!
  TestTokens(kRegex, "return / x /g;");
}

TEST_F(JsLexerTest, ReturnRegex2) {
  static const char* kRegex[] = {
    "Keyword: return",
    "Regex: /#.+/",
    "Operator: .",
    "Identifier: test",
    "Operator: (",
    "Whitespace: \n",
    "StringLiteral: '#24'",
    "Whitespace:  ",
    "Operator: )",
    "Operator: ;",
    NULL
  };

  // This test comes from the real world.  If "return" is incorrectly treated
  // as an identifier, the second slash will be treated as opening a regex
  // rather than closing it, and we'll error due to an unclosed regex.
  EXPECT_TRUE(TestTokens(kRegex, "return/#.+/.test(\n'#24' );"));
}

TEST_F(JsLexerTest, NumbersAndDotsAndIdentifiersAndKeywords) {
  static const char* kTokens[] = {
    "Keyword: return",
    "Whitespace:  ",
    "Identifier: a",
    "Operator: .",
    "Identifier: b",
    "Operator: +",
    "Number: 5.3",
    NULL
  };
  EXPECT_TRUE(TestTokens(kTokens, "return a.b+5.3"));
}

TEST_F(JsLexerTest, HtmlScriptTerminatorInComment) {
  static const char* kTokens[] = {
    "Whitespace: \n",
    "Comment: <!--",
    "Whitespace: \n",
    "Identifier: Stuff",
    "Whitespace: \n",
    "Comment: // -->",
    "Whitespace: \n",
    NULL
  };
  // See test case http://code.google.com/p/page-speed/issues/detail?id=242
  EXPECT_TRUE(TestTokens(kTokens, "\n<!--\nStuff\n// -->\n"));
}

TEST_F(JsLexerTest, Numbers) {
  static const char* kTwoDots[] = {
    "Operator: .",
    "Operator: .",
    NULL
  };
  EXPECT_TRUE(TestTokens(kTwoDots, ".."));
  static const char* kThreeDots[] = {
    "Operator: .",
    "Operator: .",
    "Operator: .",
    NULL
  };
  EXPECT_TRUE(TestTokens(kThreeDots, "..."));
  static const char* kTwoNumbers[] = {
    "Number: 1.2",
    "Number: .3",
    NULL
  };
  EXPECT_TRUE(TestTokens(kTwoNumbers, "1.2.3"));
  static const char* kNumber[] = {
    "Number: 1.23",
    NULL
  };
  EXPECT_TRUE(TestTokens(kNumber, "1.23"));
}

TEST_F(JsLexerTest, NumberProperty) {
  static const char* kNumber[] = {
    "Number: 1.",
    "Operator: .",
    "Identifier: property",
    NULL
  };
  EXPECT_TRUE(TestTokens(kNumber, "1..property"));
}

#if 0
// TODO(jmarantz): uncomment the rest of these & make sure they work.  They
// were borrowed from the mdsteele's jsminify test and need to be interpreted
// the context of a general purpose lexer.

// TODO(jmarantz): Add new tests to cover all the cases in js.

TEST_F(JsLexerTest, ThrowRegex) {
  // Make sure we understand that this is not division; "throw" is not an
  // identifier!  (And yes, in JS you're allowed to throw a regex.)
  CheckMinification("throw / x /g;", "throw/ x /g;");
}

TEST_F(JsLexerTest, ReturnThrowNumber) {
  CheckMinification("return 1;\nthrow 2;", "return 1;throw 2;");
}

const char kCrashTestString[] =
                                        "var x = 'asd \\' lse'\n"
                                        "var y /*comment*/ = /regex/\n"
                                        "var z = \"x =\" + x\n";

TEST_F(JsLexerTest, DoNotCrash) {
  // Run on all possible prefixes of kCrashTestString.  We don't care about the
  // result; we just want to make sure it doesn't crash.
  for (int i = 0, size = sizeof(kCrashTestString); i <= size; ++i) {
    GoogleString input(kCrashTestString, i);
    GoogleString output;
    pagespeed::js::MinifyJs(input, &output);
  }
}

// The below tests check for some corner cases of semicolon insertion, to make
// sure that we are minifying as much as possible (and no more!).
// See http://inimino.org/~inimino/blog/javascript_semicolons for details.

TEST_F(JsLexerTest, SemicolonInsertionIncrement) {
  CheckMinification("a\n++b\nc++\nd", "a\n++b\nc++\nd");
}

TEST_F(JsLexerTest, SemicolonInsertionDecrement) {
  CheckMinification("a\n--b\nc--\nd", "a\n--b\nc--\nd");
}

TEST_F(JsLexerTest, SemicolonInsertionAddition) {
  // No semicolons will be inserted, so the linebreaks can be removed.
  CheckMinification("i\n+\nj", "i+j");
}

TEST_F(JsLexerTest, SemicolonInsertionSubtraction) {
  // No semicolons will be inserted, so the linebreaks can be removed.
  CheckMinification("i\n-\nj", "i-j");
}

TEST_F(JsLexerTest, SemicolonInsertionLogicalOr) {
  // No semicolons will be inserted, so the linebreaks can be removed.
  CheckMinification("i\n||\nj", "i||j");
}

TEST_F(JsLexerTest, SemicolonInsertionFuncCall) {
  // No semicolons will be inserted, so the linebreak can be removed.  This is
  // actually a function call, not two statements.
  CheckMinification("a = b + c\n(d + e).print()", "a=b+c(d+e).print()");
}

TEST_F(JsLexerTest, SemicolonInsertionRegex) {
  // No semicolon will be inserted, so the linebreak and spaces can be removed
  // (this is two divisions, not a regex).
  CheckMinification("i=0\n/ [a-z] /g.exec(s)", "i=0/[a-z]/g.exec(s)");
}

TEST_F(JsLexerTest, SemicolonInsertionWhileStmt) {
  // No semicolon will be inserted, so the linebreak can be removed.
  CheckMinification("while\n(true);", "while(true);");
}

TEST_F(JsLexerTest, SemicolonInsertionReturnStmt1) {
  // A semicolon _will_ be inserted, so the linebreak _cannot_ be removed.
  CheckMinification("return\n(true);", "return\n(true);");
}

TEST_F(JsLexerTest, SemicolonInsertionReturnStmt2) {
  // A semicolon _will_ be inserted, so the linebreak _cannot_ be removed.
  CheckMinification("return\n/*comment*/(true);", "return\n(true);");
}

TEST_F(JsLexerTest, SemicolonInsertionThrowStmt) {
  // This is _not_ legal code; don't accidentally make it legal by removing the
  // linebreak.  (Eliminating a syntax error would change the semantics!)
  CheckMinification("throw\n  'error';", "throw\n'error';");
}

TEST_F(JsLexerTest, SemicolonInsertionBreakStmt) {
  // A semicolon _will_ be inserted, so the linebreak _cannot_ be removed.
  CheckMinification("break\nlabel;", "break\nlabel;");
}

TEST_F(JsLexerTest, SemicolonInsertionContinueStmt) {
  // A semicolon _will_ be inserted, so the linebreak _cannot_ be removed.
  CheckMinification("continue\nlabel;", "continue\nlabel;");
}

const char kCollapsingStringTestString[] =
    "var x = 'asd \\' lse'\n"
    "var y /*comment*/ = /re'gex/\n"
    "var z = \"x =\" + x\n";

const char kCollapsedTestString[] =
    "var x=''\n"
    "var y=/re'gex/\n"
    "var z=\"\"+x";


TEST_F(JsLexerTest, CollapsingStringTest) {
  int size = 0;
  GoogleString output;
  ASSERT_TRUE(pagespeed::js::MinifyJsAndCollapseStrings(
      kCollapsingStringTestString, &output));
  ASSERT_EQ(strlen(kCollapsedTestString), output.size());
  ASSERT_EQ(kCollapsedTestString, output);

  ASSERT_TRUE(pagespeed::js::GetMinifiedStringCollapsedJsSize(
      kCollapsingStringTestString, &size));
  ASSERT_EQ(static_cast<int>(strlen(kCollapsedTestString)), size);
}
#endif

}  // namespace

}  // namespace net_instaweb
