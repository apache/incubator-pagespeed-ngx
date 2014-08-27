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

// Tokenizing JavaScript is tricky.  Most programming languages can be lexed
// and parsed separately; for example, in Java, given the code fragment "(x +
// y) / z", you can divide it up into tokens "(", "x", "+", and so on without
// keeping track of previous tokens, whether the parens match up, etc., and
// once tokenized you can parse based on that token stream without remembering
// any of the whitespace or comments that appeared between the tokens.  In
// JavaScript, neither of these things are true.  In the above Java example,
// that slash is a division operator, but in JavaScript it *could* instead be
// the start of a regex literal if the token before the "(" was e.g. "if";
// therefore you have to keep track of the parse state.  Moreover, whitespace
// can sometimes matter in JavaScript due to semicolon insertion, and
// determining whether a given piece of whitespace matters or not requires not
// only *previous* parse state, but also the ability to look *ahead* to the
// next token (something that even other whitespace-significant languages, like
// Python or Haskell, don't require).  The goal of this class is to correctly
// tokenize JavaScript code with as little code as possible, by not being a
// full parser but still keeping track of some minimal parse state.
//
// We keep a stack of ParseState values, and in general most tokens will push a
// new state onto the stack, possibly after popping off other states.
// Examining the stack helps us to disambiguate the meanings of certain
// characters (like slashes).  So how many different ParseState values do we
// need?  The big three questions we have to be able to answer are: (1) Is this
// slash division or a regex?  (2) Are these braces a code block or an object
// literal?  (This matters primarily because a slash after a code block is a
// regex, and a slash after an object literal is division.)  (3) Does this
// linebreak induce semicolon insertion or not?  The different ParseState
// values we have exist to answer these questions.
//
// - kStartOfInput exists as a convenience.  It is only ever used at the bottom
//   of the stack, and the bottom of the stack is always kStartOfInput.  It's
//   just there so that we can always assume the stack is nonempty and thus we
//   can always read its top value.
//
// - kExpression is for expressions.  A slash after this is division.  An open
//   brace after this is an error.  A linebreak after this may or may not
//   insert a semicolon, depending on the next token.
//
// - kOperator is for prefix and binary operators, including keywords like
//   "in".  A slash after this is a regex, and braces after this are an object
//   literal.  (Note that postfix operators don't need a parse state, because a
//   postfix operator must follow an expression, and an expression followed by
//   a postfix operator is still just an expresion.)
//
// - kQuestionMark is for the "?" character.  It behaves just like other
//   operators, but we must track it separately in order to determine whether a
//   given ":" character is for a label or a ternary operator.  This matters
//   because "foo:{}" is a label and code block, while "a?foo:{}" is a ternary
//   operator and object literal.
//
// - kOpenBrace, kOpenBracket, and kOpenParen are for opening delimiters.  When
//   we encounter a closing delimiter, we pop back to the matching open
//   delimiter and then modify the stack from there depending on what was just
//   created (e.g. an expression, or a block header, or something else).
//
// - kBlockKeyword is for keywords like "if" and "for" that are followed by
//   parentheses.  We track these so we know whether a pair of parens forms an
//   expression like "(a+b)" (after which a slash is division) or a block
//   header like "if(a>b)" (after which a slash is a regex).
//
// - kBlockHeader is a completed block header, like "if(a>b)".  Certain other
//   keywords like "do" and "else" are block headers on their own.
//
// - Lastly, we're left with eight keywords that don't fit into any of the
//   above categories.  We group these into three parse states:
//
//     - kReturnThrow for "return" and "throw".  They're sort of like prefix
//       operators in that a slash after these is a regex, but a linebreak
//       after these *always* inserts a semicolon.
//
//     - kJumpKeyword for "break", "continue", and "debugger".  A slash after
//       these is an error, and a linebreak after these *always* inserts a
//       semicolon.
//
//     - kOtherKeyword for "const", "default", and "var".  A slash after these
//       is an error too, but a linebreak after these *never* inserts a
//       semicolon.
//
// To help make the above more concrete, suppose we're parsing the code:
//
//   if ([]) {
//     foo: while(true) break;
//   } else /x/.test('y');
//
// The progression of the parse stack would look like this:
//
//   if       -> BkKwd             "if" is a block keyword, so it needs (...).
//   (        -> BkKwd (
//   [        -> BkKwd ( [
//   ]        -> BkKwd ( Expr      [] is an expression (array literal).
//   )        -> BkHdr             Now "if (...)" is a complete block header.
//   {        -> {                 We can forget about the BkHdr now.
//   foo      -> { Expr            An identifier is usually an expression...
//   :        -> {                 ...nevermind, a label.  Roll back statement.
//   while    -> { BkKwd           "while" is a block keyword, just like "if".
//   ( true ) -> { BkHdr           Three more tokens gives us the block header.
//   break    -> { BkHdr Jump      "break" is special, slashes can't follow it.
//   ;        -> {                 Semicolon, roll back to start-of-statement.
//   }        ->                   Block finished.
//   else     -> BkHdr             "else" is a block header by itself.
//   /x/      -> BkHdr Expr        A slash after BkHdr is a regex.
//   .        -> BkHdr Expr Oper   A period is essentially a binary operator.
//   test     -> BkHdr Expr        "Expr Oper Expr" collapses to "Expr"
//   (        -> BkHdr Expr (
//   'y'      -> BkHdr Expr ( Expr
//   )        -> BkHdr Expr        Method call collapses into a single Expr.
//   ;        ->                   Semicolon, roll back to start-of-statement.
//
// In general, this class is focused on tokenizing, not actual parsing or
// detecting syntax errors, so there are many kinds of syntax errors that we
// don't detect and will simply ignore (such as "break 42;", which can be
// reasonably split into tokens even if it doesn't actually parse).  But we
// *must* abort whenever the parse state becomes too mangled for us to make
// meaningful decisions about what slashes mean.  For example, in the code
// "[a}/x/i", are those slashes a regex literal or division?  The question has
// no answer.  They'd be division if the code were "[a]/x/i", and a regex if
// the code were "{a}/x/i", but faced with "[a}", we have little choice but to
// abort.
//
// More information about semicolon insertion can be found here:
//   http://inimino.org/~inimino/blog/javascript_semicolons

