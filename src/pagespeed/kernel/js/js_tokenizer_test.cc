// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "pagespeed/kernel/js/js_tokenizer.h"

#include <utility>

#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/js/js_keywords.h"

using pagespeed::JsKeywords;
using pagespeed::js::JsTokenizer;
using pagespeed::js::JsTokenizerPatterns;

namespace {

const char kTestRootDir[] = "/pagespeed/kernel/js/testdata/third_party/";

class JsTokenizerTest : public testing::Test {
 protected:
  void BeginTokenizing(StringPiece input) {
    tokenizer_.reset(new JsTokenizer(&patterns_, input));
  }

  void ExpectParseStack(StringPiece expected_parse_stack) {
    EXPECT_STREQ(expected_parse_stack, tokenizer_->ParseStackForTest());
  }

  void ExpectToken(JsKeywords::Type expected_type,
                   StringPiece expected_token) {
    StringPiece actual_token;
    const JsKeywords::Type actual_type = tokenizer_->NextToken(&actual_token);
    EXPECT_EQ(make_pair(expected_type, expected_token),
              make_pair(actual_type, actual_token));
    EXPECT_FALSE(tokenizer_->has_error());
  }

  void ExpectToken(JsKeywords::Type expected_type, StringPiece expected_token,
                   StringPiece expected_parse_stack) {
    ExpectToken(expected_type, expected_token);
    ExpectParseStack(expected_parse_stack);
  }

  void ExpectEndOfInput() {
    ExpectToken(JsKeywords::kEndOfInput, "");
    ExpectParseStack("");
  }

  void ExpectError(StringPiece expected_token) {
    StringPiece actual_token;
    const JsKeywords::Type actual_type = tokenizer_->NextToken(&actual_token);
    EXPECT_EQ(make_pair(JsKeywords::kError, expected_token),
              make_pair(actual_type, actual_token));
    EXPECT_TRUE(tokenizer_->has_error());
  }

  void ExpectTokenizeFileSuccessfully(StringPiece filename) {
    // Read in the JavaScript file.
    GoogleString original;
    {
      net_instaweb::StdioFileSystem file_system;
      const GoogleString filepath = net_instaweb::StrCat(
          net_instaweb::GTestSrcDir(), kTestRootDir, filename);
      net_instaweb::GoogleMessageHandler message_handler;
      ASSERT_TRUE(file_system.ReadFile(
          filepath.c_str(), &original, &message_handler));
    }
    // Tokenize the JavaScript, appending each token onto the output string.
    // There should be no tokenizer errors.
    GoogleString output;
    {
      output.reserve(original.size());
      JsTokenizer tokenizer(&patterns_, original);
      StringPiece token;
      while (tokenizer.NextToken(&token) !=
             JsKeywords::kEndOfInput) {
        ASSERT_FALSE(tokenizer.has_error())
            << "Error at: " << token.substr(0, 150)
            << "\nWith stack: " << tokenizer.ParseStackForTest();
        token.AppendToString(&output);
      }
    }
    // The concatenation of all tokens should exactly reproduce the input.
    EXPECT_STREQ(original, output);
  }

