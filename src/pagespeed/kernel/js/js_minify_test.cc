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


#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/js/js_keywords.h"

namespace {

using net_instaweb::StrAppend;

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

const char* kAfterCompilationOld =
    "var is={ie:navigator.appName=='Microsoft Internet Explorer',"
    "java:navigator.javaEnabled(),ns:navigator.appName=='Netscape',"
    "ua:navigator.userAgent.toLowerCase(),version:parseFloat("
    "navigator.appVersion.substr(21))||parseFloat(navigator.appVersion)"
    ",win:navigator.platform=='Win32'}\n"
    "is.mac=is.ua.indexOf('mac')>=0;if(is.ua.indexOf('opera')>=0){"
    "is.ie=is.ns=false;is.opera=true;}\n"
    "if(is.ua.indexOf('gecko')>=0){is.ie=is.ns=false;is.gecko=true;}";

const char* kAfterCompilationNew =
    "var is={ie:navigator.appName=='Microsoft Internet Explorer',"
    "java:navigator.javaEnabled(),ns:navigator.appName=='Netscape',"
    "ua:navigator.userAgent.toLowerCase(),version:parseFloat("
    "navigator.appVersion.substr(21))||parseFloat(navigator.appVersion)"
    ",win:navigator.platform=='Win32'}\n"
    "is.mac=is.ua.indexOf('mac')>=0;if(is.ua.indexOf('opera')>=0){"
    "is.ie=is.ns=false;is.opera=true;}"
    "if(is.ua.indexOf('gecko')>=0){is.ie=is.ns=false;is.gecko=true;}";

const char kTestRootDir[] = "/pagespeed/kernel/js/testdata/third_party/";

class JsMinifyTest : public testing::Test {
 protected:
  void CheckOldMinification(StringPiece before, StringPiece after) {
    GoogleString output;
    EXPECT_TRUE(pagespeed::js::MinifyJs(before, &output));
    EXPECT_EQ(after, output);

    int output_size = -1;
    EXPECT_TRUE(pagespeed::js::GetMinifiedJsSize(before, &output_size));
    EXPECT_EQ(static_cast<int>(after.size()), output_size);
  }

  void CheckNewMinification(StringPiece before, StringPiece after) {
    GoogleString output;
    EXPECT_TRUE(pagespeed::js::MinifyUtf8Js(&patterns_, before, &output));
    EXPECT_EQ(after, output);
  }

  void CheckMinification(StringPiece before, StringPiece after) {
    CheckOldMinification(before, after);
    CheckNewMinification(before, after);
  }

  void CheckOldError(StringPiece input) {
    GoogleString output;
    EXPECT_FALSE(pagespeed::js::MinifyJs(input, &output));

    int output_size = -1;
    EXPECT_FALSE(pagespeed::js::GetMinifiedJsSize(input, &output_size));
    EXPECT_EQ(-1, output_size);
  }

  void CheckNewError(StringPiece input) {
    GoogleString output;
    EXPECT_FALSE(pagespeed::js::MinifyUtf8Js(&patterns_, input, &output));
  }

  void CheckError(StringPiece input) {
    CheckOldError(input);
    CheckNewError(input);
  }

  void CheckFileMinification(StringPiece before_filename,
                             StringPiece after_filename) {
    net_instaweb::StdioFileSystem file_system;
    net_instaweb::GoogleMessageHandler message_handler;
    GoogleString original;
    {
      const GoogleString filepath = net_instaweb::StrCat(
          net_instaweb::GTestSrcDir(), kTestRootDir, before_filename);
      ASSERT_TRUE(file_system.ReadFile(
          filepath.c_str(), &original, &message_handler));
    }
    GoogleString expected;
    {
      const GoogleString filepath = net_instaweb::StrCat(
          net_instaweb::GTestSrcDir(), kTestRootDir, after_filename);
      ASSERT_TRUE(file_system.ReadFile(
          filepath.c_str(), &expected, &message_handler));
    }
    GoogleString actual;
    EXPECT_TRUE(pagespeed::js::MinifyUtf8Js(&patterns_, original, &actual));
    EXPECT_STREQ(expected, actual);
  }