#include "pagespeed/kernel/js/js_tokenizer.h"

#include <stddef.h>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/js/js_keywords.h"
#include "pagespeed/kernel/util/re2.h"

namespace pagespeed {

namespace js {

namespace {

// Regex to match JavaScript identifiers.  For details, see page 18 of
// http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-262.pdf
const char* const kIdentifierRegex =
    // An identifier must begin with a $, _, unicode letter (more specifically,
    // a character in the Lu, Ll, Lt, Lm, Lo, or Nl category), or unicode
    // escape.
    "([$_\\p{Lu}\\p{Ll}\\p{Lt}\\p{Lm}\\p{Lo}\\p{Nl}]|\\\\u[0-9A-Fa-f]{4})"
    // After that, an identifier may have zero or more characters that are one
    // of the above, a combining mark (Mn or Mc), a digit (Nd), a connector
    // punctuation (Pc) or one of the characters ZERO WIDTH NON-JOINER (U+200C)
    // or ZERO WIDTH JOINER (U+200D).
    "([$_\\p{Lu}\\p{Ll}\\p{Lt}\\p{Lm}\\p{Lo}\\p{Nl}\\p{Mn}\\p{Mc}\\p{Nd}"
    "\\p{Pc}\xE2\x80\x8C\xE2\x80\x8D]|\\\\u[0-9A-Fa-f]{4})*";

// Regex to match JavaScript line comments.  This regex contains exactly one
// capturing group, which will match the linebreak (or end-of-input) that
// terminated the line comment.
const char* const kLineCommentRegex =
    "(?://|<!--|-->)\\C*?([\r\n\\p{Zl}\\p{Zp}]|\\z)";

// Regex to match JavaScript numeric literals.  This must be compiled in POSIX
// mode, so that the |'s are leftmost-longest rather than leftmost-first.
const char* const kNumericLiteralPosixRegex =
    // A number can be a hexadecimal literal, or...
    "0[xX][0-9a-fA-F]+|"
    // ...it can be a octal literal, or...
    "0[0-7]+|"
    // ...it can be a decimal literal.  To qualify as a decimal literal, it
    // must 1) start with a nonzero digit, or 2) start with zero but contain
    // a non-octal digit (8 or 9) in there somewhere, or 3) be a single zero
    // digit.
    "(([1-9][0-9]*|0([0-9]*[89][0-9]*)?)"
    // A decimal literal may optionally be followed by a decimal point and
    // fractional part:
    "(\\.[0-9]*)?"
    // Alternatively, instead of all that, a decimal literal may instead
    // start with a decimal point (instead of starting with a digit).
    "|\\.[0-9]+)"
    // Finally, any of the above kinds of decimal literal may optionally be
    // followed by an exponent.
    "([eE][+-]?[0-9]+)?";

// Regex to match JavaScript operators.
const char* const kOperatorRegex =
    // && || ++ --
    "&&|\\|\\||\\+\\+|--|"
    // * *= / /= % %= ^ ^= & &= | |= + += - -=
    "[*/%^&|+-]=?|"
    // ! != !== = == ===
    "[!=]={0,2}|"
    // < <= << <<=
    "<{1,2}=?|"
    // > >= >> >>= >>> >>>=
    ">{1,3}=?|"
    // Other operators:
    "[~,]";

// Regex to match JavaScript regex literals.  For details, see page 25 of
// http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-262.pdf
const char* const kRegexLiteralRegex =
    // Regex literals can contain characters that aren't slashes, backslashes,
    // open brackets, or linebreaks.
    "/([^/\\\\\\[\r\n\\p{Zl}\\p{Zp}]|"
    // They can also contain character classes, which are enclosed in square
    // brackets.  Within the brackets, close brackets and backslashes must be
    // escaped.  Linebreaks are *never* permitted -- not even if escaped.
    "\\[([^\\]\\\\\r\n\\p{Zl}\\p{Zp}]|"
    "\\\\[^\r\n\\p{Zl}\\p{Zp}])*\\]|"
    // Finally, they can contain escape sequences.  Again, linebreaks are
    // forbidden and cannot be escaped.
    "\\\\[^\r\n\\p{Zl}\\p{Zp}])+/"
    // Regex literals may optionally be followed by zero or more flags, which
    // can consist of any characters allowed within identifiers (even \uXXXX
    // escapes!); see kIdentifierRegex for details.  (Very few of these
    // characters are actually semantically valid regex flags, but they're all
    // lexically valid.)
    "([$_\\p{Lu}\\p{Ll}\\p{Lt}\\p{Lm}\\p{Lo}\\p{Nl}\\p{Mn}\\p{Mc}\\p{Nd}"
    "\\p{Pc}\xE2\x80\x8C\xE2\x80\x8D]|\\\\u[0-9A-Fa-f]{4})*";

// Regex to match JavaScript string literals.  For details, see page 22 of
// http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-262.pdf
// This regex will still match when given a string literal containing an
// unescaped linebreak, but the match will terminate after the linebreak; the
// caller must then check whether the start and end characters of the match are
// the same (both single quote or both double quote), and reject it if not.
const char* const kStringLiteralRegex =
    // Single-quoted string literals can contain any characters that aren't
    // single quotes, backslashes, or linebreaks.  They can also contain escape
    // sequences, which is a backslash followed either by a linebreak or by any
    // one character.  But note that the sequence \r\n counts as *one*
    // linebreak for this purpose, as does \n\r.  Finally, we use RE2's \C
    // escape for matching arbitrary bytes, along with very careful use of
    // greedy and non-greedy operators, to allow the string literal to contain
    // invalid UTF-8 characters, in case we're given e.g. Latin1-encoded input.
    // This is subtle and fragile, but fortunately we have unit tests that will
    // break if we ever get this wrong.
    //
    // This would be easier if there were a way to say "match an invalid UTF8
    // byte only", but apparently there is no way to do this in RE2.
    // See https://groups.google.com/forum/#!topic/re2-dev/26wVIHcowh4
    "'(\\C*?(\\\\(\r\n|\n\r|\n|.))?)*?['\n\r\\p{Zl}\\p{Zp}]|"
    // A string literal can also be double-quoted instead, which is the same,
    // except that double quotes must be escaped instead of single quotes.
    "\"(\\C*?(\\\\(\r\n|\n\r|\n|.))?)*?[\"\n\r\\p{Zl}\\p{Zp}]";

// Regex to match JavaScript whitespace.  For details, see page 15 of
// http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-262.pdf
// This regex contains exactly one capturing group; iff it captures anything,
// then the whitespace contains at least one linebreak.
const char* const kWhitespaceRegex =
    // Line separators include \n, \r, and characters in the "Line Separator"
    // (Zl) and "Paragraph Separator" (Zp) Unicode categories.
    "(?:([\n\r\\p{Zl}\\p{Zp}])|"
    // Horizontal whitespace includes space, \f, \t, \v, BYTE ORDER MARK
    // (U+FEFF), and characters in the "Space Separator" (Zs) Unicode category.
    "[ \f\t\v\xEF\xBB\xBF\\p{Zs}])+";

// Regex to check if the next token in the remaining input could continue the
// current statement, assuming the current statement currently ends with an
// expression.  (Note that this regex will not necessarily capture the entire
// next token; the only useful information to be had from it is whether it
// matches at all or not).
const char* const kLineContinuationRegex =
    // Any operator (even a multicharacter operator) starting with one of the
    // following characters can continue the current expression.
    "[=(*/%^&|<>?:,.]|"
    // A != can continue immediately after an expression, but not a !.
    "!=|"
    // A + or - can continue after an expression, but not a ++ or -- (because
    // JavaScript's grammar specifically forbids linebreaks between the two
    // tokens in "i++" or in "i--").
    "\\+($|[^+])|-($|[^-])|"
    // Finally, the in or instanceof operators can continue, though we have to
    // be sure we're not just looking at an identifier that starts with "in",
    // so make sure the "in" or "instanceof" is not followed by an identifier
    // character (see kIdentifierRegex for details).
    "(in|instanceof)($|[^$_\\p{Lu}\\p{Ll}\\p{Lt}\\p{Lm}\\p{Lo}\\p{Nl}\\p{Mn}"
    "\\p{Mc}\\p{Nd}\\p{Pc}\xE2\x80\x8C\xE2\x80\x8D\\\\])";

}  // namespace

JsTokenizer::JsTokenizer(const JsTokenizerPatterns* patterns,
                         StringPiece input)
    : patterns_(patterns), input_(input),
      start_of_line_(true), error_(false) {
  parse_stack_.push_back(kStartOfInput);
}

JsTokenizer::~JsTokenizer() {}

JsKeywords::Type JsTokenizer::NextToken(StringPiece* token_out) {
  // Empty out the lookahead queue before we scan any further.
  if (!lookahead_queue_.empty()) {
    const JsKeywords::Type type = lookahead_queue_.front().first;
    *token_out = lookahead_queue_.front().second;
    lookahead_queue_.pop_front();
    return type;
  }
  // If we've already encountered an error, just keep returning an error token.
  if (error_) {
    return Error(token_out);
  }
  // If we've cleanly reached the end of the input, we're done.
  if (input_.empty()) {
    parse_stack_.clear();
    token_out->clear();
    return JsKeywords::kEndOfInput;
  }
  // Invariant: until we reach the end of the input, the parse stack is never
  // empty, and the bottom entry is always kStartOfInput.  This is for
  // convenience, so that elsewhere we don't have to keep testing whether the
  // parse stack is empty before looking at the top entry.
  DCHECK(!parse_stack_.empty());
  DCHECK_EQ(kStartOfInput, parse_stack_[0]);
  // Scan and return the next token.
  const char ch = input_[0];
  switch (ch) {
    case ' ':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
      // This covers ASCII whitespace (which is the common case).  Unicode
      // whitespace is detected in the default case below.
      {
        JsKeywords::Type type;
        if (!TryConsumeWhitespace(true, &type, token_out)) {
          LOG(DFATAL) << "TryConsumeWhitespace failed on ASCII whitespace: "
                      << static_cast<int>(ch);
          return Error(token_out);
        }
        return type;
      }
    case '{':
      return ConsumeOpenBrace(token_out);
    case '}':
      return ConsumeCloseBrace(token_out);
    case '[':
      return ConsumeOpenBracket(token_out);
    case ']':
      return ConsumeCloseBracket(token_out);
    case '(':
      return ConsumeOpenParen(token_out);
    case ')':
      return ConsumeCloseParen(token_out);
    case ':':
      return ConsumeColon(token_out);
    case '.':
      return ConsumePeriod(token_out);
    case '?':
      return ConsumeQuestionMark(token_out);
    case ';':
      return ConsumeSemicolon(token_out);
    case '/':
      return ConsumeSlash(token_out);
    case '\'':
    case '"':
      return ConsumeString(token_out);
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      // Numeric literals (whether decimal, hex, or octal) start either with a
      // digit or with a period.  This line covers the starts-with-digit case,
      // while ConsumePeriod above checks for the starts-with-period case.
      return ConsumeNumber(token_out);
    default:
      {
        JsKeywords::Type type;
        if (TryConsumeIdentifierOrKeyword(&type, token_out) ||
            TryConsumeComment(&type, token_out) ||
            TryConsumeWhitespace(true, &type, token_out)) {
          return type;
        }
        // If all else fails, maybe this is an operator.  If not,
        // ConsumeOperator will return an error token.
        return ConsumeOperator(token_out);
      }
  }
}

GoogleString JsTokenizer::ParseStackForTest() const {
  GoogleString output;
  for (std::vector<ParseState>::const_iterator iter = parse_stack_.begin();
       iter != parse_stack_.end(); ++iter) {
    if (!output.empty()) {
      output.push_back(' ');
    }
    switch (*iter) {
      case kStartOfInput: output.append("Start"); break;
      case kExpression:   output.append("Expr");  break;
      case kOperator:     output.append("Oper");  break;
      case kQuestionMark: output.append("?");     break;
      case kOpenBrace:    output.append("{");     break;
      case kOpenBracket:  output.append("[");     break;
      case kOpenParen:    output.append("(");     break;
      case kBlockKeyword: output.append("BkKwd"); break;
      case kBlockHeader:  output.append("BkHdr"); break;
      case kReturnThrow:  output.append("RetTh"); break;
      case kJumpKeyword:  output.append("Jump");  break;
      case kOtherKeyword: output.append("Other"); break;
      default:
        LOG(DFATAL) << "Unknown parse state: " << *iter;
        output.append("UNKNOWN");
        break;
    }
  }
  return output;
}

JsKeywords::Type JsTokenizer::ConsumeOpenBrace(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ('{', input_[0]);
  const ParseState state = parse_stack_.back();
  if (state == kExpression || state == kBlockKeyword ||
      state == kJumpKeyword || state == kOtherKeyword) {
    return Error(token_out);
  } else if (state == kBlockHeader) {
    parse_stack_.pop_back();
  }
  parse_stack_.push_back(kOpenBrace);
  Emit(1, true, token_out);
  return JsKeywords::kOperator;
}

JsKeywords::Type JsTokenizer::ConsumeCloseBrace(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ('}', input_[0]);
  // Pop the most recent kOpenBrace (and everything above it) off the stack.
  while (true) {
    DCHECK(!parse_stack_.empty());
    const ParseState state = parse_stack_.back();
    if (state == kOpenBrace) {
      parse_stack_.pop_back();
      break;
    } else if (state == kStartOfInput || state == kOpenBracket ||
               state == kOpenParen || state == kBlockKeyword) {
      return Error(token_out);
    } else {
      parse_stack_.pop_back();
    }
  }
  // Depending on the parse state that came before the kOpenBrace, we just
  // closed either an object literal (which is a kExpression), or a block
  // (which isn't).
  DCHECK(!parse_stack_.empty());
  const ParseState state = parse_stack_.back();
  if (state == kOperator || state == kQuestionMark || state == kOpenBracket ||
      state == kOpenParen || state == kReturnThrow) {
    PushExpression();
  }
  // Emit a token for the close brace.
  Emit(1, true, token_out);
  return JsKeywords::kOperator;
}

JsKeywords::Type JsTokenizer::ConsumeOpenBracket(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ('[', input_[0]);
  const ParseState state = parse_stack_.back();
  if (state == kBlockKeyword || state == kJumpKeyword ||
      state == kOtherKeyword) {
    return Error(token_out);
  }
  parse_stack_.push_back(kOpenBracket);
  Emit(1, true, token_out);
  return JsKeywords::kOperator;
}

JsKeywords::Type JsTokenizer::ConsumeCloseBracket(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ(']', input_[0]);
  // Pop the most recent kOpenBracket (and everything above it) off the stack.
  while (true) {
    DCHECK(!parse_stack_.empty());
    const ParseState state = parse_stack_.back();
    if (state == kOpenBracket) {
      parse_stack_.pop_back();
      break;
    } else if (state == kStartOfInput ||
               state == kOpenBrace || state == kOpenParen ||
               state == kBlockKeyword || state == kBlockHeader) {
      return Error(token_out);
    } else {
      parse_stack_.pop_back();
    }
  }
  PushExpression();
  // Emit a token for the close bracket.
  Emit(1, true, token_out);
  return JsKeywords::kOperator;
}

JsKeywords::Type JsTokenizer::ConsumeOpenParen(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ('(', input_[0]);
  const ParseState state = parse_stack_.back();
  if (state == kJumpKeyword || state == kOtherKeyword) {
    return Error(token_out);
  }
  parse_stack_.push_back(kOpenParen);
  Emit(1, true, token_out);
  return JsKeywords::kOperator;
}

JsKeywords::Type JsTokenizer::ConsumeCloseParen(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ(')', input_[0]);
  // Pop the most recent kOpenParen (and everything above it) off the stack.
  while (true) {
    DCHECK(!parse_stack_.empty());
    const ParseState state = parse_stack_.back();
    if (state == kOpenParen) {
      parse_stack_.pop_back();
      break;
    } else if (state == kStartOfInput ||
               state == kOpenBrace || state == kOpenBracket ||
               state == kBlockKeyword || state == kBlockHeader) {
      return Error(token_out);
    } else {
      parse_stack_.pop_back();
    }
  }
  // If this is the closing paren of e.g. "if (...)", then we've just created a
  // kBlockHeader.  Otherwise, we've just created a kExpression.
  DCHECK(!parse_stack_.empty());
  if (parse_stack_.back() == kBlockKeyword) {
    parse_stack_.pop_back();
    PushBlockHeader();
  } else {
    PushExpression();
  }
  // Emit a token for the close parenthesis.
  Emit(1, true, token_out);
  return JsKeywords::kOperator;
}

JsKeywords::Type JsTokenizer::ConsumeBlockComment(StringPiece* token_out) {
  DCHECK_GE(input_.size(), 2u);
  DCHECK_EQ('/', input_[0]);
  DCHECK_EQ('*', input_[1]);
  const stringpiece_ssize_type index = input_.find("*/", 2);
  if (index == StringPiece::npos) {
    return Error(token_out);
  }
  Emit(index + 2, false, token_out);
  return JsKeywords::kComment;
}

JsKeywords::Type JsTokenizer::ConsumeLineComment(StringPiece* token_out) {
  Re2StringPiece unconsumed = StringPieceToRe2(input_);
  Re2StringPiece linebreak;
  if (!RE2::Consume(&unconsumed, patterns_->line_comment_pattern, &linebreak)) {
    // We only call ConsumeLineComment when we're sure we're looking at a line
    // comment, so this ought not happen even for pathalogical input.
    LOG(DFATAL) << "Failed to match line comment pattern: "
                << input_.substr(0, 50);
    return Error(token_out);
  }
  Emit(input_.size() - unconsumed.size() - linebreak.size(), false, token_out);
  return JsKeywords::kComment;
}

bool JsTokenizer::TryConsumeComment(
    JsKeywords::Type* type_out, StringPiece* token_out) {
  DCHECK(!input_.empty());
  if (input_.starts_with("/*")) {
    *type_out = ConsumeBlockComment(token_out);
    return true;
  }
  if (input_.starts_with("//") || input_.starts_with("<!--") ||
      (start_of_line_ && input_.starts_with("-->"))) {
    *type_out = ConsumeLineComment(token_out);
    return true;
  }
  return false;
}

JsKeywords::Type JsTokenizer::ConsumeColon(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ(':', input_[0]);
  while (true) {
    DCHECK(!parse_stack_.empty());
    switch (parse_stack_.back()) {
      // If we reach a kQuestionMark, this colon is part of a ternary
      // operator.  Remove the kQuestionMark and replace it with a kOperator.
      case kQuestionMark:
        parse_stack_.pop_back();
        PushOperator();
        Emit(1, true, token_out);
        return JsKeywords::kOperator;
      // If we reach the start of the statement without seeing a kQuestionMark,
      // this was a label.  No need to push any new parse state.
      case kStartOfInput:
      case kOpenBrace:
      case kBlockHeader:
        Emit(1, true, token_out);
        return JsKeywords::kOperator;
      // Skip past anything that could lie between the colon and the question
      // mark or start-of-statement.  This includes the kOtherKeyword parse
      // state for the sake of the "default" keyword.
      case kExpression:
      case kOtherKeyword:
        parse_stack_.pop_back();
        break;
      // Reaching any other parse state is an error.
      case kOperator:
      case kOpenBracket:
      case kOpenParen:
      case kBlockKeyword:
      case kReturnThrow:
      case kJumpKeyword:
        return Error(token_out);
      default:
        LOG(DFATAL) << "Unknown parse state: " << parse_stack_.back();
        return Error(token_out);
    }
  }
}

bool JsTokenizer::TryConsumeIdentifierOrKeyword(
    JsKeywords::Type* type_out, StringPiece* token_out) {
  DCHECK(!input_.empty());
  // First, check if we're looking at an identifier (or keyword); if not,
  // return immediately.
  Re2StringPiece unconsumed = StringPieceToRe2(input_);
  if (!RE2::Consume(&unconsumed, patterns_->identifier_pattern)) {
    return false;
  }
  // We have a match.  Determine which keyword it is, if any.
  const int index = input_.size() - unconsumed.size();
  JsKeywords::Flag flag_ignored;
  JsKeywords::Type type =
      JsKeywords::Lookup(input_.substr(0, index), &flag_ignored);
  switch (type) {
    // If the word isn't a keyword, then it's an identifier.  Also, these other
    // "keywords" are only reserved for future use in strict mode, and
    // otherwise are legal identifiers.  Since we don't detect strict mode
    // errors yet, just always allow them as identifiers.
    case JsKeywords::kNotAKeyword:
    case JsKeywords::kImplements:
    case JsKeywords::kInterface:
    case JsKeywords::kLet:
    case JsKeywords::kPackage:
    case JsKeywords::kPrivate:
    case JsKeywords::kProtected:
    case JsKeywords::kPublic:
    case JsKeywords::kStatic:
    case JsKeywords::kYield:
      type = JsKeywords::kIdentifier;
      // An identifier just after a kBlockKeyword is the name of a function
      // declaration; we just ignore it and leave the parse state as
      // kBlockKeyword.  Other identifiers are treated as kExpressions.
      if (parse_stack_.back() != kBlockKeyword) {
        PushExpression();
      }
      break;
    // These keywords are expressions.  A slash after one of these is division
    // (rather than a regex literal).
    case JsKeywords::kFalse:
    case JsKeywords::kNull:
    case JsKeywords::kThis:
    case JsKeywords::kTrue:
      PushExpression();
      break;
    // These keywords must be followed by something in parentheses.  A slash
    // immediately after one of these is invalid; a slash after the parentheses
    // is the start of a regex literal (rather than division).
    case JsKeywords::kCatch:
    case JsKeywords::kFor:
    case JsKeywords::kFunction:
    case JsKeywords::kIf:
    case JsKeywords::kSwitch:
    case JsKeywords::kWhile:
    case JsKeywords::kWith:
      parse_stack_.push_back(kBlockKeyword);
      break;
    // These keywords mark the start of a block.  A slash after one of these is
    // the start of a regex literal (rather than division); an open brace after
    // one of these is the start of a block (rather than an object literal).
    case JsKeywords::kDo:
    case JsKeywords::kElse:
    case JsKeywords::kFinally:
    case JsKeywords::kTry:
      PushBlockHeader();
      break;
    // These keywords act like operators (sort of).  A slash after one of these
    // marks the start of a regex literal (rather than division); an open brace
    // after one of these is the start of an object literal (rather than a
    // block).
    case JsKeywords::kCase:
    case JsKeywords::kDelete:
    case JsKeywords::kIn:
    case JsKeywords::kInstanceof:
    case JsKeywords::kNew:
    case JsKeywords::kTypeof:
    case JsKeywords::kVoid:
      PushOperator();
      break;
    // These two keywords are like prefix operators in their treatment of
    // slashes, but a linebreak after them always induces semicolon insertion.
    case JsKeywords::kReturn:
    case JsKeywords::kThrow:
      parse_stack_.push_back(kReturnThrow);
      break;
    // These keywords can't have a division operator or a regex literal after
    // them, so a slash after one of these is an error (not counting comments,
    // of course).  Moreover, a linebreak after them always induces semicolon
    // insertion.
    case JsKeywords::kBreak:
    case JsKeywords::kContinue:
    case JsKeywords::kDebugger:
      parse_stack_.push_back(kJumpKeyword);
      break;
    // These keywords also can't have a division operator or a regex literal
    // after them.  However, a linebreak after them never induces semicolon
    // insertion.
    case JsKeywords::kConst:
    case JsKeywords::kDefault:
    case JsKeywords::kVar:
      parse_stack_.push_back(kOtherKeyword);
      break;
    // These keywords are reserved and may not be used:
    case JsKeywords::kClass:
    case JsKeywords::kEnum:
    case JsKeywords::kExport:
    case JsKeywords::kExtends:
    case JsKeywords::kImport:
    case JsKeywords::kSuper:
      *type_out = Error(token_out);
      return true;
    default:
      LOG(DFATAL) << "Unknown keyword type: " << type;
      *type_out = Error(token_out);
      return true;
  }
  Emit(index, true, token_out);
  *type_out = type;
  return true;
}

JsKeywords::Type JsTokenizer::ConsumeNumber(StringPiece* token_out) {
  DCHECK(!input_.empty());
  Re2StringPiece unconsumed = StringPieceToRe2(input_);
  if (!RE2::Consume(&unconsumed, patterns_->numeric_literal_pattern)) {
    // We only call ConsumeNumber when we're sure we're looking at a numeric
    // literal, so this ought not happen even for pathalogical input.
    LOG(DFATAL) << "Failed to match number pattern: " << input_.substr(0, 50);
    return Error(token_out);
  }
  PushExpression();
  Emit(input_.size() - unconsumed.size(), true, token_out);
  return JsKeywords::kNumber;
}

JsKeywords::Type JsTokenizer::ConsumeOperator(StringPiece* token_out) {
  DCHECK(!input_.empty());
  Re2StringPiece unconsumed = StringPieceToRe2(input_);
  if (!RE2::Consume(&unconsumed, patterns_->operator_pattern)) {
    // Unrecognized character:
    return Error(token_out);
  }
  Emit(input_.size() - unconsumed.size(), true, token_out);
  const StringPiece token = *token_out;
  // Is this a postfix operator?  We treat those differently than prefix or
  // unary operators.
  DCHECK(!parse_stack_.empty());
  if ((token == "++" || token == "--") &&
      parse_stack_.back() == kExpression) {
    // Postfix operator; leave the parse state as kExpression.
  } else {
    // Prefix or binary operator; push it onto the stack.
    PushOperator();
  }
  return JsKeywords::kOperator;
}

JsKeywords::Type JsTokenizer::ConsumePeriod(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ('.', input_[0]);
  if (input_.size()  >= 2) {
    const int next = input_[1];
    if (next >= '0' && next <= '9') {
      return ConsumeNumber(token_out);
    }
  }
  PushOperator();
  Emit(1, true, token_out);
  return JsKeywords::kOperator;
}

JsKeywords::Type JsTokenizer::ConsumeQuestionMark(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ('?', input_[0]);
  DCHECK(!parse_stack_.empty());
  if (parse_stack_.back() != kExpression) {
    return Error(token_out);
  }
  parse_stack_.push_back(kQuestionMark);
  Emit(1, true, token_out);
  return JsKeywords::kOperator;
}

JsKeywords::Type JsTokenizer::ConsumeRegex(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ('/', input_[0]);
  Re2StringPiece unconsumed = StringPieceToRe2(input_);
  if (!RE2::Consume(&unconsumed, patterns_->regex_literal_pattern)) {
    // EOF or a linebreak in the regex will cause an error.
    return Error(token_out);
  }
  PushExpression();
  Emit(input_.size() - unconsumed.size(), true, token_out);
  return JsKeywords::kRegex;
}

JsKeywords::Type JsTokenizer::ConsumeSemicolon(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ(';', input_[0]);
  // Semicolons can appear either at the end of a statement, or within a
  // for-loop header.  So pop the parse state back to the previous open brace
  // (or start of input) for end-of-statement, or the previous open paren (in
  // which case we'd better be within a block header).
  while (true) {
    DCHECK(!parse_stack_.empty());
    const ParseState state = parse_stack_.back();
    if (state == kOpenBracket) {
      return Error(token_out);
    } else if (state == kOpenParen) {
      // Semicolon within parens is only okay if it's a for-loop header, so the
      // parse state below the kOpenParen had better be kBlockKeyword (for the
      // "for" keyword) or else this is a parse error.  (Since the top state is
      // currently kOpenParen, and the bottom state is always kStartOfInput, we
      // know that the parse stack has at least two entries right now).
      DCHECK_GE(parse_stack_.size(), 2u);
      if (parse_stack_[parse_stack_.size() - 2] != kBlockKeyword) {
        return Error(token_out);
      }
      break;
    } else if (state == kStartOfInput || state == kOpenBrace) {
      break;
    }
    parse_stack_.pop_back();
  }
  // Emit a token for the semicolon.
  Emit(1, true, token_out);
  return JsKeywords::kOperator;
}

JsKeywords::Type JsTokenizer::ConsumeSlash(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK_EQ('/', input_[0]);
  // If the slash is immediately followed by a slash or star, it's a comment,
  // no matter what the current parse state is.
  if (input_.size() >= 2) {
    const int next = input_[1];
    if (next == '/') {
      return ConsumeLineComment(token_out);
    } else if (next == '*') {
      return ConsumeBlockComment(token_out);
    }
  }
  // Otherwise, we have to consult the current parse state to decide if this
  // slash is a division operator or the start of a regex literal.
  DCHECK(!parse_stack_.empty());
  switch (parse_stack_.back()) {
    case kExpression:
      return ConsumeOperator(token_out);
    case kStartOfInput:
    case kOperator:
    case kQuestionMark:
    case kOpenBrace:
    case kOpenBracket:
    case kOpenParen:
    case kBlockHeader:
    case kReturnThrow:
      return ConsumeRegex(token_out);
    case kBlockKeyword:
    case kJumpKeyword:
    case kOtherKeyword:
      return Error(token_out);
    default:
      LOG(DFATAL) << "Unknown parse state: " << parse_stack_.back();
      return Error(token_out);
  }
}

JsKeywords::Type JsTokenizer::ConsumeString(StringPiece* token_out) {
  DCHECK(!input_.empty());
  DCHECK(input_[0] == '"' || input_[0] == '\'');
  Re2StringPiece unconsumed = StringPieceToRe2(input_);
  if (!RE2::Consume(&unconsumed, patterns_->string_literal_pattern) ||
      input_[input_.size() - unconsumed.size() - 1] != input_[0]) {
    // EOF or an unescaped linebreak in the string will cause an error.
    return Error(token_out);
  }
  PushExpression();
  Emit(input_.size() - unconsumed.size(), true, token_out);
  return JsKeywords::kStringLiteral;
}

bool JsTokenizer::TryConsumeWhitespace(
    bool allow_semicolon_insertion,
    JsKeywords::Type* type_out, StringPiece* token_out) {
  DCHECK(!input_.empty());
  // First, check if we're looking at whitespace; if not, return immediately.
  Re2StringPiece unconsumed = StringPieceToRe2(input_);
  Re2StringPiece linebreak;
  if (!RE2::Consume(&unconsumed, patterns_->whitespace_pattern, &linebreak)) {
    return false;
  }
  // Yep, this was whitespace; emit a token.
  Emit(input_.size() - unconsumed.size(), false, token_out);
  // Now we have to decide what kind of whitespace this was.  If it contained
  // no linebreaks, it's just regular whitespace.
  if (linebreak.empty()) {
    *type_out = JsKeywords::kWhitespace;
  } else {
    // Otherwise, we have to decide whether or not this linebreak will cause
    // semicolon insertion.
    start_of_line_ = true;
    if (allow_semicolon_insertion && TryInsertLinebreakSemicolon()) {
      *type_out = JsKeywords::kSemiInsert;
    } else {
      *type_out = JsKeywords::kLineSeparator;
    }
  }
  return true;
}

JsKeywords::Type JsTokenizer::Error(StringPiece* token_out) {
  error_ = true;
  *token_out = input_;
  input_.clear();
  return JsKeywords::kError;
}

void JsTokenizer::Emit(int num_chars, bool non_whitespace,
                       StringPiece* token_out) {
  DCHECK_GT(num_chars, 0);
  DCHECK_LE(static_cast<size_t>(num_chars), input_.size());
  const StringPiece token = input_.substr(0, num_chars);
  if (non_whitespace) {
    start_of_line_ = false;
  }
  *token_out = token;
  input_ = input_.substr(num_chars);
}

void JsTokenizer::PushBlockHeader() {
  // Push a kBlockHeader state onto the stack, but if there's already a
  // kBlockHeader on the stack (e.g. as in "else if (...)"), merge the two
  // together by simply leaving the stack alone.
  DCHECK(!parse_stack_.empty());
  if (parse_stack_.back() != kBlockHeader) {
    parse_stack_.push_back(kBlockHeader);
  }
}

void JsTokenizer::PushExpression() {
  // Push a kExpression state onto the stack, merging it with any kExpression or
  // kOperator states on top (e.g. so "a + b" -> "Expr Oper Expr" becomes "Expr"
  // and "foo(1)" -> "Expr ( Expr )" becomes "Expr Expr" becomes "Expr").
  DCHECK(!parse_stack_.empty());
  while (parse_stack_.back() == kExpression ||
         parse_stack_.back() == kOperator) {
    parse_stack_.pop_back();
    DCHECK(!parse_stack_.empty());
  }
  parse_stack_.push_back(kExpression);
}

void JsTokenizer::PushOperator() {
  // Push a kOperator state onto the stack, but if there's already a kOperator
  // on the stack (e.g. as in "x && !y"), merge the two together by simply
  // leaving the stack alone.
  DCHECK(!parse_stack_.empty());
  if (parse_stack_.back() != kOperator) {
    parse_stack_.push_back(kOperator);
  }
}

bool JsTokenizer::TryInsertLinebreakSemicolon() {
  // Determining whether semicolon insertion happens requires checking the next
  // non-whitespace/comment token, so skip past any comments and whitespace and
  // store them in the lookahead queue.  Note that whether or not the linebreak
  // we're considering in this method inserts a semicolon, the subsequent
  // whitespace we're about to skip past certainly won't.
  DCHECK(lookahead_queue_.empty());
  {
    JsKeywords::Type type;
    StringPiece token;
    while (!input_.empty() &&
           (TryConsumeComment(&type, &token) ||
            TryConsumeWhitespace(false, &type, &token))) {
      lookahead_queue_.push_back(std::make_pair(type, token));
    }
  }
  // Even if semicolon insertion would technically happen for the linebreak
  // here, we will pretend that it won't if we're about to hit a real
  // semicolon, or if the semicolon would be inserted anyway without the
  // linebreak.
  if (input_.empty() || input_[0] == ';' || input_[0] == '}') {
    return false;
  }
  // Whether semicolon insertion can happen depends on the current parse state.
  DCHECK(!parse_stack_.empty());
  switch (parse_stack_.back()) {
    case kStartOfInput:
    case kOpenBrace:
    case kOpenBracket:
    case kOpenParen:
    case kBlockKeyword:
    case kBlockHeader:
      // Semicolon insertion never happens in places where it would create an
      // empty statement.
      return false;
    case kExpression:
      // A statement can't end with an unclosed paren or bracket; in
      // particular, semicolons for a for-loop header are never inserted.
      for (std::vector<ParseState>::const_reverse_iterator iter =
               parse_stack_.rbegin(), end = parse_stack_.rend();
           iter != end; ++iter) {
        const ParseState state = *iter;
        if (state == kOpenParen || state == kOpenBracket) {
          return false;
        }
        if (state == kOpenBrace || state == kBlockHeader) {
          break;
        }
      }
      // Semicolon insertion will not happen after an expression if the next
      // token could continue the statement.
      {
        Re2StringPiece unconsumed = StringPieceToRe2(input_);
        if (RE2::Consume(&unconsumed, patterns_->line_continuation_pattern)) {
          return false;
        }
      }
      break;
    // Binary and prefix operators should not have semicolon insertion happen
    // after them.
    case kOperator:
    case kQuestionMark:
      return false;
    // Line continuations are never permitted after return, throw, break,
    // continue, or debugger keywords, so a semicolon is always inserted for
    // those.
    case kReturnThrow:
    case kJumpKeyword:
      break;
    // A statement cannot end after const, default, or var, so we never insert
    // a semicolon after those.
    case kOtherKeyword:
      return false;
    default:
      LOG(DFATAL) << "Unknown parse state: " << parse_stack_.back();
      break;
  }
  // We've decided at this point that semicolon insertion will happen, so
  // update the parse stack to end the current statement.
  while (true) {
    DCHECK(!parse_stack_.empty());
    const ParseState state = parse_stack_.back();
    if (state == kStartOfInput || state == kOpenBrace) {
      break;
    }
    parse_stack_.pop_back();
  }
  return true;
}

JsTokenizerPatterns::JsTokenizerPatterns()
    : identifier_pattern(kIdentifierRegex),
      line_comment_pattern(kLineCommentRegex),
      numeric_literal_pattern(kNumericLiteralPosixRegex, re2::posix_syntax),
      operator_pattern(kOperatorRegex),
      regex_literal_pattern(kRegexLiteralRegex),
      string_literal_pattern(kStringLiteralRegex),
      whitespace_pattern(kWhitespaceRegex),
      line_continuation_pattern(kLineContinuationRegex) {
  DCHECK(identifier_pattern.ok());
  DCHECK(numeric_literal_pattern.ok());
  DCHECK(operator_pattern.ok());
  DCHECK(regex_literal_pattern.ok());
  DCHECK(string_literal_pattern.ok());
  DCHECK(whitespace_pattern.ok());
  DCHECK(line_continuation_pattern.ok());
}

JsTokenizerPatterns::~JsTokenizerPatterns() {}

}  // namespace js

}  // namespace pagespeed
