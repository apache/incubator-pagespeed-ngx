// Copyright 2011 Google Inc. All Rights Reserved.
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
// Author: jmarantz@google.com
//
// This is based on third_party/libpagespeed/src/pagespeed/js/js_minify.cc by
// mdsteele@google.com

#ifndef NET_INSTAWEB_JS_PUBLIC_JS_LEXER_H_
#define NET_INSTAWEB_JS_PUBLIC_JS_LEXER_H_

#include "net/instaweb/js/public/js_keywords.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

// Lexical analysis class for Javascript.
class JsLexer {
 public:
  JsLexer();
  void Lex(const StringPiece& contents);
  const char* keyword_string(JsKeywords::Type keyword) {
    return keyword_vector_[static_cast<int>(keyword)];
  }

  // Grabs the next token from the stream.
  JsKeywords::Type NextToken(StringPiece* token);

  // Was there an error in the stream?
  bool error() const { return error_; }

 private:
  // Method used to determine whether we are still in a particular
  // Lexer state.
  typedef bool (JsLexer::*LexicalPredicate)(uint8 ch, int index);

  JsKeywords::Type IdentifierOrKeyword(const StringPiece& name);
  JsKeywords::Type NumberOrDot(const StringPiece& number_or_dot);

  // Walks through input text looking for the end of the current token.
  // When predicate(char, index) returns false, the token is over, and
  // the callback 'fn' is called with a StringPiece of the character
  // bounds of the token.
  //
  // If 'include_last_char' is specified, then the terminating
  // character is included in the StringPiece passed to 'fn'.  If
  // ok_to_terminate_with_eof is false and the input text ends before
  // predicate() returns false, then an error is signaled, resulting
  // in Lex() returning false.  However, the in-progress token is
  // passed to 'fn'.
  void Consume(LexicalPredicate predicate,
               bool include_last_char,
               bool ok_to_terminate_with_eof,
               StringPiece* token);

  bool IsSpace(uint8 ch, int index);
  bool IsLineSeparator(uint8 ch, int index);
  bool IsNumber(uint8 ch, int index);
  bool InBlockComment(uint8 ch, int index);
  bool InSingleLineComment(uint8 ch, int index);
  bool InIdentifier(uint8 ch, int index);
  bool InOperator(uint8 ch, int index);
  bool InString(uint8 ch, int index);
  bool InRegex(uint8 ch, int index);

  // Returns 'true' if this is the start of an identifier.
  bool IdentifierStart(uint8 ch);

  // If the character is a backslash, updates backslash_mode_ and returns
  // true, so the caller can skip over the next character, as indicated by
  // lexical context.
  bool ProcessBackslash(uint8 ch);

  JsKeywords::Type ConsumeSlash(StringPiece* token);

  StringPiece input_;
  int index_;
  int prev_char_;
  int token_start_;
  int token_start_index_;
  int dot_count_;
  bool error_;
  bool backslash_mode_;
  bool last_token_may_end_value_;
  bool within_brackets_;
  bool seen_a_dot_;

  CharStarVector keyword_vector_;

  DISALLOW_COPY_AND_ASSIGN(JsLexer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_JS_PUBLIC_JS_LEXER_H_