 private:
  JsTokenizerPatterns patterns_;
  scoped_ptr<JsTokenizer> tokenizer_;
};

TEST_F(JsTokenizerTest, EmptyInput) {
  BeginTokenizing("");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Blocks) {
  BeginTokenizing("if (foo){\n"
                  "}else if(bar){\n"
                  "}else baz;");
  ExpectParseStack("Start");
  ExpectToken(JsKeywords::kIf,         "if",   "Start BkKwd");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "(",    "Start BkKwd (");
  ExpectToken(JsKeywords::kIdentifier, "foo",  "Start BkKwd ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")",    "Start BkHdr");
  ExpectToken(JsKeywords::kOperator,   "{",    "Start {");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kOperator,   "}",    "Start");
  ExpectToken(JsKeywords::kElse,       "else", "Start BkHdr");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIf,         "if",   "Start BkHdr BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(",    "Start BkHdr BkKwd (");
  ExpectToken(JsKeywords::kIdentifier, "bar",  "Start BkHdr BkKwd ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")",    "Start BkHdr");
  ExpectToken(JsKeywords::kOperator,   "{",    "Start {");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kOperator,   "}",    "Start");
  ExpectToken(JsKeywords::kElse,       "else", "Start BkHdr");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "baz",  "Start BkHdr Expr");
  ExpectToken(JsKeywords::kOperator,   ";",    "Start");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Functions) {
  BeginTokenizing("function foo(){return 1}\n"
                  "bar=function(){return 2};\n"
                  "(function(){window=5})();");
  ExpectParseStack("Start");
  ExpectToken(JsKeywords::kFunction,   "function");
  ExpectParseStack("Start BkKwd");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "foo");
  ExpectParseStack("Start BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start BkHdr");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectParseStack("Start {");
  ExpectToken(JsKeywords::kReturn,     "return");
  ExpectParseStack("Start { RetTh");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kNumber,     "1");
  ExpectParseStack("Start { RetTh Expr");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectParseStack("Start");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "bar");
  ExpectParseStack("Start Expr");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kFunction,   "function");
  ExpectParseStack("Start Expr Oper BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start Expr Oper BkHdr");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectParseStack("Start Expr Oper {");
  ExpectToken(JsKeywords::kReturn,     "return");
  ExpectParseStack("Start Expr Oper { RetTh");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kNumber,     "2");
  ExpectParseStack("Start Expr Oper { RetTh Expr");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectParseStack("Start Expr");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectParseStack("Start");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectParseStack("Start (");
  ExpectToken(JsKeywords::kFunction,   "function");
  ExpectParseStack("Start ( BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectParseStack("Start ( BkKwd (");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start ( BkHdr");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectParseStack("Start ( {");
  ExpectToken(JsKeywords::kIdentifier, "window");
  ExpectParseStack("Start ( { Expr");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectParseStack("Start ( { Expr Oper");
  ExpectToken(JsKeywords::kNumber,     "5");
  ExpectParseStack("Start ( { Expr");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectParseStack("Start ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start Expr");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectParseStack("Start Expr (");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start Expr");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectParseStack("Start");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, ForLoop) {
  BeginTokenizing("loop:for(var x=0;x<5;++x){\n"
                  "  break loop;\n"
                  "}");
  ExpectToken(JsKeywords::kIdentifier, "loop");
  ExpectToken(JsKeywords::kOperator,   ":");
  ExpectToken(JsKeywords::kFor,        "for");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kVar,        "var");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kNumber,     "0");
  ExpectParseStack("Start BkKwd ( Other Expr");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectParseStack("Start BkKwd (");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "<");
  ExpectToken(JsKeywords::kNumber,     "5");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kOperator,   "++");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectParseStack("Start BkKwd ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start BkHdr");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectParseStack("Start {");
  ExpectToken(JsKeywords::kLineSeparator, "\n  ");

  ExpectToken(JsKeywords::kBreak,      "break");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "loop");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Keywords) {
  // A piece of code that contains every keyword at least once.
  BeginTokenizing("(function(){\n"
                  " var\n"
                  "   x=typeof null\n"
                  " const y=void false\n"
                  " delete x\n"
                  " if(this instanceof String){\n"
                  "  debugger\n"
                  "  for(z in this)\n"
                  "   continue\n"
                  "  do break;while(true)\n"
                  "  switch(y){\n"
                  "   case 0:\n"
                  "   default:\n"
                  "    try{\n"
                  "     with(this){\n"
                  "      throw new Object()\n"
                  "     }\n"
                  "    }catch(e){\n"
                  "     return\n"
                  "    }finally{}\n"
                  "  }\n"
                  " }else return\n"
                  "})");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kFunction,   "function");
  ExpectParseStack("Start ( BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectToken(JsKeywords::kLineSeparator, "\n ");
  ExpectParseStack("Start ( {");

  ExpectToken(JsKeywords::kVar,        "var");
  ExpectParseStack("Start ( { Other");
  ExpectToken(JsKeywords::kLineSeparator, "\n   ");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kTypeof,     "typeof");
  ExpectParseStack("Start ( { Other Expr Oper");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kNull,       "null");
  ExpectParseStack("Start ( { Other Expr");
  ExpectToken(JsKeywords::kSemiInsert, "\n ");
  ExpectParseStack("Start ( {");

  ExpectToken(JsKeywords::kConst,      "const");
  ExpectParseStack("Start ( { Other");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "y");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kVoid,       "void");
  ExpectParseStack("Start ( { Other Expr Oper");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kFalse,      "false");
  ExpectParseStack("Start ( { Other Expr");
  ExpectToken(JsKeywords::kSemiInsert, "\n ");
  ExpectParseStack("Start ( {");

  ExpectToken(JsKeywords::kDelete,     "delete");
  ExpectParseStack("Start ( { Oper");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kSemiInsert, "\n ");
  ExpectParseStack("Start ( {");

  ExpectToken(JsKeywords::kIf,         "if");
  ExpectParseStack("Start ( { BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kThis,       "this");
  ExpectParseStack("Start ( { BkKwd ( Expr");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kInstanceof, "instanceof");
  ExpectParseStack("Start ( { BkKwd ( Expr Oper");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "String");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectToken(JsKeywords::kLineSeparator, "\n  ");
  ExpectParseStack("Start ( { {");

  ExpectToken(JsKeywords::kDebugger,   "debugger");
  ExpectParseStack("Start ( { { Jump");
  ExpectToken(JsKeywords::kSemiInsert, "\n  ");
  ExpectParseStack("Start ( { {");

  ExpectToken(JsKeywords::kFor,        "for");
  ExpectParseStack("Start ( { { BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kIdentifier, "z");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIn,         "in");
  ExpectParseStack("Start ( { { BkKwd ( Expr Oper");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kThis,       "this");
  ExpectParseStack("Start ( { { BkKwd ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectToken(JsKeywords::kLineSeparator, "\n   ");
  ExpectParseStack("Start ( { { BkHdr");

  ExpectToken(JsKeywords::kContinue,   "continue");
  ExpectParseStack("Start ( { { BkHdr Jump");
  ExpectToken(JsKeywords::kSemiInsert, "\n  ");
  ExpectParseStack("Start ( { {");

  ExpectToken(JsKeywords::kDo,         "do");
  ExpectParseStack("Start ( { { BkHdr");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kBreak,      "break");
  ExpectParseStack("Start ( { { BkHdr Jump");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectParseStack("Start ( { {");
  ExpectToken(JsKeywords::kWhile,      "while");
  ExpectParseStack("Start ( { { BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kTrue,       "true");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start ( { { BkHdr");
  ExpectToken(JsKeywords::kLineSeparator, "\n  ");

  ExpectToken(JsKeywords::kSwitch,     "switch");
  ExpectParseStack("Start ( { { BkHdr BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kIdentifier, "y");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start ( { { BkHdr");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectParseStack("Start ( { { {");
  ExpectToken(JsKeywords::kLineSeparator, "\n   ");

  ExpectToken(JsKeywords::kCase,       "case");
  ExpectParseStack("Start ( { { { Oper");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kNumber,     "0");
  ExpectParseStack("Start ( { { { Expr");
  ExpectToken(JsKeywords::kOperator,   ":");
  ExpectParseStack("Start ( { { {");
  ExpectToken(JsKeywords::kLineSeparator, "\n   ");

  ExpectToken(JsKeywords::kDefault,    "default");
  ExpectParseStack("Start ( { { { Other");
  ExpectToken(JsKeywords::kOperator,   ":");
  ExpectParseStack("Start ( { { {");
  ExpectToken(JsKeywords::kLineSeparator, "\n    ");

  ExpectToken(JsKeywords::kTry,        "try");
  ExpectParseStack("Start ( { { { BkHdr");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectToken(JsKeywords::kLineSeparator, "\n     ");
  ExpectParseStack("Start ( { { { {");

  ExpectToken(JsKeywords::kWith,       "with");
  ExpectParseStack("Start ( { { { { BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kThis,       "this");
  ExpectParseStack("Start ( { { { { BkKwd ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectToken(JsKeywords::kLineSeparator, "\n      ");
  ExpectParseStack("Start ( { { { { {");

  ExpectToken(JsKeywords::kThrow,      "throw");
  ExpectParseStack("Start ( { { { { { RetTh");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kNew,        "new");
  ExpectParseStack("Start ( { { { { { RetTh Oper");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "Object");
  ExpectParseStack("Start ( { { { { { RetTh Expr");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectToken(JsKeywords::kLineSeparator, "\n     ");
  ExpectParseStack("Start ( { { { { { RetTh Expr");

  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectToken(JsKeywords::kLineSeparator, "\n    ");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectParseStack("Start ( { { {");
  ExpectToken(JsKeywords::kCatch,      "catch");
  ExpectParseStack("Start ( { { { BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kIdentifier, "e");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectParseStack("Start ( { { { {");
  ExpectToken(JsKeywords::kLineSeparator, "\n     ");

  ExpectToken(JsKeywords::kReturn,     "return");
  ExpectParseStack("Start ( { { { { RetTh");
  ExpectToken(JsKeywords::kLineSeparator, "\n    ");

  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectParseStack("Start ( { { {");
  ExpectToken(JsKeywords::kFinally,    "finally");
  ExpectParseStack("Start ( { { { BkHdr");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectParseStack("Start ( { { { {");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectToken(JsKeywords::kLineSeparator, "\n  ");
  ExpectParseStack("Start ( { { {");

  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectToken(JsKeywords::kLineSeparator, "\n ");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectParseStack("Start ( {");
  ExpectToken(JsKeywords::kElse,       "else");
  ExpectParseStack("Start ( { BkHdr");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kReturn,     "return");
  ExpectParseStack("Start ( { BkHdr RetTh");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectParseStack("Start ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start Expr");
  ExpectEndOfInput();
  ExpectParseStack("");
}

TEST_F(JsTokenizerTest, StrictModeReservedWords) {
  // These names are reserved words in strict mode, but otherwise they're legal
  // identifiers.
  // TODO(mdsteele): At some point we may want to implement strict mode error
  //   checking, at which point we should add a test with "use strict".
  BeginTokenizing("var implements,interface,let,package\n"
                  "   ,private,protected,public,static,yield;");
  ExpectToken(JsKeywords::kVar,        "var");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "implements");
  ExpectToken(JsKeywords::kOperator,   ",");
  ExpectToken(JsKeywords::kIdentifier, "interface");
  ExpectToken(JsKeywords::kOperator,   ",");
  ExpectToken(JsKeywords::kIdentifier, "let");
  ExpectToken(JsKeywords::kOperator,   ",");
  ExpectToken(JsKeywords::kIdentifier, "package");
  ExpectToken(JsKeywords::kLineSeparator, "\n   ");

  ExpectToken(JsKeywords::kOperator,   ",");
  ExpectToken(JsKeywords::kIdentifier, "private");
  ExpectToken(JsKeywords::kOperator,   ",");
  ExpectToken(JsKeywords::kIdentifier, "protected");
  ExpectToken(JsKeywords::kOperator,   ",");
  ExpectToken(JsKeywords::kIdentifier, "public");
  ExpectToken(JsKeywords::kOperator,   ",");
  ExpectToken(JsKeywords::kIdentifier, "static");
  ExpectToken(JsKeywords::kOperator,   ",");
  ExpectToken(JsKeywords::kIdentifier, "yield");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Comments) {
  BeginTokenizing("x=1; // hello\n"
                  "y=/* world */2;\n"
                  "z<!--sgml\n"    // <!-- is a line comment, but
                  "foo-->bar;\n"   // --> is a line comment only at
                  " --> sgml\n");  // the start of the line.
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kNumber,     "1");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kComment,    "// hello");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "y");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kComment,    "/* world */");
  ExpectToken(JsKeywords::kNumber,     "2");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "z");
  ExpectToken(JsKeywords::kComment,    "<!--sgml");
  ExpectToken(JsKeywords::kSemiInsert, "\n");

  ExpectToken(JsKeywords::kIdentifier, "foo");
  ExpectToken(JsKeywords::kOperator,   "--");
  ExpectToken(JsKeywords::kOperator,   ">");
  ExpectToken(JsKeywords::kIdentifier, "bar");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n ");

  ExpectToken(JsKeywords::kComment,    "--> sgml");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, LineComments) {
  BeginTokenizing("1//foo\r"
                  "+2//bar\xE2\x80\xA8"  // U+2028 LINE SEPARATOR"
                  "+3//baz\xE2\x80\xA9"  // U+2029 PARAGRAPH SEPARATOR
                  "4//quux\xE2\x81\x9F"  // U+205F MEDIUM MATHEMATICAL SPACE
                  "+5//hello, world!\n"
                  "6");
  ExpectToken(JsKeywords::kNumber,     "1");
  ExpectToken(JsKeywords::kComment,    "//foo");
  ExpectToken(JsKeywords::kLineSeparator, "\r");

  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kNumber,     "2");
  ExpectToken(JsKeywords::kComment,    "//bar");
  ExpectToken(JsKeywords::kLineSeparator, "\xE2\x80\xA8");

  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kNumber,     "3");
  ExpectToken(JsKeywords::kComment,    "//baz");
  ExpectToken(JsKeywords::kSemiInsert, "\xE2\x80\xA9");

  ExpectToken(JsKeywords::kNumber,     "4");
  ExpectToken(JsKeywords::kComment,    "//quux\xE2\x81\x9F+5//hello, world!");
  ExpectToken(JsKeywords::kSemiInsert, "\n");

  ExpectToken(JsKeywords::kNumber,     "6");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, SgmlComments) {
  BeginTokenizing("4+<!--foo\n"
                  "3+\n"
                  "/*foo*/ --> foo\n"
                  "x --> foo\n");
  ExpectToken(JsKeywords::kNumber,     "4");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kComment,    "<!--foo");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kNumber,     "3");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kComment,    "/*foo*/");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kComment,    "--> foo");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "--");
  ExpectToken(JsKeywords::kOperator,   ">");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "foo");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Whitespace) {
  BeginTokenizing("\xEF\xBB\xBF"     // U+FEFF BYTE ORDER MARK
                  "a\f=\t1\v+ 2\r"
                  "5\xC2\xA0"        // U+00A0 NO-BREAK SPACE
                  "\xE2\x81\x9F"     // U+205F MEDIUM MATHEMATICAL SPACE
                  "+\xE1\x9A\x80"    // U+1680 OGHAM SPACE MARK
                  "7\xE2\x80\xA9"    // U+2029 PARAGRAPH SEPARATOR
                  ";\xE2\x80\xA8");  // U+2028 LINE SEPARATOR
  ExpectToken(JsKeywords::kWhitespace, "\xEF\xBB\xBF");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kWhitespace, "\f");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kWhitespace, "\t");
  ExpectToken(JsKeywords::kNumber,     "1");
  ExpectToken(JsKeywords::kWhitespace, "\v");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kNumber,     "2");
  ExpectToken(JsKeywords::kSemiInsert, "\r");

  ExpectToken(JsKeywords::kNumber,     "5");
  ExpectToken(JsKeywords::kWhitespace, "\xC2\xA0\xE2\x81\x9F");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kWhitespace, "\xE1\x9A\x80");
  ExpectToken(JsKeywords::kNumber,     "7");
  ExpectToken(JsKeywords::kLineSeparator, "\xE2\x80\xA9");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\xE2\x80\xA8");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, NumericLiterals) {
  BeginTokenizing("var  foo =\n  .85 - 0191.e+3+0171.toString();\n"
                  "1..property");
  ExpectToken(JsKeywords::kVar,        "var");
  ExpectToken(JsKeywords::kWhitespace, "  ");
  ExpectToken(JsKeywords::kIdentifier, "foo");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kLineSeparator, "\n  ");
  ExpectToken(JsKeywords::kNumber,     ".85");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "-");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kNumber,     "0191.e+3");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kNumber,     "0171");
  ExpectToken(JsKeywords::kOperator,   ".");
  ExpectToken(JsKeywords::kIdentifier, "toString");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kNumber,     "1.");
  ExpectToken(JsKeywords::kOperator,   ".");
  ExpectToken(JsKeywords::kIdentifier, "property");
  ExpectEndOfInput();
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, RegexLiterals) {
  BeginTokenizing("foo=/quux/;\n"
                  "bar=/quux/ig;\n"
                  "baz=a/quux/ig;");
  ExpectToken(JsKeywords::kIdentifier, "foo");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kRegex,      "/quux/");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "bar");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kRegex,      "/quux/ig");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "baz");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "/");
  ExpectToken(JsKeywords::kIdentifier, "quux");
  ExpectToken(JsKeywords::kOperator,   "/");
  ExpectToken(JsKeywords::kIdentifier, "ig");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, RegexVsSlash) {
  BeginTokenizing("if(a*(b+c)/d<e)/d<e/.exec('\\'');\n"
                  "else/x/.exec(\"\");");
  ExpectParseStack("Start");
  ExpectToken(JsKeywords::kIf,         "if");
  ExpectParseStack("Start BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectParseStack("Start BkKwd (");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectParseStack("Start BkKwd ( Expr");
  ExpectToken(JsKeywords::kOperator,   "*");
  ExpectParseStack("Start BkKwd ( Expr Oper");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectParseStack("Start BkKwd ( Expr Oper (");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectParseStack("Start BkKwd ( Expr Oper ( Expr");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectParseStack("Start BkKwd ( Expr Oper ( Expr Oper");
  ExpectToken(JsKeywords::kIdentifier, "c");
  ExpectParseStack("Start BkKwd ( Expr Oper ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start BkKwd ( Expr");
  ExpectToken(JsKeywords::kOperator,   "/");
  ExpectParseStack("Start BkKwd ( Expr Oper");
  ExpectToken(JsKeywords::kIdentifier, "d");
  ExpectParseStack("Start BkKwd ( Expr");
  ExpectToken(JsKeywords::kOperator,   "<");
  ExpectParseStack("Start BkKwd ( Expr Oper");
  ExpectToken(JsKeywords::kIdentifier, "e");
  ExpectParseStack("Start BkKwd ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start BkHdr");
  ExpectToken(JsKeywords::kRegex,      "/d<e/");
  ExpectParseStack("Start BkHdr Expr");
  ExpectToken(JsKeywords::kOperator,   ".");
  ExpectParseStack("Start BkHdr Expr Oper");
  ExpectToken(JsKeywords::kIdentifier, "exec");
  ExpectParseStack("Start BkHdr Expr");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectParseStack("Start BkHdr Expr (");
  ExpectToken(JsKeywords::kStringLiteral, "'\\''");
  ExpectParseStack("Start BkHdr Expr ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start BkHdr Expr");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectParseStack("Start");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kElse,       "else");
  ExpectParseStack("Start BkHdr");
  ExpectToken(JsKeywords::kRegex,      "/x/");
  ExpectParseStack("Start BkHdr Expr");
  ExpectToken(JsKeywords::kOperator,   ".");
  ExpectParseStack("Start BkHdr Expr Oper");
  ExpectToken(JsKeywords::kIdentifier, "exec");
  ExpectParseStack("Start BkHdr Expr");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectParseStack("Start BkHdr Expr (");
  ExpectToken(JsKeywords::kStringLiteral, "\"\"");
  ExpectParseStack("Start BkHdr Expr ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectParseStack("Start BkHdr Expr");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectParseStack("Start");
  ExpectEndOfInput();
  ExpectParseStack("");
}

TEST_F(JsTokenizerTest, Operators1) {
  BeginTokenizing("foo /= bar+++baz;\n"
                  "a=b==c&&d===e;\n"
                  "a>>>=b>=c?d>>_:$;");
  ExpectToken(JsKeywords::kIdentifier, "foo");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "/=");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "bar");
  ExpectToken(JsKeywords::kOperator,   "++");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kIdentifier, "baz");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectToken(JsKeywords::kOperator,   "==");
  ExpectToken(JsKeywords::kIdentifier, "c");
  ExpectToken(JsKeywords::kOperator,   "&&");
  ExpectToken(JsKeywords::kIdentifier, "d");
  ExpectToken(JsKeywords::kOperator,   "===");
  ExpectToken(JsKeywords::kIdentifier, "e");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   ">>>=");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectToken(JsKeywords::kOperator,   ">=");
  ExpectToken(JsKeywords::kIdentifier, "c");
  ExpectToken(JsKeywords::kOperator,   "?");
  ExpectToken(JsKeywords::kIdentifier, "d");
  ExpectToken(JsKeywords::kOperator,   ">>");
  ExpectToken(JsKeywords::kIdentifier, "_");
  ExpectToken(JsKeywords::kOperator,   ":");
  ExpectToken(JsKeywords::kIdentifier, "$");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Operators2) {
  BeginTokenizing("a+++b\n"
                  "a+ ++b\n"
                  "a+ +b\n"
                  "a---b\n"
                  "a-++b\n"
                  "!!b\n");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "++");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectToken(JsKeywords::kSemiInsert, "\n");

  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "++");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectToken(JsKeywords::kSemiInsert, "\n");

  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectToken(JsKeywords::kSemiInsert, "\n");

  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "--");
  ExpectToken(JsKeywords::kOperator,   "-");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectToken(JsKeywords::kSemiInsert, "\n");

  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "-");
  ExpectToken(JsKeywords::kOperator,   "++");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectToken(JsKeywords::kSemiInsert, "\n");

  ExpectToken(JsKeywords::kOperator,   "!");
  ExpectToken(JsKeywords::kOperator,   "!");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Colons) {
  // Each of the three lines below contains the substring ":{}/x/i".  However,
  // in the first two lines that's a label colon, followed by an empty block,
  // followed by a regex literal, while in the third line it's an ternary
  // operator, followed by an empty object literal, followed by division.
  BeginTokenizing("switch(x){default:{}/x/i}\n"
                  "foobar:{}/x/i;\n"
                  "a?b:{}/x/i");
  ExpectToken(JsKeywords::kSwitch,     "switch");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectToken(JsKeywords::kDefault,    "default");
  ExpectParseStack("Start { Other");
  ExpectToken(JsKeywords::kOperator,   ":");
  ExpectParseStack("Start {");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectToken(JsKeywords::kRegex,      "/x/i");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "foobar");
  ExpectParseStack("Start Expr");
  ExpectToken(JsKeywords::kOperator,   ":");
  ExpectParseStack("Start");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectToken(JsKeywords::kRegex,      "/x/i");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "?");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectParseStack("Start Expr ? Expr");
  ExpectToken(JsKeywords::kOperator,   ":");
  ExpectParseStack("Start Expr Oper");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectToken(JsKeywords::kOperator,   "/");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "/");
  ExpectToken(JsKeywords::kIdentifier, "i");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, ObjectLiteralsArray) {
  BeginTokenizing("[{a:42},{a:32}]");
  ExpectToken(JsKeywords::kOperator,   "[",  "Start [");
  ExpectToken(JsKeywords::kOperator,   "{",  "Start [ {");
  ExpectToken(JsKeywords::kIdentifier, "a",  "Start [ { Expr");
  ExpectToken(JsKeywords::kOperator,   ":",  "Start [ {");
  ExpectToken(JsKeywords::kNumber,     "42", "Start [ { Expr");
  ExpectToken(JsKeywords::kOperator,   "}",  "Start [ Expr");
  ExpectToken(JsKeywords::kOperator,   ",",  "Start [ Expr Oper");
  ExpectToken(JsKeywords::kOperator,   "{",  "Start [ Expr Oper {");
  ExpectToken(JsKeywords::kIdentifier, "a",  "Start [ Expr Oper { Expr");
  ExpectToken(JsKeywords::kOperator,   ":",  "Start [ Expr Oper {");
  ExpectToken(JsKeywords::kNumber,     "32", "Start [ Expr Oper { Expr");
  ExpectToken(JsKeywords::kOperator,   "}",  "Start [ Expr");
  ExpectToken(JsKeywords::kOperator,   "]",  "Start Expr");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, TrailingCommas) {
  BeginTokenizing("x={a:1,}\n"
                  "y=[,,]");
  ExpectToken(JsKeywords::kIdentifier, "x",  "Start Expr");
  ExpectToken(JsKeywords::kOperator,   "=",  "Start Expr Oper");
  ExpectToken(JsKeywords::kOperator,   "{",  "Start Expr Oper {");
  ExpectToken(JsKeywords::kIdentifier, "a",  "Start Expr Oper { Expr");
  ExpectToken(JsKeywords::kOperator,   ":",  "Start Expr Oper {");
  ExpectToken(JsKeywords::kNumber,     "1",  "Start Expr Oper { Expr");
  ExpectToken(JsKeywords::kOperator,   ",",  "Start Expr Oper { Expr Oper");
  ExpectToken(JsKeywords::kOperator,   "}",  "Start Expr");
  ExpectToken(JsKeywords::kSemiInsert, "\n", "Start");
  ExpectToken(JsKeywords::kIdentifier, "y",  "Start Expr");
  ExpectToken(JsKeywords::kOperator,   "=",  "Start Expr Oper");
  ExpectToken(JsKeywords::kOperator,   "[",  "Start Expr Oper [");
  ExpectToken(JsKeywords::kOperator,   ",",  "Start Expr Oper [ Oper");
  ExpectToken(JsKeywords::kOperator,   ",",  "Start Expr Oper [ Oper");
  ExpectToken(JsKeywords::kOperator,   "]",  "Start Expr");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, ObjectLiteralRegexLiteral) {
  // On the first line, this looks like it should be an object literal divided
  // by x divided by i, but nope, that's a block with a labelled expression
  // statement, followed by a regex literal.  The second line, on the other
  // hand, _is_ an object literal, followed by division.
  BeginTokenizing("{foo:1} / x/i;\n"
                  "x={foo:1} / x/i;");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectToken(JsKeywords::kIdentifier, "foo");
  ExpectToken(JsKeywords::kOperator,   ":");
  ExpectToken(JsKeywords::kNumber,     "1");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kRegex,      "/ x/i");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kOperator,   "{");
  ExpectToken(JsKeywords::kIdentifier, "foo");
  ExpectToken(JsKeywords::kOperator,   ":");
  ExpectToken(JsKeywords::kNumber,     "1");
  ExpectToken(JsKeywords::kOperator,   "}");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "/");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "/");
  ExpectToken(JsKeywords::kIdentifier, "i");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, EmptyBlockRegexLiteral) {
  BeginTokenizing("if(true){{}/foo/}");
  ExpectToken(JsKeywords::kIf,         "if",    "Start BkKwd");
  ExpectToken(JsKeywords::kOperator,   "(",     "Start BkKwd (");
  ExpectToken(JsKeywords::kTrue,       "true",  "Start BkKwd ( Expr");
  ExpectToken(JsKeywords::kOperator,   ")",     "Start BkHdr");
  ExpectToken(JsKeywords::kOperator,   "{",     "Start {");
  ExpectToken(JsKeywords::kOperator,   "{",     "Start { {");
  ExpectToken(JsKeywords::kOperator,   "}",     "Start {");
  ExpectToken(JsKeywords::kRegex,      "/foo/", "Start { Expr");
  ExpectToken(JsKeywords::kOperator,   "}",     "Start");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, TrickyRegexLiteral) {
  BeginTokenizing("var x=a[0] / b /i;\n"
                  "var y=a[0]+/ b /i;");
  ExpectToken(JsKeywords::kVar,        "var");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "[");
  ExpectToken(JsKeywords::kNumber,     "0");
  ExpectToken(JsKeywords::kOperator,   "]");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "/");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "/");
  ExpectToken(JsKeywords::kIdentifier, "i");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kVar,        "var");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "y");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "[");
  ExpectToken(JsKeywords::kNumber,     "0");
  ExpectToken(JsKeywords::kOperator,   "]");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kRegex,      "/ b /i");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, RegexLiteralsWithBrackets) {
  BeginTokenizing("/http:\\/\\/[^/]+\\// / /z[\\]/ ]/");
  // The / in [^/] doesn't end the regex.
  ExpectToken(JsKeywords::kRegex,      "/http:\\/\\/[^/]+\\//");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "/");
  ExpectToken(JsKeywords::kWhitespace, " ");
  // The first ] is escaped and doesn't close the [, so the following / doesn't
  // close the regex.
  ExpectToken(JsKeywords::kRegex,      "/z[\\]/ ]/");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, ReturnRegex) {
  // Make sure we understand that this is not division; "return" is not an
  // identifier!
  BeginTokenizing("return / x /g;\n"
                  "return/#.+/.test(\n'#24' );");
  ExpectToken(JsKeywords::kReturn,     "return");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kRegex,      "/ x /g");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kReturn,     "return");
  ExpectToken(JsKeywords::kRegex,      "/#.+/");
  ExpectToken(JsKeywords::kOperator,   ".");
  ExpectToken(JsKeywords::kIdentifier, "test");
  ExpectToken(JsKeywords::kOperator,   "(");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectToken(JsKeywords::kStringLiteral, "'#24'");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   ")");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, ReturnRegex2) {
  // Make sure we understand that this is not division; "return" is not an
  // identifier!
  BeginTokenizing("return / x /g;");
  ExpectToken(JsKeywords::kReturn,     "return");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kRegex,      "/ x /g");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, ThrowRegex) {
  // Make sure we understand that this is not division; "throw" is not an
  // identifier!  (And yes, in JS you're allowed to throw a regex.)
  BeginTokenizing("throw / x /g;");
  ExpectToken(JsKeywords::kThrow,      "throw");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kRegex,      "/ x /g");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, UnicodeRegexFlags) {
  // The Look Of Disapproval emoticon is probably not a semantically valid
  // regex flag, but it is lexically valid, so we should be able to tokenize
  // it.
  BeginTokenizing("/\xE2\x98\x83/\xE0\xB2\xA0_\xE0\xB2\xA0\xE2\x80\xA9;");
  ExpectToken(JsKeywords::kRegex, "/\xE2\x98\x83/\xE0\xB2\xA0_\xE0\xB2\xA0");
  ExpectToken(JsKeywords::kLineSeparator, "\xE2\x80\xA9");
  ExpectToken(JsKeywords::kOperator, ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, SemicolonInsertion1) {
  BeginTokenizing("3\n"  // Semicolon is not inserted here.
                  "// foo\n"
                  "--> foo\n"
                  "-5\n"  // Semicolon is inserted here.
                  "// bar\n"
                  "6");
  ExpectToken(JsKeywords::kNumber,     "3");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectToken(JsKeywords::kComment,    "// foo");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectToken(JsKeywords::kComment,    "--> foo");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectToken(JsKeywords::kOperator,   "-");
  ExpectToken(JsKeywords::kNumber,     "5");
  ExpectToken(JsKeywords::kSemiInsert, "\n");
  ExpectToken(JsKeywords::kComment,    "// bar");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectToken(JsKeywords::kNumber,     "6");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, SemicolonInsertion2) {
  BeginTokenizing("w\n"  // Semicolon is inserted here...
                  "++\n" // ...but not here.
                  "x\n"  // Semicolon inserted here again.
                  "y++\n"  // And here again.
                  "z");
  ExpectToken(JsKeywords::kIdentifier,    "w",  "Start Expr");
  ExpectToken(JsKeywords::kSemiInsert,    "\n", "Start");
  ExpectToken(JsKeywords::kOperator,      "++", "Start Oper");
  ExpectToken(JsKeywords::kLineSeparator, "\n", "Start Oper");
  ExpectToken(JsKeywords::kIdentifier,    "x",  "Start Expr");
  ExpectToken(JsKeywords::kSemiInsert,    "\n", "Start");
  ExpectToken(JsKeywords::kIdentifier,    "y",  "Start Expr");
  ExpectToken(JsKeywords::kOperator,      "++", "Start Expr");
  ExpectToken(JsKeywords::kSemiInsert,    "\n", "Start");
  ExpectToken(JsKeywords::kIdentifier,    "z",  "Start Expr");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, SemicolonInsertion3) {
  BeginTokenizing("x\nin\xE1\x9A\x80y;\n"  // U+1680 OGHAM SPACE MARK
                  "x\nin\\u0063\ny;\n"
                  "x\ninstanceof\ny;\n"
                  "x\ninstanceof\xE0\xB2\xA0_\n"  // U+0CA0 KANNADA LETTER TTHA
                  "y;");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectToken(JsKeywords::kIn,         "in");
  ExpectToken(JsKeywords::kWhitespace, "\xE1\x9A\x80");
  ExpectToken(JsKeywords::kIdentifier, "y");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kSemiInsert, "\n");
  ExpectToken(JsKeywords::kIdentifier, "in\\u0063");
  ExpectToken(JsKeywords::kSemiInsert, "\n");
  ExpectToken(JsKeywords::kIdentifier, "y");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectToken(JsKeywords::kInstanceof, "instanceof");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectToken(JsKeywords::kIdentifier, "y");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kSemiInsert, "\n");
  ExpectToken(JsKeywords::kIdentifier, "instanceof\xE0\xB2\xA0_");
  ExpectToken(JsKeywords::kSemiInsert, "\n");
  ExpectToken(JsKeywords::kIdentifier, "y");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, SemicolonInsertion4) {
  BeginTokenizing("x={}\n"
                  "{debugger}");
  ExpectToken(JsKeywords::kIdentifier, "x",        "Start Expr");
  ExpectToken(JsKeywords::kOperator,   "=",        "Start Expr Oper");
  ExpectToken(JsKeywords::kOperator,   "{",        "Start Expr Oper {");
  ExpectToken(JsKeywords::kOperator,   "}",        "Start Expr");
  ExpectToken(JsKeywords::kSemiInsert, "\n",       "Start");
  ExpectToken(JsKeywords::kOperator,   "{",        "Start {");
  ExpectToken(JsKeywords::kDebugger,   "debugger", "Start { Jump");
  ExpectToken(JsKeywords::kOperator,   "}",        "Start");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, UnclosedBlockComment) {
  BeginTokenizing("debugger; /* foo");
  ExpectToken(JsKeywords::kDebugger,   "debugger");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectError("/* foo");
}

