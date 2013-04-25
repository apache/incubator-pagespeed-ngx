// Copyright 2009 Google Inc. All Rights Reserved.
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

#include "pagespeed/kernel/js/js_minify.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/js/js_keywords.h"

namespace pagespeed {

// This sample code comes from Douglas Crockford's jsmin example.
const char* kBeforeCompilation =
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

const char* kAfterCompilation =
    "var is={ie:navigator.appName=='Microsoft Internet Explorer',"
    "java:navigator.javaEnabled(),ns:navigator.appName=='Netscape',"
    "ua:navigator.userAgent.toLowerCase(),version:parseFloat("
    "navigator.appVersion.substr(21))||parseFloat(navigator.appVersion)"
    ",win:navigator.platform=='Win32'}\n"
    "is.mac=is.ua.indexOf('mac')>=0;if(is.ua.indexOf('opera')>=0){"
    "is.ie=is.ns=false;is.opera=true;}\n"
    "if(is.ua.indexOf('gecko')>=0){is.ie=is.ns=false;is.gecko=true;}";

class JsMinifyTest : public testing::Test {
 protected:
  void CheckMinification(const StringPiece& before,
                         const StringPiece& after) {
    GoogleString output;
    EXPECT_TRUE(pagespeed::js::MinifyJs(before, &output));
    EXPECT_EQ(after, output);

    int output_size = -1;
    EXPECT_TRUE(pagespeed::js::GetMinifiedJsSize(before, &output_size));
    EXPECT_EQ(static_cast<int>(after.size()), output_size);
  }

