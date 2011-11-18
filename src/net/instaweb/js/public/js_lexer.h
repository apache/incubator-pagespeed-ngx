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
  // A consumer of the Lexer supplies an instance of this callback class,
  // specifying implementations of the "protected:" methods.
  class Callback {
   public:
    Callback() : last_token_may_end_value_(false), error_(false) {}
    virtual ~Callback();

   protected:
    virtual void Keyword(JsKeywords::Keyword keyword) = 0;

    // Comments are passed to the callback including the comment
    // delimeter.  This same function is called for line-comments,
    // and block comments.
    virtual void Comment(const StringPiece& comment) = 0;

    // TODO(jmarantz): break out Newline() as a separate event,
    // otherwise a parser would have to re-examine the whitespace.
    virtual void Whitespace(const StringPiece& whitespace) = 0;

    virtual void Regex(const StringPiece& regex) = 0;

    // StringLiterals are passed with the quote delimeters.
    virtual void StringLiteral(const StringPiece& string_literal) = 0;

    virtual void Number(const StringPiece& number) = 0;

    // Note -- not all multi-character operators are tokenized properly yet.
    virtual void Operator(const StringPiece& op) = 0;

    virtual void Identifier(const StringPiece& identifier) = 0;

    // This is called by the Lexer whenever a new document is parsed.  Any
    // internal state from the callback can be cleared here in an override.
    virtual void Clear();

   private:
    friend class JsLexer;

    void IdentifierOrKeyword(const StringPiece& comment);
    void NumberOrDot(const StringPiece& comment);

    // Indicates whether the last token parsed is likely to be the
    // last token of a value.  This is only valid to look at right
    // after calling IdentifierOrKeyword() or NumberOrDot().  It is
    // used for figuring out whether a subsequent "/" indicates the
    // start of a regular-expression token or something else (comment
    // or divide).
    bool last_token_may_end_value() const { return last_token_may_end_value_; }

    // Inicates whether an error was detected interpreting a token.
    bool error() const { return error_; }

    bool last_token_may_end_value_;
    bool error_;

    DISALLOW_COPY_AND_ASSIGN(Callback);
  };

  JsLexer();
  bool Lex(const StringPiece& contents, Callback* callback);
  const char* keyword_string(JsKeywords::Keyword keyword) {
    return keyword_vector_[static_cast<int>(keyword)];
  }

 private:
  // Method used to determine whether we are still in a particular
  // Lexer state.
  typedef bool (JsLexer::*LexicalPredicate)(uint8 ch, int index);

  // Method used to run a Callback method on exiting a state.
  typedef void (Callback::*CallbackFunction)(const StringPiece& piece);

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
  void Consume(CallbackFunction fn,
               LexicalPredicate predicate,
               bool include_last_char,
               bool ok_to_terminate_with_eof);

  bool IsSpace(uint8 ch, int index);
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

  void ConsumeSlash();

  Callback* callback_;
  StringPiece input_;
  int index_;
  int prev_char_;
  int token_start_;
  int token_start_index_;
  int dot_count_;
  bool error_;
  bool backslash_mode_;
  bool last_token_is_value_;
  bool within_brackets_;
  bool seen_a_dot_;

  CharStarVector keyword_vector_;

  DISALLOW_COPY_AND_ASSIGN(JsLexer);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_JS_PUBLIC_JS_LEXER_H_