TEST_F(JsTokenizerTest, UnclosedRegexLiteral) {
  BeginTokenizing("var bar=/quux;");
  ExpectParseStack("Start");
  ExpectToken(JsKeywords::kVar,        "var", "Start Other");
  ExpectToken(JsKeywords::kWhitespace, " ",   "Start Other");
  ExpectToken(JsKeywords::kIdentifier, "bar", "Start Other Expr");
  ExpectToken(JsKeywords::kOperator,   "=",   "Start Other Expr Oper");
  ExpectError("/quux;");
  ExpectParseStack("Start Other Expr Oper");
}

TEST_F(JsTokenizerTest, LinebreakInRegex) {
  // Regexes cannot contain linebreaks.
  BeginTokenizing("x=/foo\nquux/;");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectError("/foo\nquux/;");

  // They can contain Unicode characters, but not Unicode linebreaks.
  BeginTokenizing("x=/foo\xE2\x98\x83quux/+"  // U+2603 SNOWMAN
                  "/foo\xE2\x80\xA9quux/;");  // U+2029 PARAGRAPH SEPARATOR
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kRegex,      "/foo\xE2\x98\x83quux/");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectError("/foo\xE2\x80\xA9quux/;");

  // Unlike in strings, newlines in regexes cannot be escaped by backslashes.
  BeginTokenizing("x=/foo\\\nquux/;");
  ExpectToken(JsKeywords::kIdentifier, "x");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectError("/foo\\\nquux/;");
}