  void CheckError(const StringPiece& input) {
    GoogleString output;
    EXPECT_FALSE(pagespeed::js::MinifyJs(input, &output));
    EXPECT_TRUE(output.empty());

    int output_size = -1;
    EXPECT_FALSE(pagespeed::js::GetMinifiedJsSize(input, &output_size));
    EXPECT_EQ(-1, output_size);
  }
};

TEST_F(JsMinifyTest, Basic) {
  CheckMinification(kBeforeCompilation, kAfterCompilation);
}

TEST_F(JsMinifyTest, AlreadyMinified) {
  CheckMinification(kAfterCompilation, kAfterCompilation);
}

TEST_F(JsMinifyTest, ErrorUnclosedComment) {
  CheckError("/* not valid javascript");
}

TEST_F(JsMinifyTest, ErrorUnclosedString) {
  CheckError("\"not valid javascript");
}

TEST_F(JsMinifyTest, ErrorUnclosedRegex) {
  CheckError("/not_valid_javascript");
}

TEST_F(JsMinifyTest, ErrorRegexNewline) {
  CheckError("/not_valid\njavascript/;");
}

TEST_F(JsMinifyTest, SignedCharDoesntSignExtend) {
  const unsigned char input[] = { 0xff, 0x00 };
  const char* input_nosign = reinterpret_cast<const char*>(input);
  CheckMinification(input_nosign, input_nosign);
}

TEST_F(JsMinifyTest, DealWithCrlf) {
  CheckMinification("var x = 1;\r\nvar y = 2;", "var x=1;var y=2;");
}

TEST_F(JsMinifyTest, DealWithTabs) {
  CheckMinification("var x = 1;\n\tvar y = 2;", "var x=1;var y=2;");
}

TEST_F(JsMinifyTest, EscapedCrlfInStringLiteral) {
  CheckMinification("var x = 'foo\\\r\nbar';", "var x='foo\\\r\nbar';");
}

TEST_F(JsMinifyTest, EmptyInput) {
  CheckMinification("", "");
}

TEST_F(JsMinifyTest, TreatCarriageReturnAsLinebreak) {
  CheckMinification("x = 1\ry = 2", "x=1\ny=2");
}

// See http://code.google.com/p/page-speed/issues/detail?id=607
TEST_F(JsMinifyTest, CarriageReturnEndsLineComment) {
  CheckMinification("x = 1 // foobar\ry = 2", "x=1\ny=2");
}

// See http://code.google.com/p/page-speed/issues/detail?id=198
TEST_F(JsMinifyTest, LeaveIEConditionalCompilationComments) {
  CheckMinification(
      "/*@cc_on\n"
      "  /*@if (@_win32)\n"
      "    document.write('IE');\n"
      "  @else @*/\n"
      "    document.write('other');\n"
      "  /*@end\n"
      "@*/",
      "/*@cc_on\n"
      "  /*@if (@_win32)\n"
      "    document.write('IE');\n"
      "  @else @*/\n"
      "document.write('other');/*@end\n"
      "@*/");
}

TEST_F(JsMinifyTest, DoNotJoinPlusses) {
  CheckMinification("var x = 'date=' + +new Date();",
                    "var x='date='+ +new Date();");
}

TEST_F(JsMinifyTest, DoNotJoinPlusAndPlusPlus) {
  CheckMinification("var x = y + ++z;", "var x=y+ ++z;");
}

TEST_F(JsMinifyTest, DoNotJoinPlusPlusAndPlus) {
  CheckMinification("var x = y++ + z;", "var x=y++ +z;");
}

TEST_F(JsMinifyTest, DoNotJoinMinuses) {
  CheckMinification("var x = 'date=' - -new Date();",
                    "var x='date='- -new Date();");
}

TEST_F(JsMinifyTest, DoNotJoinMinusAndMinusMinus) {
  CheckMinification("var x = y - --z;", "var x=y- --z;");
}

TEST_F(JsMinifyTest, DoNotJoinMinusMinusAndMinus) {
  CheckMinification("var x = y-- - z;", "var x=y-- -z;");
}

TEST_F(JsMinifyTest, DoJoinBangs) {
  CheckMinification("var x = ! ! y;", "var x=!!y;");
}

// See http://code.google.com/p/page-speed/issues/detail?id=242
TEST_F(JsMinifyTest, RemoveSurroundingSgmlComment) {
  CheckMinification("<!--\nvar x = 42;\n//-->", "var x=42;");
}

TEST_F(JsMinifyTest, RemoveSurroundingSgmlCommentWithoutSlashSlash) {
  CheckMinification("<!--\nvar x = 42;\n-->\n", "var x=42;");
}

// See http://code.google.com/p/page-speed/issues/detail?id=242
TEST_F(JsMinifyTest, SgmlLineComment) {
  CheckMinification("var x = 42; <!-- comment\nvar y = 17;",
                    "var x=42;var y=17;");
}

TEST_F(JsMinifyTest, RemoveSgmlCommentCloseOnOwnLine1) {
  CheckMinification("var x = 42;\n    --> \n", "var x=42;");
}

TEST_F(JsMinifyTest, RemoveSgmlCommentCloseOnOwnLine2) {
  CheckMinification("-->\nvar x = 42;\n", "var x=42;");
}

TEST_F(JsMinifyTest, DoNotRemoveSgmlCommentCloseInMidLine) {
  CheckMinification("var x = 42; --> \n", "var x=42;-->");
}

TEST_F(JsMinifyTest, DoNotCreateLineComment) {
  // Yes, this is legal code.  It sets x to NaN.
  CheckMinification("var x = 42 / /foo/;\n", "var x=42/ /foo/;");
}

TEST_F(JsMinifyTest, DoNotCreateSgmlLineComment1) {
  // Yes, this is legal code.  It tests if x is less than not(decrement y).
  CheckMinification("if (x <! --y) { x = 0; }\n", "if(x<! --y){x=0;}");
}

TEST_F(JsMinifyTest, DoNotCreateSgmlLineComment2) {
  // Yes, this is legal code.  It tests if x is less than not(decrement y).
  CheckMinification("if (x < !--y) { x = 0; }\n", "if(x< !--y){x=0;}");
}

TEST_F(JsMinifyTest, TrickyRegexLiteral) {
  // The first assignment is two divisions; the second assignment is a regex
  // literal.  JSMin gets this wrong (it removes whitespace from the regex).
  CheckMinification("var x = a[0] / b /i;\n var y = a[0] + / b /i;",
                    "var x=a[0]/b/i;var y=a[0]+/ b /i;");
}

// See http://code.google.com/p/modpagespeed/issues/detail?id=327
TEST_F(JsMinifyTest, RegexLiteralWithBrackets1) {
  // The / in [^/] doesn't end the regex, so the // is not a comment.
  CheckMinification("var x = /http:\\/\\/[^/]+\\//, y = 3;",
                    "var x=/http:\\/\\/[^/]+\\//,y=3;");
}

TEST_F(JsMinifyTest, RegexLiteralWithBrackets2) {
  // The first ] is escaped and doesn't close the [, so the following / doesn't
  // close the regex, so the following space is still in the regex and must be
  // preserved.
  CheckMinification("var x = /z[\\]/ ]/, y = 3;",
                    "var x=/z[\\]/ ]/,y=3;");
}

TEST_F(JsMinifyTest, ReturnRegex1) {
  // Make sure we understand that this is not division; "return" is not an
  // identifier!
  CheckMinification("return / x /g;", "return/ x /g;");
}

TEST_F(JsMinifyTest, ReturnRegex2) {
  // This test comes from the real world.  If "return" is incorrectly treated
  // as an identifier, the second slash will be treated as opening a regex
  // rather than closing it, and we'll error due to an unclosed regex.
  CheckMinification("return/#.+/.test(\n'#24' );",
                    "return/#.+/.test('#24');");
}

TEST_F(JsMinifyTest, ThrowRegex) {
  // Make sure we understand that this is not division; "throw" is not an
  // identifier!  (And yes, in JS you're allowed to throw a regex.)
  CheckMinification("throw / x /g;", "throw/ x /g;");
}

TEST_F(JsMinifyTest, ReturnThrowNumber) {
  CheckMinification("return 1;\nthrow 2;", "return 1;throw 2;");
}

TEST_F(JsMinifyTest, KeywordPrecedesRegex) {
  // Make sure "typeof /./" sees the first "/" as a regex and not division.
  // If it thinks it's a division then it will treat the "/    /" as a regex
  // and not remove the comment.  Do the same for all such keywords.
  // Example, "typeof /./    /* hi there */;" ->  "typeof/./;"
  for (JsKeywords::Iterator iter; !iter.AtEnd(); iter.Next()) {
    if (JsKeywords::CanKeywordPrecedeRegEx(iter.name())) {
      GoogleString input =
          net_instaweb::StrCat(iter.name(), " /./   /* hi there */;");
      GoogleString expected = net_instaweb::StrCat(iter.name(), "/./;");
      CheckMinification(input, expected);
    }
  }
}

const char kCrashTestString[] =
    "var x = 'asd \\' lse'\n"
    "var y /*comment*/ = /regex/\n"
    "var z = \"x =\" + x\n";

TEST_F(JsMinifyTest, DoNotCrash) {
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

TEST_F(JsMinifyTest, SemicolonInsertionIncrement) {
  CheckMinification("a\n++b\nc++\nd", "a\n++b\nc++\nd");
}

TEST_F(JsMinifyTest, SemicolonInsertionDecrement) {
  CheckMinification("a\n--b\nc--\nd", "a\n--b\nc--\nd");
}

TEST_F(JsMinifyTest, SemicolonInsertionAddition) {
  // No semicolons will be inserted, so the linebreaks can be removed.
  CheckMinification("i\n+\nj", "i+j");
}

TEST_F(JsMinifyTest, SemicolonInsertionSubtraction) {
  // No semicolons will be inserted, so the linebreaks can be removed.
  CheckMinification("i\n-\nj", "i-j");
}

TEST_F(JsMinifyTest, SemicolonInsertionLogicalOr) {
  // No semicolons will be inserted, so the linebreaks can be removed.
  CheckMinification("i\n||\nj", "i||j");
}

TEST_F(JsMinifyTest, SemicolonInsertionFuncCall) {
  // No semicolons will be inserted, so the linebreak can be removed.  This is
  // actually a function call, not two statements.
  CheckMinification("a = b + c\n(d + e).print()", "a=b+c(d+e).print()");
}

TEST_F(JsMinifyTest, SemicolonInsertionRegex) {
  // No semicolon will be inserted, so the linebreak and spaces can be removed
  // (this is two divisions, not a regex).
  CheckMinification("i=0\n/ [a-z] /g.exec(s)", "i=0/[a-z]/g.exec(s)");
}

TEST_F(JsMinifyTest, SemicolonInsertionWhileStmt) {
  // No semicolon will be inserted, so the linebreak can be removed.
  CheckMinification("while\n(true);", "while(true);");
}

TEST_F(JsMinifyTest, SemicolonInsertionReturnStmt1) {
  // A semicolon _will_ be inserted, so the linebreak _cannot_ be removed.
  CheckMinification("return\n(true);", "return\n(true);");
}

TEST_F(JsMinifyTest, SemicolonInsertionReturnStmt2) {
  // A semicolon _will_ be inserted, so the linebreak _cannot_ be removed.
  CheckMinification("return\n/*comment*/(true);", "return\n(true);");
}

TEST_F(JsMinifyTest, SemicolonInsertionThrowStmt) {
  // This is _not_ legal code; don't accidentally make it legal by removing the
  // linebreak.  (Eliminating a syntax error would change the semantics!)
  CheckMinification("throw\n  'error';", "throw\n'error';");
}

TEST_F(JsMinifyTest, SemicolonInsertionBreakStmt) {
  // A semicolon _will_ be inserted, so the linebreak _cannot_ be removed.
  CheckMinification("break\nlabel;", "break\nlabel;");
}

TEST_F(JsMinifyTest, SemicolonInsertionContinueStmt) {
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


TEST_F(JsMinifyTest, CollapsingStringTest) {
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

}  // namespace pagespeed
