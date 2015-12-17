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

#ifndef PAGESPEED_KERNEL_JS_JS_TOKENIZER_H_
#define PAGESPEED_KERNEL_JS_JS_TOKENIZER_H_

#include <deque>
#include <utility>
#include <vector>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/js/js_keywords.h"
#include "pagespeed/kernel/util/re2.h"

namespace pagespeed {

namespace js {

struct JsTokenizerPatterns;

// This class accurately breaks up JavaScript code into a sequence of tokens.
// This includes tokens for comments and whitespace; every byte of the input is
// represented in the token stream, so that concatenating the text of each
// token will perfectly recover the original input, even in error cases (since
// the final, error token will contain the entire rest of the input).  Also,
// each whitespace token is classified by the tokenizer as 1) not containing
// linebreaks, 2) containing linebreaks but not inducing semicolon insertion,
// or 3) inducing semicolon insertion.
//
// To do all this, JsTokenizer keeps track of a minimal amount of parse state
// to allow it to accurately differentiate between division operators and regex
// literals, and to determine which linebreaks will result in semicolon
// insertion and which will not.  If the given JavaScript code is syntactically
// incorrect such that this differentiation becomes impossible, this class will
// return an error, but will still tokenize as much as it can up to that point
// (note however that many other kinds of syntax errors will be ignored; being
// a complete parser or syntax checker is a non-goal of this class).
//
// This class can also be used to tokenize JSON.  Note that a JSON object, such
// as {"foo":"bar"}, is NOT legal JavaScript code by itself (since, absent any
// context, the braces will be interpreted as a code block rather than as an
// object literal); however, JsTokenizer contains special logic to recognize
// this case and still tokenize it correctly.
//
// This separation of tokens and classification of whitespace means that this
// class can be used to create a robust JavaScript minifier (see js_minify.h).
// It could also perhaps be used as the basis of a more complete JavaScript
// parser.
class JsTokenizer {
 public:
  // Creates a tokenizer that will tokenize the given UTF8-encoded input string
  // (which must outlive the JsTokenizer object).
  JsTokenizer(const JsTokenizerPatterns* patterns, StringPiece input);

  ~JsTokenizer();

  // Gets the next token type from the input, and stores the relevant substring
  // of the original input in token_out (which must be non-NULL).  If the end
  // of input has been reached, returns kEndOfInput and sets token_out to the
  // empty string.  If an error is encountered, sets has_error() to true,
  // returns kError, and sets token_out to the remainder of the input.
  JsKeywords::Type NextToken(StringPiece* token_out);

  // True if an error has been encountered.  All future calls to NextToken()
  // will return JsKeywords::kError with an empty token string.
  bool has_error() const { return error_; }

  // Return a string representing the current parse stack, for testing only.
  GoogleString ParseStackForTest() const;

 private:
  // An entry in the parse stack.  This does not fully capture the grammar of
  // JavaScript -- far from it -- rather, it is just barely nuanced enough to
  // determine which linebreaks are important for semicolon insertion, and to
  // tell whether or not a given slash begins a regex literal.  If it turns out
  // to insufficiently nuanced (i.e. we find new bugs), it can be refined by
  // adding more parse states.
  enum ParseState {
    kStartOfInput,  // For convenience, the bottom of the stack is always this.
    kExpression,
    kOperator,      // A prefix or binary operator (including some keywords).
    kPeriod,
    kQuestionMark,
    kOpenBrace,
    kOpenBracket,
    kOpenParen,
    kBlockKeyword,  // Keyword that precedes "(...)", e.g. "if" or "for".
    kBlockHeader,   // Start of block, e.g. "if (...)", "for (...)", or "else".
    kReturnThrow,   // A return or throw keyword.
    kJumpKeyword,   // A break, continue, or debugger keyword.
    kOtherKeyword,  // A const, default, or var keyword.
  };

  // Enum for tracking whether the first three tokens in the input are open
  // brace, string literal, colon.  If so, we're parsing a JSON object,
  // otherwise we'll assume we're parsing legal JS code.
  enum JsonStep {
    kJsonStart,
    kJsonOpenBrace,
    kJsonOpenBraceStringLiteral,
    kIsJsonObject,
    kIsNotJsonObject,
  };