TEST_F(JsTokenizerTest, LinebreakInStringLiteral) {
  // Strings can contain linebreaks only if escaped by a backslash.
  BeginTokenizing("'foo\\\nquux'+"
                  // A CRLF counts as one linebreak.
                  "\"foo\\\r\nquux\"+"
                  // Apparently, so does LFCR, for some reason.
                  "\"foo\\\n\rquux\"+"
                  // But two LFs are two linebreaks!
                  "'foo\\\n\nquux';");
  ExpectToken(JsKeywords::kStringLiteral, "'foo\\\nquux'");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kStringLiteral, "\"foo\\\r\nquux\"");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kStringLiteral, "\"foo\\\n\rquux\"");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectError("'foo\\\n\nquux';");

  // Strings can contain Unicode characters.
  BeginTokenizing("'foo\xE2\x98\x83quux'+"    // U+2603 SNOWMAN
                  // Unicode linebreaks are allowed if backslash-escaped.
                  "'foo\\\xE2\x80\xA8quux'+"  // U+2028 LINE SEPARATOR
                  // But it's an error if the Unicode linebreak isn't escaped!
                  "'foo\xE2\x80\xA8quux';");  // U+2028 LINE SEPARATOR
  ExpectToken(JsKeywords::kStringLiteral, "'foo\xE2\x98\x83quux'");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kStringLiteral, "'foo\\\xE2\x80\xA8quux'");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectError("'foo\xE2\x80\xA8quux';");
}

