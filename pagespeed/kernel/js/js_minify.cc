// Copyright 2010 Google Inc. All Rights Reserved.
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

#include "base/logging.h"
#include "pagespeed/kernel/base/source_map.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/js/js_keywords.h"
#include "pagespeed/kernel/js/js_tokenizer.h"

using pagespeed::JsKeywords;

namespace pagespeed {

namespace js {

namespace {

// TODO(mdsteele): Once we're confident in the new minifier, delete the
//   contents of this "legacy" namespace and just use the new implementation.
namespace legacy {

// Javascript's grammar has the appalling property that it cannot be lexed
// without also being parsed, due to its semicolon insertion rules and the
// ambiguity between regex literals and the division operator.  We don't want
// to build a full parser just for the sake of removing whitespace/comments, so
// this code uses some heuristics to try to guess the relevant parsing details.

const int kEOF = -1;  // represents the end of the input

// A token can either be a character (0-255) or one of these constants:
const int kStartToken = 256;  // the start of the input
const int kCCCommentToken = 257;  // a conditional compilation comment
const int kRegexToken = 258;  // a regular expression literal
const int kStringToken = 259;  // a string literal
// We have to differentiate between the keywords that can precede a regex
// (such as throw) and those that can't to ensure that we don't treat return or
// throw as a primary expression (which could mess up linebreak removal or
// differentiating between division and regexes).
const int kNameNumberToken = 260;  // name, number, keyword
const int kKeywordCanPrecedeRegExToken = 261;
// The ++ and -- tokens affect the semicolon insertion rules in Javascript, so
// we need to track them carefully in order to get whitespace removal right.
// Other multicharacter operators (such as += or ===) can just be treated as
// multiple single character operators, and it'll all come out okay.
const int kPlusPlusToken = 262;  // a ++ token
const int kMinusMinusToken = 263;  // a -- token

// Is this a character that can appear in identifiers?
int IsIdentifierChar(int c) {
  // Note that backslashes can appear in identifiers due to unicode escape
  // sequences (e.g. \u03c0).
  return ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
          (c >= 'A' && c <= 'Z') || c == '_' || c == '$' || c == '\\' ||
          c >= 127);
}

// Return true if the given token cannot ever be the first or last token of a
// statement; that is, a semicolon will never be inserted next to this token.
// This function is used to help us with linebreak suppression.
bool CannotBeginOrEndStatement(int token) {
  switch (token) {
    case kStartToken:
    case '=':
    case '<':
    case '>':
    case ';':
    case ':':
    case '?':
    case '|':
    case '^':
    case '&':
    case '*':
    case '/':
    case '%':
    case ',':
    case '.':
      return true;
    default:
      return false;
  }
}

// Return true if the given token signifies that we are at the end of a primary
// expression (e.g. 42, or foo[0], or func()).  This function is used to help
// us with linebreak suppression and to tell the difference between regex
// literals and division operators.
bool EndsPrimaryExpression(int token) {
  switch (token) {
    case kNameNumberToken:
    case kRegexToken:
    case kStringToken:
    case ')':
    case ']':
      return true;
    default:
      return false;
  }
}

// Return true if we can safely remove a linebreak from between the given two
// tokens (that is, if we're sure that the linebreak will not result in
// semicolon insertion), or false if we're not sure we can remove it safely.
bool CanSuppressLinebreak(int prev_token, int next_token) {
  // We can suppress the linebreak if the previous token can't possibly be
  // the end of a statement.
  if (CannotBeginOrEndStatement(prev_token) ||
      prev_token == '(' || prev_token == '[' || prev_token == '{' ||
      prev_token == '!' || prev_token == '~' ||
      prev_token == '+' || prev_token == '-') {
    return true;
  }
  // We can suppress the linebreak if the next token can't possibly be the
  // beginning of a statement.
  if (CannotBeginOrEndStatement(next_token) ||
      next_token == ')' || next_token == ']' ||
      next_token == '}') {
    return true;
  }
  // We can suppress the linebreak if one-token lookahead tells us that we
  // could keep parsing without inserting a semicolon.
  if (EndsPrimaryExpression(prev_token) &&
      (next_token == '(' || next_token == '[' ||
       next_token == '+' || next_token == '-')) {
    return true;
  }
  // Otherwise, we should leave the linebreak there, to be safe.
  return false;
}

class StringConsumer {
 public:
  explicit StringConsumer(GoogleString* output) : output_(output) {}
  void push_back(char character) {
    output_->push_back(character);
  }
  void append(const StringPiece& str) {
    str.AppendToString(output_);
  }
  GoogleString* output_;
};

class SizeConsumer {
 public:
  explicit SizeConsumer(GoogleString* ignored) : size_(0) {}
  void push_back(char character) {
    ++size_;
  }
  void append(const StringPiece& str) {
    size_ += str.size();
  }
  int size_;
};

template<typename OutputConsumer>
class Minifier {
 public:
  Minifier(const StringPiece& input, GoogleString* output);
  ~Minifier() {}