  // Consumes an appropriate amount of input and return an appropriate token.
  JsKeywords::Type ConsumeOpenBrace(StringPiece* token_out);
  JsKeywords::Type ConsumeCloseBrace(StringPiece* token_out);
  JsKeywords::Type ConsumeOpenBracket(StringPiece* token_out);
  JsKeywords::Type ConsumeCloseBracket(StringPiece* token_out);
  JsKeywords::Type ConsumeOpenParen(StringPiece* token_out);
  JsKeywords::Type ConsumeCloseParen(StringPiece* token_out);
  JsKeywords::Type ConsumeBlockComment(StringPiece* token_out);
  JsKeywords::Type ConsumeLineComment(StringPiece* token_out);
  JsKeywords::Type ConsumeColon(StringPiece* token_out);
  JsKeywords::Type ConsumeComma(StringPiece* token_out);
  JsKeywords::Type ConsumeNumber(StringPiece* token_out);
  JsKeywords::Type ConsumeOperator(StringPiece* token_out);
  JsKeywords::Type ConsumePeriod(StringPiece* token_out);
  JsKeywords::Type ConsumeQuestionMark(StringPiece* token_out);
  JsKeywords::Type ConsumeRegex(StringPiece* token_out);
  JsKeywords::Type ConsumeSemicolon(StringPiece* token_out);
  JsKeywords::Type ConsumeSlash(StringPiece* token_out);
  JsKeywords::Type ConsumeString(StringPiece* token_out);

  // For each of these methods, if the start of the input is that kind of
  // token, consumes the token and returns true, otherwise returns false
  // without making changes.
  bool TryConsumeComment(
      JsKeywords::Type* type_out, StringPiece* token_out);
  bool TryConsumeIdentifierOrKeyword(
      JsKeywords::Type* type_out, StringPiece* token_out);
  bool TryConsumeWhitespace(
      bool allow_semicolon_insertion,
      JsKeywords::Type* type_out, StringPiece* token_out);

  // Sets error_ to true and returns an error token.
  JsKeywords::Type Error(StringPiece* token_out);

  // Stores the next num_chars characters of the input into *token_out, and
  // then increment the start of input_ by num_chars characters.  If the token
  // type is not comment or whitespace, sets start_of_line_ to false.  Also
  // updates json_step_ based on the token type.  Returns the token type passed
  // in, for convenience.
  JsKeywords::Type Emit(JsKeywords::Type type, int num_chars,
                        StringPiece* token_out);

  // Pushes a new state onto the parse_stack_, merging states as needed.
  void PushBlockHeader();
  void PushExpression();
  void PushOperator();

  // If a semicolon will be inserted between the previous token and the next
  // token (assuming there was a linebreak in between) that _wouldn't_ be
  // inserted if the linebreak weren't there, update the parse stack to reflect
  // the semicolon insertion and return true; otherwise do nothing and return
  // false.
  bool TryInsertLinebreakSemicolon();

  // Returns true if an open brace at this parse state begins an object
  // literal, or false if it begins a block.
  static bool CanPreceedObjectLiteral(ParseState state);

  const JsTokenizerPatterns* patterns_;
  std::vector<ParseState> parse_stack_;
  std::deque<std::pair<JsKeywords::Type, StringPiece> > lookahead_queue_;
  StringPiece input_;  // The portion of input that has yet to be consumed.
  JsonStep json_step_;
  bool start_of_line_;  // No non-whitespace/comment tokens on this line yet.
  bool error_;

  DISALLOW_COPY_AND_ASSIGN(JsTokenizer);
};

// Structure to store RE2 patterns that can be shared by instances of
// JsTokenizer.  These patterns are slightly expensive to compile, so we'd
// rather not create one for every JsTokenizer instance, but unfortunately C++
// static initializers can run in non-deterministic order and cause other
// integration issues.  Instead, you must create a JsTokenizerPatterns object
// yourself and pass it to the JsTokenizer constructor; ideally, you would just
// create one and share it for all JsTokenizer instances.
struct JsTokenizerPatterns {
 public:
  JsTokenizerPatterns();
  ~JsTokenizerPatterns();

  const RE2 identifier_pattern;
  const RE2 line_comment_pattern;
  const RE2 numeric_literal_pattern;
  const RE2 operator_pattern;
  const RE2 regex_literal_pattern;
  const RE2 string_literal_pattern;
  const RE2 whitespace_pattern;
  const RE2 line_continuation_pattern;

 private:
  DISALLOW_COPY_AND_ASSIGN(JsTokenizerPatterns);
};

}  // namespace js

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_JS_JS_TOKENIZER_H_