TEST_F(JsTokenizerTest, EscapedQuotesInStringLiteral1) {
  BeginTokenizing("'foo\\\\\\'bar';");
  ExpectToken(JsKeywords::kStringLiteral, "'foo\\\\\\'bar'");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, EscapedQuotesInStringLiteral2) {
  BeginTokenizing("\"baz\"+'foo\\\\\\'ba\"r';");
  ExpectToken(JsKeywords::kStringLiteral, "\"baz\"");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kStringLiteral, "'foo\\\\\\'ba\"r'");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, EscapedQuotesInStringLiteral3) {
  BeginTokenizing("'b\\\\az'+'foo\\'bar';");
  ExpectToken(JsKeywords::kStringLiteral, "'b\\\\az'");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kStringLiteral, "'foo\\'bar'");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, EscapedQuotesInStringLiteral4) {
  BeginTokenizing("'b\\'az'+'foobar';");
  ExpectToken(JsKeywords::kStringLiteral, "'b\\'az'");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kStringLiteral, "'foobar'");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, EscapedQuotesInStringLiteral5) {
  BeginTokenizing("\"f\xFFoo\\\\\\\"bar\";");
  ExpectToken(JsKeywords::kStringLiteral, "\"f\xFFoo\\\\\\\"bar\"");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, UnclosedStringLiteral) {
  BeginTokenizing("bar='quux;");
  ExpectToken(JsKeywords::kIdentifier, "bar");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectError("'quux;");
}