  pagespeed::js::JsTokenizerPatterns patterns_;
};

TEST_F(JsMinifyTest, Basic) {
  // Our new minifier is slightly better at removing linebreaks than our old
  // minifier, so they get slightly different results for this test.
  CheckOldMinification(kBeforeCompilation, kAfterCompilationOld);
  CheckNewMinification(kBeforeCompilation, kAfterCompilationNew);
}

TEST_F(JsMinifyTest, AlreadyMinified) {
  CheckMinification(kAfterCompilationNew, kAfterCompilationNew);
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
  const unsigned char input[] = { 0xe0, 0xb2, 0xa0, 0x00 };
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
  // Our new minifier is slightly better at removing linebreaks than our old
  // minifier, so they get slightly different results for this test.
  CheckOldMinification(
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
  CheckNewMinification(
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
      "  @else @*/document.write('other');/*@end\n"
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

TEST_F(JsMinifyTest, DoNotJoinDecimalIntegerAndDot) {
  // 34 .toString() is legal code, but 34.toString() isn't, because the . in
  // the second example gets parsed as part of the literal (decimal point).  So
  // we need to leave a space in there.  Our old minifier gets this wrong, but
  // the new minifier should handle it correctly.
  CheckNewMinification("0192  . toString()", "0192 .toString()");
}

TEST_F(JsMinifyTest, DoJoinHexOctalIntegerAndDot) {
  // On the other hand, hex and octal literals can't have decimal points, so we
  // don't need the space here.
  CheckMinification("0x3e2  . toString() + 0172  . toString()",
                    "0x3e2.toString()+0172.toString()");
}

TEST_F(JsMinifyTest, DoJoinDecimalFractionAndDot) {
  // Also, if the decimal literal can't take another decimal point, then we can
  // safely remove the space.
  CheckMinification("3.5 . toString() + 3e2 . toString()",
                    "3.5.toString()+3e2.toString()");
}

TEST_F(JsMinifyTest, TrickyRegexLiteral) {
  // The first assignment is two divisions; the second assignment is a regex
  // literal.  JSMin gets this wrong (it removes whitespace from the regex).
  CheckMinification("var x = a[0] / b /i;\n var y = a[0] + / b /i;",
                    "var x=a[0]/b/i;var y=a[0]+/ b /i;");
}

TEST_F(JsMinifyTest, ObjectLiteralRegexLiteral) {
  // On the first line, this looks like it should be an object literal divided
  // by x divided by i, but nope, that's a block with a labelled expression
  // statement, followed by a regex literal.  The second line, on the other
  // hand, _is_ an object literal, followed by division.  Our old minifier gets
  // the second one wrong, but the new minifier should handle it correctly.
  CheckMinification("{foo: 123} / x /i;", "{foo:123}/ x /i;");
  CheckNewMinification("x={foo: 1} / x /i;", "x={foo:1}/x/i;");
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
  for (pagespeed::JsKeywords::Iterator iter; !iter.AtEnd(); iter.Next()) {
    if (pagespeed::JsKeywords::CanKeywordPrecedeRegEx(iter.name())) {
      GoogleString input =
          net_instaweb::StrCat(iter.name(), " /./   /* hi there */;");
      GoogleString expected = net_instaweb::StrCat(iter.name(), "/./;");
      CheckMinification(input, expected);
    }
  }
}

TEST_F(JsMinifyTest, LoopRegex) {
  // Make sure we understand that a slash after "while (...)" or "for (...)" is
  // a regex, not division.  Our old minifier gets this wrong, but the new
  // minifier should handle it correctly.
  CheckNewMinification("while (0) /\\//.exec('');",
                       "while(0)/\\//.exec('');");
  CheckNewMinification("for (x in y) / z /.exec(x);",
                       "for(x in y)/ z /.exec(x);");
}

TEST_F(JsMinifyTest, LabelRegex) {
  // Make sure we understand that a slash after a label is a regex, not
  // division.
  CheckMinification("{ foo: / x /.exec(''); }", "{foo:/ x /.exec('');}");
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
  // A trickier case that only the new minifier gets right:
  CheckNewMinification("a\n++\nb\nc++\nd", "a\n++b\nc++\nd");
}

TEST_F(JsMinifyTest, SemicolonInsertionDecrement) {
  CheckMinification("a\n--b\nc--\nd", "a\n--b\nc--\nd");
  // A trickier case that only the new minifier gets right:
  CheckNewMinification("a\n--\nb\nc--\nd", "a\n--b\nc--\nd");
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

TEST_F(JsMinifyTest, SemicolonInsertionComment) {
  CheckMinification("a=b\n /*hello*/ c=d\n", "a=b\nc=d");
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

TEST_F(JsMinifyTest, SemicolonInsertionDebuggerStmt) {
  // A semicolon _will_ be inserted, so the linebreak _cannot_ be removed.
  CheckMinification("debugger\nfoo;", "debugger\nfoo;");
}

TEST_F(JsMinifyTest, Latin1Input) {
  // Try to minify input that is Latin-1 encoded.  This is not valid UTF-8, but
  // we should be able to proceed gracefully (in most cases) if the non-ascii
  // characters only ever appear in string literals and comments.
  CheckMinification("str='Qu\xE9 pasa';// 'qu\xE9' means 'what'\n"
                    "cents=/* 73\xA2 is $0.73 */73;",
                    "str='Qu\xE9 pasa';cents=73;");
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

TEST_F(JsMinifyTest, MinifyAngular) {
  CheckFileMinification("angular.original", "angular.minified");
}

TEST_F(JsMinifyTest, MinifyJQuery) {
  CheckFileMinification("jquery.original", "jquery.minified");
}

TEST_F(JsMinifyTest, MinifyPrototype) {
  CheckFileMinification("prototype.original", "prototype.minified");
}

// Simple method for serializing Mappings so that they can be compared against
// gold versions.
GoogleString MappingsToString(
    const net_instaweb::source_map::MappingVector& mappings) {
  GoogleString result("{");
  for (int i = 0, n = mappings.size(); i < n; ++i) {
    StrAppend(&result, "(",
              net_instaweb::IntegerToString(mappings[i].gen_line), ", ",
              net_instaweb::IntegerToString(mappings[i].gen_col),  ", ");
    StrAppend(&result,
              net_instaweb::IntegerToString(mappings[i].src_file), ", ",
              net_instaweb::IntegerToString(mappings[i].src_line), ", ",
              net_instaweb::IntegerToString(mappings[i].src_col),  "), ");
  }
  result += "}";
  return result;
}

TEST_F(JsMinifyTest, SourceMapsSimple) {
  const char js_before[] =
      "/* Simple hello world program. */\n"
      "alert( 'Hello, World!' );\n";
  const char expected_js_after[] =
      "alert('Hello, World!');";
  const char expected_map[] =
      "{"
      "(0, 0, 0, 1, 0), "    // alert(
      "(0, 6, 0, 1, 7), "    // 'Hello, World!'
      "(0, 21, 0, 1, 23), "  // );
      "}";

  GoogleString output;
  net_instaweb::source_map::MappingVector mappings;
  EXPECT_TRUE(pagespeed::js::MinifyUtf8JsWithSourceMap(
      &patterns_, js_before, &output, &mappings));

  EXPECT_EQ(expected_js_after, output);

  EXPECT_EQ(expected_map, MappingsToString(mappings));
}

TEST_F(JsMinifyTest, SourceMapsComplex) {
  GoogleString output;
  net_instaweb::source_map::MappingVector mappings;
  EXPECT_TRUE(pagespeed::js::MinifyUtf8JsWithSourceMap(
      &patterns_, kBeforeCompilation, &output, &mappings));

  EXPECT_EQ(kAfterCompilationNew, output);

  const char expected_map[] =
      "{"
      "(0, 0, 0, 14, 0), "     // var is
      "(0, 6, 0, 14, 7), "     // =
      "(0, 7, 0, 14, 9), "     // {

      "(0, 8, 0, 15, 4), "     // ie:
      "(0, 11, 0, 15, 13), "   // navigator.appName
      "(0, 28, 0, 15, 31), "   // ==
      "(0, 30, 0, 15, 34), "   // 'Microsoft Internet Explorer',

      "(0, 60, 0, 16, 4), "    // java:
      "(0, 65, 0, 16, 13), "   // navigator.javaEnabled(),

      "(0, 89, 0, 17, 4), "    // ns:
      "(0, 92, 0, 17, 13), "   // navigator.appName
      "(0, 109, 0, 17, 31), "  // ==
      "(0, 111, 0, 17, 34), "  // 'Netscape',

      "(0, 122, 0, 18, 4), "   // ua:
      "(0, 125, 0, 18, 13), "  // navigator.userAgent.toLowerCase(),

      "(0, 159, 0, 19, 4), "   // version:
      "(0, 167, 0, 19, 13), "  // parseFloat(navigator.appVersion.substr(21))
      "(0, 210, 0, 19, 57), "  // ||
      "(0, 212, 0, 20, 13), "  // parseFloat(navigator.appVersion),

      "(0, 245, 0, 21, 4), "   // win:
      "(0, 249, 0, 21, 13), "  // navigator.platform
      "(0, 267, 0, 21, 32), "  // ==
      "(0, 269, 0, 21, 35), "  // 'Win32'
      "(0, 276, 0, 22, 0), "   // }

      "(1, 0, 0, 23, 0), "     // is.mac
      "(1, 6, 0, 23, 7), "     // =
      "(1, 7, 0, 23, 9), "     // is.ua.indexOf('mac')
      "(1, 27, 0, 23, 30), "   // >=
      "(1, 29, 0, 23, 33), "   // 0;

      "(1, 31, 0, 24, 0), "    // if
      "(1, 33, 0, 24, 3), "    // (is.ua.indexOf('opera')
      "(1, 56, 0, 24, 27), "   // >=
      "(1, 58, 0, 24, 30), "   // 0)
      "(1, 60, 0, 24, 33), "   // {

      "(1, 61, 0, 25, 4), "    // is.ie
      "(1, 66, 0, 25, 10), "   // =
      "(1, 67, 0, 25, 12), "   // is.ns
      "(1, 72, 0, 25, 18), "   // =
      "(1, 73, 0, 25, 20), "   // false;

      "(1, 79, 0, 26, 4), "    // is.opera
      "(1, 87, 0, 26, 13), "   // =
      "(1, 88, 0, 26, 15), "   // true;
      "(1, 93, 0, 27, 0), "    // }

      "(1, 94, 0, 28, 0), "    // if
      "(1, 96, 0, 28, 3), "    // (is.ua.indexOf('gecko')
      "(1, 119, 0, 28, 27), "  // >=
      "(1, 121, 0, 28, 30), "  // 0)
      "(1, 123, 0, 28, 33), "  // {

      "(1, 124, 0, 29, 4), "   // is.ie
      "(1, 129, 0, 29, 10), "  // =
      "(1, 130, 0, 29, 12), "  // is.ns
      "(1, 135, 0, 29, 18), "  // =
      "(1, 136, 0, 29, 20), "  // false;

      "(1, 142, 0, 30, 4), "   // is.gecko
      "(1, 150, 0, 30, 13), "  // =
      "(1, 151, 0, 30, 15), "  // true;
      "(1, 156, 0, 31, 0), "   // }
      "}";

  EXPECT_EQ(expected_map, MappingsToString(mappings));
}

}  // namespace