  // Return a pointer to an OutputConsumer instance if minification was
  // successful, NULL otherwise.
  OutputConsumer* GetOutput();

  // Enable collapsing strings while minifiying. Should call after constructor,
  // and before calling GetOutput().
  void EnableStringCollapse() { collapse_string_ = true; }

 private:
  int Peek();
  void ChangeToken(int next_token);
  void InsertSpaceIfNeeded();
  void ConsumeBlockComment();
  void ConsumeLineComment();
  void ConsumeNameOrNumber();
  void ConsumeRegex();
  void ConsumeString();
  void Minify();

  // Returns the input size as an int to avoid signed/unsigned comparison
  // warnings.
  int input_size() const { return input_.size(); }

  const StringPiece input_;
  int index_;
  OutputConsumer output_;
  JsWhitespace whitespace_;  // whitespace since the previous token
  int prev_token_;
  bool error_;
  bool collapse_string_;
};

template<typename OutputConsumer>
Minifier<OutputConsumer>::Minifier(const StringPiece& input,
                                   GoogleString* output)
  : input_(input),
    index_(0),
    output_(output),
    whitespace_(kNoWhitespace),
    prev_token_(kStartToken),
    error_(false),
    collapse_string_(false) {}

// Return the next character after index_, or kEOF if there aren't any more.
template<typename OutputConsumer>
int Minifier<OutputConsumer>::Peek() {
  return (index_ + 1 < input_size() ?
          static_cast<int>(input_[index_ + 1]) : kEOF);
}

// Switch to a new prev_token, and insert a newline if necessary.  Call this
// right before appending a token onto the output.
template<typename OutputConsumer>
void Minifier<OutputConsumer>::ChangeToken(int next_token) {
  // If there've been any linebreaks since the previous token, we may need to
  // insert a linebreak here to avoid running afoul of semicolon insertion
  // (that is, the code may be relying on semicolon insertion here, and
  // removing the linebreak would break it).
  if (whitespace_ == kLinebreak &&
      !CanSuppressLinebreak(prev_token_, next_token)) {
    output_.push_back('\n');
  }
  whitespace_ = kNoWhitespace;
  prev_token_ = next_token;
}

// If there's been any whitespace since the previous token, insert some
// whitespace now to separate the previous token from the next token.
template<typename OutputConsumer>
void Minifier<OutputConsumer>::InsertSpaceIfNeeded() {
  switch (whitespace_) {
    case kSpace:
      output_.push_back(' ');
      break;
    case kLinebreak:
      output_.push_back('\n');
      break;
    default:
      break;
  }
  whitespace_ = kNoWhitespace;
}

template<typename OutputConsumer>
void Minifier<OutputConsumer>::ConsumeBlockComment() {
  DCHECK(index_ + 1 < input_size());
  DCHECK_EQ(input_[index_], '/');
  DCHECK_EQ(input_[index_ + 1], '*');
  const int begin = index_;
  index_ += 2;
  // We want to remove comments, but we need to preserve IE conditional
  // compilation comments to avoid breaking scripts that rely on them.
  // See http://code.google.com/p/page-speed/issues/detail?id=198
  const bool may_be_ccc = (index_ < input_size() && input_[index_] == '@');
  while (index_ < input_size()) {
    if (input_[index_] == '*' && Peek() == '/') {
      index_ += 2;
      if (may_be_ccc && input_[index_ - 3] == '@') {
        ChangeToken(kCCCommentToken);
        output_.append(input_.substr(begin, index_ - begin));
      } else if (whitespace_ == kNoWhitespace) {
        whitespace_ = kSpace;
      }
      return;
    }
    ++index_;
  }
  // If we reached EOF without the comment being closed, then this is an error.
  error_ = true;
}

template<typename OutputConsumer>
void Minifier<OutputConsumer>::ConsumeLineComment() {
  while (index_ < input_size() && input_[index_] != '\n' &&
         input_[index_] != '\r') {
    ++index_;
  }
  whitespace_ = kLinebreak;
}

// Consume a keyword, name, or number.
template<typename OutputConsumer>
void Minifier<OutputConsumer>::ConsumeNameOrNumber() {
  if (prev_token_ == kNameNumberToken ||
      prev_token_ == kKeywordCanPrecedeRegExToken ||
      prev_token_ == kRegexToken) {
    InsertSpaceIfNeeded();
  }
  GoogleString token;
  while (index_ < input_size() && IsIdentifierChar(input_[index_])) {
    token.push_back(input_[index_]);
    ++index_;
  }
  // For the most part, we can just treat keywords the same as identifiers, and
  // we'll still minify correctly. However, some keywords (like return and
  // throw) in particular must be treated differently, to help us tell the
  // difference between regex literals and division operators:
  //   return/ x /g;  // this returns a regex literal; preserve whitespace
  //   reTurn/ x /g;  // this performs two divisions; remove whitespace
  ChangeToken(JsKeywords::CanKeywordPrecedeRegEx(token)
                  ? kKeywordCanPrecedeRegExToken
                  : kNameNumberToken);
  output_.append(token);
}

template<typename OutputConsumer>
void Minifier<OutputConsumer>::ConsumeRegex() {
  DCHECK(index_ < input_size());
  DCHECK_EQ(input_[index_], '/');
  const int begin = index_;
  ++index_;
  bool within_brackets = false;
  while (index_ < input_size()) {
    const char ch = input_[index_];
    ++index_;
    if (ch == '\\') {
      // If we see a backslash, don't check the next character (this is mainly
      // relevant if the next character is a slash that would otherwise close
      // the regex literal, or a closing bracket when we are within brackets).
      ++index_;
    } else if (ch == '/') {
      // Slashes within brackets are implicitly escaped.
      if (!within_brackets) {
        // Don't accidentally create a line comment.
        if (prev_token_ == '/') {
          InsertSpaceIfNeeded();
        }
        ChangeToken(kRegexToken);
        output_.append(input_.substr(begin, index_ - begin));
        return;
      }
    } else if (ch == '[') {
      // Regex brackets don't nest, so we don't need a stack -- just a bool.
      within_brackets = true;
    } else if (ch == ']') {
      within_brackets = false;
    } else if (ch == '\n') {
      break;  // error
    }
  }
  // If we reached newline or EOF without the regex being closed, then this is
  // an error.
  error_ = true;
}

template<typename OutputConsumer>
void Minifier<OutputConsumer>::ConsumeString() {
  DCHECK(index_ < input_size());
  const int begin = index_;
  const char quote = input_[begin];
  DCHECK(quote == '"' || quote == '\'');
  ++index_;
  while (index_ < input_size()) {
    const char ch = input_[index_];
    ++index_;
    if (ch == '\\') {
      ++index_;
    } else {
      if (ch == quote) {
        ChangeToken(kStringToken);
        if (collapse_string_) {
          output_.push_back(quote);
          output_.push_back(quote);
        } else {
          output_.append(input_.substr(begin, index_ - begin));
        }
        return;
      }
    }
  }
  // If we reached EOF without the string being closed, then this is an error.
  error_ = true;
}

template<typename OutputConsumer>
void Minifier<OutputConsumer>::Minify() {
  while (index_ < input_size() && !error_) {
    const char ch = input_[index_];
    // Track whitespace since the previous token.  kNoWhitespace means no
    // whitespace; kLinebreak means there's been at least one linebreak; kSpace
    // means there's been spaces/tabs, but no linebreaks.
    if (ch == '\n' || ch == '\r') {
      whitespace_ = kLinebreak;
      ++index_;
    } else if (ch == ' ' || ch == '\t') {
      if (whitespace_ == kNoWhitespace) {
        whitespace_ = kSpace;
      }
      ++index_;
    } else if (ch == '\'' || ch == '"') {
      // Strings
      ConsumeString();
    } else if (ch == '/') {
      // A slash could herald a line comment, a block comment, a regex literal,
      // or a mere division operator; we need to figure out which it is.
      // Differentiating between division and regexes is mostly impossible
      // without parsing, so we do our best based on the previous token.

      const int next = Peek();
      if (next == '/') {
        ConsumeLineComment();
      } else if (next == '*') {
        ConsumeBlockComment();
      } else if (EndsPrimaryExpression(prev_token_)) {
        // If the slash is following a primary expression (like a literal, or
        // (...), or foo[0]), then it's definitely a division operator.  These
        // are previous tokens for which (I think) we can be sure that we're
        // following a primary expression.
        ChangeToken('/');
        output_.push_back(ch);
        ++index_;
      } else {
        // If we can't be sure it's division, then we must assume it's a regex
        // so that we don't remove whitespace that we shouldn't.  There are
        // cases that we'll get wrong, but it's hard to do better without
        // parsing.
        ConsumeRegex();
      }
    } else if (IsIdentifierChar(ch)) {
      // Identifiers, keywords, and numeric literals:
      ConsumeNameOrNumber();
    } else if (ch == '<' && input_.substr(index_).starts_with("<!--")) {
      // Treat <!-- as a line comment.  Note that the substr() here is very
      // efficient because input_ is a StringPiece, not a string.
      ConsumeLineComment();
    } else if (ch == '-' &&
               (whitespace_ == kLinebreak || prev_token_ == kStartToken) &&
               input_.substr(index_).starts_with("-->")) {
      // Treat --> as a line comment if it's at the start of a line.
      ConsumeLineComment();
    } else if (ch == '+' && Peek() == '+') {
      // Treat ++ differently than two +'s.  It has different whitespace rules:
      //   - A statement cannot ever end with +, but it can end with ++.  Thus,
      //     a linebreak after + can always be removed (no semicolon will be
      //     inserted), but a linebreak after ++ generally cannot.
      //   - A + at the start of a line can continue the previous line, but a ++
      //     cannot (a linebreak is _not_ permitted between i and ++ in an i++
      //     statement).  Thus, a linebreak just before a + can be removed in
      //     certain cases (if we can decide that a semicolon would not be
      //     inserted), but a linebreak just before a ++ never can.

      // Careful to leave whitespace so as not to create a +++ or ++++, which
      // can be ambiguous.
      if (prev_token_ == '+' || prev_token_ == kPlusPlusToken) {
        InsertSpaceIfNeeded();
      }
      ChangeToken(kPlusPlusToken);
      output_.append("++");
      index_ += 2;
    } else if (ch == '-' && Peek() == '-') {
      // Treat -- differently than two -'s.  It has different whitespace rules,
      // analogous to those of ++ (see above).

      // Careful to leave whitespace so as not to create a --- or ----, which
      // can be ambiguous.  Also careful of !'s, since we don't want to
      // accidentally create an SGML line comment.
      if (prev_token_ == '-' || prev_token_ == kMinusMinusToken ||
          prev_token_ == '!') {
        InsertSpaceIfNeeded();
      }
      ChangeToken(kMinusMinusToken);
      output_.append("--");
      index_ += 2;
    } else {
      // Copy other characters over verbatim, but make sure not to join two +
      // tokens into ++ or two - tokens into --, or to join ++ and + into +++
      // or -- and - into ---, or to minify the sequence of tokens < ! - - into
      // an SGML line comment.
      if ((prev_token_ == ch && (ch == '+' || ch == '-')) ||
          (prev_token_ == kPlusPlusToken && ch == '+') ||
          (prev_token_ == kMinusMinusToken && ch == '-') ||
          (prev_token_ == '<' && ch == '!') ||
          (prev_token_ == '!' && ch == '-')) {
        InsertSpaceIfNeeded();
      }
      ChangeToken(ch);
      output_.push_back(ch);
      ++index_;
    }
  }
}

template<typename OutputConsumer>
OutputConsumer* Minifier<OutputConsumer>::GetOutput() {
  Minify();
  if (!error_) {
    return &output_;
  }
  return NULL;
}

bool MinifyJs(const StringPiece& input, GoogleString* out) {
  Minifier<StringConsumer> minifier(input, out);
  return (minifier.GetOutput() != NULL);
}

bool GetMinifiedJsSize(const StringPiece& input, int* minimized_size) {
  Minifier<SizeConsumer> minifier(input, NULL);
  SizeConsumer* output = minifier.GetOutput();
  if (output) {
    *minimized_size = output->size_;
    return true;
  } else {
    return false;
  }
}

bool MinifyJsAndCollapseStrings(const StringPiece& input,
                               GoogleString* out) {
  Minifier<StringConsumer> minifier(input, out);
  minifier.EnableStringCollapse();
  return (minifier.GetOutput() != NULL);
}

bool GetMinifiedStringCollapsedJsSize(const StringPiece& input,
                                      int* minimized_size) {
  Minifier<SizeConsumer> minifier(input, NULL);
  minifier.EnableStringCollapse();
  SizeConsumer* output = minifier.GetOutput();
  if (output) {
    *minimized_size = output->size_;
    return true;
  } else {
    return false;
  }
}

}  // namespace legacy

bool IsNameNumberOrKeyword(JsKeywords::Type type) {
  switch (type) {
    case JsKeywords::kComment:
    case JsKeywords::kWhitespace:
    case JsKeywords::kLineSeparator:
    case JsKeywords::kSemiInsert:
    case JsKeywords::kRegex:
    case JsKeywords::kStringLiteral:
    case JsKeywords::kOperator:
    case JsKeywords::kEndOfInput:
    case JsKeywords::kError:
      return false;
    default:
      return true;
  }
}

// Updates *line and *col numbers based on the next incremental chunk of text.
// Note: This only works correctly for ASCII text. If text contains multi-byte
// UTF-8 chars, our updates will be incorrect.
void UpdateLineAndCol(StringPiece text, int* line, int* col) {
  for (int i = 0, n = text.size(); i < n; ++i) {
    if (text[i] == '\n') {
      // TODO(sligocki): We should allow all Unicode newline chars.
      *line += 1;
      *col = 0;
    } else {
      // TODO(sligocki): Count number of Unicode chars, not number of bytes.
      *col += 1;
    }
  }
}

bool ShouldRecordStep(
    const net_instaweb::source_map::MappingVector& mapping,
    const net_instaweb::source_map::Mapping& next) {
  // Should record first mapping.
  if (mapping.empty()) {
    return true;
  }

  const net_instaweb::source_map::Mapping& prev = mapping.back();
  if (next.gen_line == prev.gen_line) {
    // Should record iff different number of newlines or different num of cols.
    return (next.src_line != prev.src_line ||
            next.gen_col - prev.gen_col != next.src_col - prev.src_col);
  }

  // If line changes, we should record it.
  return true;
}

}  // namespace

JsMinifyingTokenizer::JsMinifyingTokenizer(
    const JsTokenizerPatterns* patterns, StringPiece input)
    : tokenizer_(patterns, input), whitespace_(kNoWhitespace),
      prev_type_(JsKeywords::kEndOfInput), prev_token_(),
      next_type_(JsKeywords::kEndOfInput), next_token_(),
      mappings_(NULL) {}

JsMinifyingTokenizer::JsMinifyingTokenizer(
    const JsTokenizerPatterns* patterns, StringPiece input,
    net_instaweb::source_map::MappingVector* mappings)
    : tokenizer_(patterns, input), whitespace_(kNoWhitespace),
      prev_type_(JsKeywords::kEndOfInput), prev_token_(),
      next_type_(JsKeywords::kEndOfInput), next_token_(),
      mappings_(mappings),
      current_position_(0, 0, 0, 0, 0), next_position_(0, 0, 0, 0, 0) {}

JsMinifyingTokenizer::~JsMinifyingTokenizer() {}

JsKeywords::Type JsMinifyingTokenizer::NextToken(StringPiece* token_out) {
  net_instaweb::source_map::Mapping token_out_position;
  const JsKeywords::Type type = NextTokenHelper(token_out, &token_out_position);
  if (mappings_ != NULL && type != JsKeywords::kEndOfInput &&
      ShouldRecordStep(*mappings_, token_out_position)) {
    mappings_->push_back(token_out_position);
  }
  // Update generated file line and col # with the output token.
  // Note: We use a helper function to avoid having to add this before every
  // return in NextTokenHelper.
  UpdateLineAndCol(*token_out, &current_position_.gen_line,
                   &current_position_.gen_col);
  return type;
}

JsKeywords::Type JsMinifyingTokenizer::NextTokenHelper(
    StringPiece* token_out, net_instaweb::source_map::Mapping* position_out) {
  if (next_type_ != JsKeywords::kEndOfInput) {
    prev_type_ = next_type_;
    prev_token_ = next_token_;
    *token_out = next_token_;
    *position_out = next_position_;
    // next_position_.gen_line and .gen_col are out of date because they were
    // computed in the previous call to NextTokenHelper().
    position_out->gen_line = current_position_.gen_line;
    position_out->gen_col = current_position_.gen_col;

    next_type_ = JsKeywords::kEndOfInput;
    next_token_.clear();
    return prev_type_;
  }
  net_instaweb::source_map::Mapping first_position = current_position_;
  while (true) {
    StringPiece token;
    const JsKeywords::Type type = tokenizer_.NextToken(&token);
    // Position of start of token
    net_instaweb::source_map::Mapping token_position = current_position_;
    // Update source file line and col # with the consumed input token.
    UpdateLineAndCol(token, &current_position_.src_line,
                     &current_position_.src_col);
    if (type == JsKeywords::kWhitespace) {
      if (whitespace_ == kNoWhitespace) {
        whitespace_ = kSpace;
      }
    } else if (type == JsKeywords::kLineSeparator) {
      whitespace_ = kLinebreak;
    } else if (type == JsKeywords::kSemiInsert) {
      whitespace_ = kNoWhitespace;
      prev_type_ = type;
      prev_token_ = "\n";
      *token_out = prev_token_;
      *position_out = first_position;  // Beginning of whitespace/comments.
      return type;
    } else if (type == JsKeywords::kComment) {
      // Emit comments that look like they might be IE conditional compilation
      // comments; treat all other comments as whitespace.
      // TODO(mdsteele): We should retain copyrights by default, and/or retain
      //   all comments matching a user-specified pattern.  It might also be
      //   nice to make retaining of IE conditional compilation comments
      //   optional, so we can turn it off for non-IE browsers.
      if (token.size() >= 6 && token.starts_with("/*@") &&
          token.ends_with("@*/")) {
        *token_out = token;
        *position_out = first_position;  // Beginning of whitespace/comments.
        return type;
      } else if (whitespace_ == kNoWhitespace) {
        whitespace_ = kSpace;
      }
    } else {
      const JsWhitespace whitespace = whitespace_;
      whitespace_ = kNoWhitespace;
      if (whitespace != kNoWhitespace &&
          WhitespaceNeededBefore(type, token)) {
        next_type_ = type;
        next_token_ = token;
        next_position_ = token_position;
        *position_out = first_position;  // Beginning of whitespace/comments.
        if (whitespace == kLinebreak) {
          *token_out = "\n";
          return JsKeywords::kLineSeparator;
        } else {
          *token_out = " ";
          return JsKeywords::kWhitespace;
        }
      }
      prev_type_ = type;
      prev_token_ = token;
      *token_out = token;
      *position_out = token_position;
      return type;
    }
  }
}

bool JsMinifyingTokenizer::WhitespaceNeededBefore(
    JsKeywords::Type type, StringPiece token) {
  // Whitespace is needed 1) to separate words and numbers, 2) to prevent from
  // glomming a period onto the end of numeric literal that will absorb it as a
  // decimal point, and 3) to prevent us from joining operators together to
  // form line comments or other operators.
  if (IsNameNumberOrKeyword(type)) {
    return (IsNameNumberOrKeyword(prev_type_) ||
            prev_type_ == JsKeywords::kRegex);
  } else if (token == ".") {
    // To avoid merging tokens, we can't append a period to the end of a number
    // literal that...
    return (prev_type_ == JsKeywords::kNumber &&
            // ...doesn't already have a decimal point or exponent, and...
            prev_token_.find_first_of(".eE") == StringPiece::npos &&
            // ...either doesn't start with a zero digit, or...
            (!prev_token_.starts_with("0") ||
             // ...does start with a zero digit, but is neither hex nor octal.
             (prev_token_.find_first_of("xX") == StringPiece::npos &&
              prev_token_.find_first_of("89") != StringPiece::npos)));
  } else if (prev_token_.ends_with("/")) {
    return token.starts_with("/");
  } else if (prev_token_.ends_with("+")) {
    return token.starts_with("+");
  } else if (prev_token_.ends_with("<")) {
    return token.starts_with("!");
  } else if (prev_token_.ends_with("!") ||
             prev_token_.ends_with("-")) {
    return token.starts_with("-");
  }
  return false;
}

bool MinifyUtf8Js(const JsTokenizerPatterns* patterns,
                  StringPiece input, GoogleString* output) {
  return MinifyUtf8JsWithSourceMap(patterns, input, output, NULL);
}

bool MinifyUtf8JsWithSourceMap(
    const JsTokenizerPatterns* patterns,
    StringPiece input, GoogleString* output,
    net_instaweb::source_map::MappingVector* mappings) {
  JsMinifyingTokenizer tokenizer(patterns, input, mappings);
  while (true) {
    StringPiece token;
    switch (tokenizer.NextToken(&token)) {
      case JsKeywords::kEndOfInput:
        DCHECK(token.empty());
        DCHECK(!tokenizer.has_error());
        return true;
      case JsKeywords::kError:
        DCHECK(tokenizer.has_error());
        token.AppendToString(output);
        return false;
      default:
        token.AppendToString(output);
        break;
    }
  }
}

bool MinifyJs(const StringPiece& input, GoogleString* out) {
  return legacy::MinifyJs(input, out);
}

bool GetMinifiedJsSize(const StringPiece& input, int* minimized_size) {
  return legacy::GetMinifiedJsSize(input, minimized_size);
}

bool MinifyJsAndCollapseStrings(const StringPiece& input,
                               GoogleString* out) {
  return legacy::MinifyJsAndCollapseStrings(input, out);
}

bool GetMinifiedStringCollapsedJsSize(const StringPiece& input,
                                      int* minimized_size) {
  return legacy::GetMinifiedStringCollapsedJsSize(input, minimized_size);
}

}  // namespace js

}  // namespace pagespeed