TEST_F(JsTokenizerTest, UnmatchedCloseParen) {
  BeginTokenizing("bar='quux');");
  ExpectToken(JsKeywords::kIdentifier, "bar");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kStringLiteral, "'quux'");
  ExpectError(");");
}

TEST_F(JsTokenizerTest, BogusInputCharacter) {
  BeginTokenizing("var #foo;");
  ExpectToken(JsKeywords::kVar,        "var");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectError("#foo;");
}

TEST_F(JsTokenizerTest, BackslashesInIdentifier) {
  BeginTokenizing("a\\u03c0b");
  ExpectToken(JsKeywords::kIdentifier, "a\\u03c0b");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, BackslashesInString) {
  BeginTokenizing("\"a\\\"b\"");
  ExpectToken(JsKeywords::kStringLiteral, "\"a\\\"b\"");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, CombinePluses) {
  BeginTokenizing("a+++b");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "++");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, CombinePluses2) {
  BeginTokenizing("a+ ++b");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "++");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, CombinePlusesSpace) {
  BeginTokenizing("a+ +b");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, CombineMinuses) {
  BeginTokenizing("a---b");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "--");
  ExpectToken(JsKeywords::kOperator,   "-");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, CombineMixed) {
  BeginTokenizing("a--+b");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "--");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectEndOfInput();

}

TEST_F(JsTokenizerTest, CombineMixed2) {
  BeginTokenizing("a-++b");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   "-");
  ExpectToken(JsKeywords::kOperator,   "++");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, CombineBangs) {
  BeginTokenizing("!!b");
  ExpectToken(JsKeywords::kOperator,   "!");
  ExpectToken(JsKeywords::kOperator,   "!");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, NumbersAndDotsAndIdentifiersAndKeywords) {
  BeginTokenizing("return a.b+5.3");
  ExpectToken(JsKeywords::kReturn,     "return");
  ExpectToken(JsKeywords::kWhitespace, " ");
  ExpectToken(JsKeywords::kIdentifier, "a");
  ExpectToken(JsKeywords::kOperator,   ".");
  ExpectToken(JsKeywords::kIdentifier, "b");
  ExpectToken(JsKeywords::kOperator,   "+");
  ExpectToken(JsKeywords::kNumber,     "5.3");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, NumberProperty) {
  BeginTokenizing("1..property");
  ExpectToken(JsKeywords::kNumber,     "1.");
  ExpectToken(JsKeywords::kOperator,   ".");
  ExpectToken(JsKeywords::kIdentifier, "property");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, LineCommentAtEndOfInput) {
  BeginTokenizing("hello//world");
  ExpectToken(JsKeywords::kIdentifier, "hello");
  ExpectToken(JsKeywords::kComment,    "//world");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Latin1BlockComment) {
  // Try to tokenize input that is Latin-1 encoded.  This is not valid UTF-8,
  // but we should be able to proceed gracefully (in most cases) if the
  // non-ascii characters only ever appear in string literals and comments.
  BeginTokenizing("/* qu\xE9 pasa */\n");
  ExpectToken(JsKeywords::kComment, "/* qu\xE9 pasa */");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Latin1LineComment) {
  // Try to tokenize input that is Latin-1 encoded.  This is not valid UTF-8,
  // but we should be able to proceed gracefully (in most cases) if the
  // non-ascii characters only ever appear in string literals and comments.
  BeginTokenizing("// qu\xE9 pasa\n");
  ExpectToken(JsKeywords::kComment, "// qu\xE9 pasa");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Latin1StringLiteral) {
  // Try to tokenize input that is Latin-1 encoded.  This is not valid UTF-8,
  // but we should be able to proceed gracefully (in most cases) if the
  // non-ascii characters only ever appear in string literals and comments.
  BeginTokenizing("\"qu\xE9 pasa\"\n");
  ExpectToken(JsKeywords::kStringLiteral, "\"qu\xE9 pasa\"");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectEndOfInput();

  // An example with more complicated escaping:
  BeginTokenizing("'\xAA\\'\xBB\\\r\n\xCC'\n");
  ExpectToken(JsKeywords::kStringLiteral, "'\xAA\\'\xBB\\\r\n\xCC'");
  ExpectToken(JsKeywords::kLineSeparator, "\n");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, Latin1Input) {
  // Try to tokenize input that is Latin-1 encoded.  This is not valid UTF-8,
  // but we should be able to proceed gracefully (in most cases) if the
  // non-ascii characters only ever appear in string literals and comments.
  BeginTokenizing("str='Qu\xE9 pasa';// 'qu\xE9' means 'what'\n"
                  "cents=/* 73\xA2 is $0.73 */73;");

  ExpectToken(JsKeywords::kIdentifier, "str");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kStringLiteral, "'Qu\xE9 pasa'");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectToken(JsKeywords::kComment,    "// 'qu\xE9' means 'what'");
  ExpectToken(JsKeywords::kLineSeparator, "\n");

  ExpectToken(JsKeywords::kIdentifier, "cents");
  ExpectToken(JsKeywords::kOperator,   "=");
  ExpectToken(JsKeywords::kComment,    "/* 73\xA2 is $0.73 */");
  ExpectToken(JsKeywords::kNumber,     "73");
  ExpectToken(JsKeywords::kOperator,   ";");
  ExpectEndOfInput();
}

TEST_F(JsTokenizerTest, TokenizeAngular) {
  ExpectTokenizeFileSuccessfully("angular.original");
}

TEST_F(JsTokenizerTest, TokenizeJQuery) {
  ExpectTokenizeFileSuccessfully("jquery.original");
}

TEST_F(JsTokenizerTest, TokenizePrototype) {
  ExpectTokenizeFileSuccessfully("prototype.original");
}

}  // namespace
