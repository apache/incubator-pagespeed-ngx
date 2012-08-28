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

#include <ctype.h>  // isascii

#include <algorithm>  // std::min
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "strings/memutil.h"
#include "strings/strutil.h"
#include "third_party/utf/utf.h"
#include "util/gtl/stl_util.h"
#include "util/utf8/public/unicodetext.h"
#include "util/utf8/public/unilib.h"
#include "webutil/css/string.h"
#include "webutil/css/string_util.h"
#include "webutil/css/util.h"
#include "webutil/css/value.h"


namespace Css {

const uint64 Parser::kNoError;
const uint64 Parser::kUtf8Error;
const uint64 Parser::kDeclarationError;
const uint64 Parser::kSelectorError;
const uint64 Parser::kFunctionError;
const uint64 Parser::kMediaError;
const uint64 Parser::kCounterError;
const uint64 Parser::kHtmlCommentError;
const uint64 Parser::kValueError;
const uint64 Parser::kRulesetError;
const uint64 Parser::kSkippedTokenError;
const uint64 Parser::kCharsetError;
const uint64 Parser::kBlockError;
const uint64 Parser::kNumberError;
const uint64 Parser::kImportError;
const uint64 Parser::kAtRuleError;

const int Parser::kMaxErrorsRemembered;

// Using isascii with signed chars is unfortunately undefined.
static inline bool IsAscii(char c) {
  return isascii(static_cast<unsigned char>(c));
}

class Tracer {  // in opt mode, do nothing.
 public:
  Tracer(const char* name, const char** in) { }
};


// ****************
// constructors
// ****************

Parser::Parser(const char* utf8text, const char* textend)
    : begin_(utf8text),
      in_(begin_),
      end_(textend),
      quirks_mode_(true),
      preservation_mode_(false),
      errors_seen_mask_(kNoError),
      unparseable_sections_seen_mask_(kNoError) {
}

Parser::Parser(const char* utf8text)
    : begin_(utf8text),
      in_(begin_),
      end_(utf8text + strlen(utf8text)),
      quirks_mode_(true),
      preservation_mode_(false),
      errors_seen_mask_(kNoError),
      unparseable_sections_seen_mask_(kNoError) {
}

Parser::Parser(StringPiece s)
    : begin_(s.begin()),
      in_(begin_),
      end_(s.end()),
      quirks_mode_(true),
      preservation_mode_(false),
      errors_seen_mask_(kNoError),
      unparseable_sections_seen_mask_(kNoError) {
}

int Parser::ErrorNumber(uint64 error_flag) {
  for (int i = 0; i < 64; ++i) {
    if (error_flag & (1ULL << i)) {
      return i;
    }
  }
  LOG(DFATAL) << "Invalid error flag.";
  return -1;
}

const int Parser::kErrorContext = 20;
void Parser::ReportParsingError(uint64 error_flag,
                                const StringPiece& message) {
  errors_seen_mask_ |= error_flag;
  // Make sure we don't print outside of the range in_ begin_ to end_.
  const char* context_begin = in_ - std::min(static_cast<int64>(kErrorContext),
                                             static_cast<int64>(in_ - begin_));
  const char* context_end = in_ + std::min(static_cast<int64>(kErrorContext),
                                           static_cast<int64>(end_ - in_));
  string context(context_begin, context_end - context_begin);
  string full_message = StringPrintf(
      "%s at byte %d \"...%s...\"",
      message.as_string().c_str(), CurrentOffset(), context.c_str());
  VLOG(1) << full_message;
  if (errors_seen_.size() < kMaxErrorsRemembered) {
    ErrorInfo info = {ErrorNumber(error_flag), CurrentOffset(), full_message};
    errors_seen_.push_back(info);
  }
}

// ****************
// Helper functions
// ****************

// is c a space?  Only the characters "space" (Unicode code 32), "tab"
// (9), "line feed" (10), "carriage return" (13), and "form feed" (12)
// can occur in whitespace. Other space-like characters, such as
// "em-space" (8195) and "ideographic space" (12288), are never part
// of whitespace.
// http://www.w3.org/TR/REC-CSS2/syndata.html#whitespace
static bool IsSpace(char c) {
  switch (c) {
    case ' ': case '\t': case '\r': case '\n': case '\f':
      return true;
    default:
      return false;
  }
}

// If the character c is a hex digit, DeHex returns the number it
// represents ('0' => 0, 'A' => 10, 'F' => 15).  Otherwise, DeHex
// returns -1.
static int DeHex(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'F') {
    return (c - 'A') + 10;
  } else if (c >= 'a' && c <= 'f') {
    return (c - 'a') + 10;
  } else {
    return -1;
  }
}

// ****************
// Recursive-descent functions.
//
// The best documentation for these is in cssparser.h.
//
// ****************

// consume whitespace and comments.
void Parser::SkipSpace() {
  Tracer trace(__func__, &in_);
  while (in_ < end_) {
    if (IsSpace(*in_))
      in_++;
    else if (in_ + 1 < end_ && in_[0] == '/' && in_[1] == '*')
      SkipComment();
    else
      return;
  }
}

// consume comment /* aoeuaoe */
void Parser::SkipComment() {
  DCHECK(in_ + 2 <= end_ && in_[0] == '/' && in_[1] == '*');
  in_ += 2;  // skip the /*
  while (in_ + 1 < end_) {
    if (in_[0] == '*' && in_[1] == '/') {
      in_ += 2;
      return;
    } else {
      in_++;
    }
  }
  in_ = end_;
}

// skips until delim is seen or end-of-stream. returns if delim is actually
// seen.
bool Parser::SkipPastDelimiter(char delim) {
  SkipSpace();
  while (in_ < end_ && *in_ != delim) {
    ++in_;
    SkipSpace();
  }

  if (Done()) return false;
  ++in_;
  return true;
}

// returns true if there might be a token to read
bool Parser::SkipToNextAny() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  while (in_ < end_) {
    switch (*in_) {
      case '{':
        ReportParsingError(kSkippedTokenError,
                           "Ignoring block between tokens.");
        SkipBlock();  // ignore
        break;
      case '@':
        ReportParsingError(kSkippedTokenError,
                           "Ignoring @ident between tokens.");
        in_++;
        // TODO(sligocki): Should we just skip the at-keyword, or an entire
        // at-rule with SkipToAtRuleEnd(). This affects how a declatation like:
        //   foo: #000 @ident url(foo.mks) { block { rep } } end
        // would be parsed. Specifically, should the url() be ignored?
        ParseIdent();  // ignore
        break;
      case ';': case '}':
      case '!':
        return false;
      default:
        return true;
    }
    SkipSpace();
  }
  return false;
}

// In CSS2, identifiers (including element names, classes, and IDs in
// selectors) can contain only the characters [A-Za-z0-9] and ISO
// 10646 characters 161 and higher, plus the hyphen (-); they cannot
// start with a hyphen or a digit. They can also contain escaped
// characters and any ISO 10646 character as a numeric code (see next
// item). For instance, the identifier "B&W?" may be written as
// "B\&W\?" or "B\26 W\3F".
//
// We're a little more forgiving than the standard and permit hyphens
// and digits to start identifiers.
//
// FIXME(yian): actually, IE is more forgiving than Firefox in using a class
// selector starting with digits.
//
// http://www.w3.org/TR/REC-CSS2/syndata.html#value-def-identifier
static bool StartsIdent(char c) {
  return ((c >= 'A' && c <= 'Z')
          || (c >= 'a' && c <= 'z')
          || (c >= '0' && c <= '9')
          || c == '-' || c == '_'
          || !IsAscii(c));
}

UnicodeText Parser::ParseIdent(const StringPiece& allowed_chars) {
  Tracer trace(__func__, &in_);
  UnicodeText s;
  while (in_ < end_) {
    if ((*in_ >= 'A' && *in_ <= 'Z')
        || (*in_ >= 'a' && *in_ <= 'z')
        || (*in_ >= '0' && *in_ <= '9')
        || *in_ == '-' || *in_ == '_'
        || allowed_chars.find(*in_) != allowed_chars.npos) {
      s.push_back(*in_);
      in_++;
    } else if (!IsAscii(*in_)) {
      Rune rune;
      int len = charntorune(&rune, in_, end_-in_);
      if (len && rune != Runeerror) {
        if (rune >= 161) {
          s.push_back(rune);
          in_ += len;
        } else {  // characters 128-160 can't be in identifiers.
          return s;
        }
      } else {  // Encoding error.  Be a little forgiving.
        ReportParsingError(kUtf8Error, "UTF8 parsing error in identifier");
        in_++;
      }
    } else if (*in_ == '\\') {
      s.push_back(ParseEscape());
    } else {
      return s;
    }
  }
  return s;
}

// Returns the codepoint for the current escape.
// \abcdef => codepoint 0xabcdef.  also consumes whitespace afterwards.
// \(UTF8-encoded unicode character) => codepoint for that character
char32 Parser::ParseEscape() {
  Tracer trace(__func__, &in_);
  SkipSpace();
  DCHECK_LT(in_, end_);
  DCHECK_EQ(*in_, '\\');
  in_++;
  if (Done()) return static_cast<char32>('\\');

  char32 codepoint = 0;

  int dehexed = DeHex(*in_);
  if (dehexed == -1) {
    Rune rune;
    int len = charntorune(&rune, in_, end_-in_);
    if (len && rune != Runeerror) {
      in_ += len;
    } else {
      ReportParsingError(kUtf8Error, "UTF8 parsing error");
      in_++;
    }
    codepoint = rune;
  } else {
    for (int count = 0; count < 6 && in_ < end_; count++) {
      dehexed = DeHex(*in_);
      if (dehexed == -1)
        break;
      in_++;
      codepoint = codepoint << 4 | dehexed;
    }
    if (end_ - in_ >= 2 && memcmp(in_, "\r\n", 2) == 0)
      in_ += 2;
    else if (IsSpace(*in_))
      in_++;
  }

  if (!UniLib::IsInterchangeValid(codepoint)) {
    // From http://www.w3.org/TR/CSS2/syndata.html#escaped-characters:
    //   It is undefined in CSS 2.1 what happens if a style sheet does
    //   contain a character with Unicode codepoint zero.
    // We replace them (and all other improper escapes with a space
    // and log an error.
    ReportParsingError(kUtf8Error, StringPrintf(
        "Invalid CSS-escaped Unicode value: 0x%lX",
        static_cast<unsigned long int>(codepoint)));
    codepoint = ' ';
  }
  return codepoint;
}

// Starts at delim.
template<char delim>
UnicodeText Parser::ParseString() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  DCHECK_LT(in_, end_);
  DCHECK_EQ(*in_, delim);
  in_++;
  if (Done()) return UnicodeText();

  UnicodeText s;
  while (in_ < end_) {
    switch (*in_) {
      case delim:
        in_++;
        return s;
      case '\n':
        return s;
      case '\\':
        if (in_ + 1 < end_ && in_[1] == '\n') {
          in_ += 2;
        } else {
          s.push_back(ParseEscape());
        }
        break;
      default:
        if (!IsAscii(*in_)) {
          Rune rune;
          int len = charntorune(&rune, in_, end_-in_);
          if (len && rune != Runeerror) {
            s.push_back(rune);
            in_ += len;
          } else {
            ReportParsingError(kUtf8Error, "UTF8 parsing error in string");
            in_++;
          }
        } else {
          s.push_back(*in_);
          in_++;
        }
        break;
    }
  }
  return s;
}

// parse ident or 'string'
UnicodeText Parser::ParseStringOrIdent() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return UnicodeText();
  DCHECK_LT(in_, end_);

  if (*in_ == '\'') {
    return ParseString<'\''>();
  } else if (*in_ == '"') {
    return ParseString<'"'>();
  } else {
    return ParseIdent();
  }
}

// Parse a CSS number, including unit or percent sign.
Value* Parser::ParseNumber() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  const char* begin = in_;
  if (in_ < end_ && (*in_ == '-' || *in_ == '+'))  // sign
    in_++;
  while (in_ < end_ && isdigit(*in_)) {
    in_++;
  }
  if (*in_ == '.') {
    in_++;

    while (in_ < end_ && isdigit(*in_)) {
      in_++;
    }
  }
  double num = 0;
  if (in_ == begin || !ParseDouble(begin, in_ - begin, &num)) {
    ReportParsingError(kNumberError, StringPrintf(
        "Failed to parse number %s", string(begin, in_ - begin).c_str()));
    return NULL;
  }
  if (*in_ == '%') {
    in_++;
    return new Value(num, Value::PERCENT);
  } else if (StartsIdent(*in_)) {
    return new Value(num, ParseIdent());
  } else {
    return new Value(num, Value::NO_UNIT);
  }
}

HtmlColor Parser::ParseColor() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return HtmlColor("", 0);
  DCHECK_LT(in_, end_);

  unsigned char hexdigits[6] = {0};
  int dehexed;
  int i = 0;

  const char* oldin = in_;

  // To further mess things up, IE also accepts string values happily.
  if (*in_ == '"' || *in_ == '\'') {
    in_++;
    if (Done()) return HtmlColor("", 0);
  }

  bool rgb_valid = quirks_mode_ || *in_ == '#';

  if (*in_ == '#') in_++;

  while (in_ < end_ && i < 6 && (dehexed = DeHex(*in_)) != -1) {
    hexdigits[i] = static_cast<unsigned char>(dehexed);
    i++;
    in_++;
  }

  // close strings. Assume a named color if there are trailing characters
  if (*oldin == '"' || *oldin == '\'') {
    if (Done() || *in_ != *oldin)  // no need to touch in_, will redo anyway.
      i = 0;
    else
      in_++;
  }

  // Normally, ParseXXX() routines stop wherever it cannot be consumed and
  // doesn't check whether the next character is valid. which should be caught
  // by the next ParseXXX() routine. But ParseColor may be called to test
  // whether a numerical value can be used as color, and fail over to a normal
  // ParseAny(). We need to do an immediate check here to guarantine a valid
  // non-color number (such as 100%) will not be accepted as a color.
  //
  // We also do not want rrggbb (without #) to be accepted in non-quirks mode,
  // but HtmlColor will happily accept it anyway. Do a sanity check here.
  if (i == 3 || i == 6) {
    if (!rgb_valid ||
        (!Done() && (*in_ == '%' || StartsIdent(*in_))))
      return HtmlColor("", 0);
  }

  if (i == 3) {
    return HtmlColor(hexdigits[0] | hexdigits[0] << 4,
                     hexdigits[1] | hexdigits[1] << 4,
                     hexdigits[2] | hexdigits[2] << 4);
  } else if (i == 6) {
    return HtmlColor(hexdigits[1] | hexdigits[0] << 4,
                     hexdigits[3] | hexdigits[2] << 4,
                     hexdigits[5] | hexdigits[4] << 4);
  } else {
    in_ = oldin;

    // A named color must not begin with #, but we need to parse it anyway and
    // report failure later.
    bool name_valid = true;
    if (*in_ == '#') {
      in_++;
      name_valid = false;
    }

    string ident = UnicodeTextToUTF8(ParseStringOrIdent());
    HtmlColor val("", 0);
    if (name_valid) {
      val.SetValueFromName(ident.c_str());
      if (!val.IsDefined())
        Util::GetSystemColor(ident, &val);
    }
    return val;
  }
}

// Parse body of generic function foo(a, "b" 3, d(e, #fff)) without
// consuming final right-paren.
//
// Both commas and spaces are allowed as separators and are remembered.
FunctionParameters* Parser::ParseFunction() {
  Tracer trace(__func__, &in_);
  scoped_ptr<FunctionParameters> params(new FunctionParameters);

  SkipSpace();
  // Separator before next value. Initial value doesn't matter.
  FunctionParameters::Separator separator = FunctionParameters::SPACE_SEPARATED;
  while (!Done()) {
    DCHECK_LT(in_, end_);
    switch (*in_) {
      case ')':
        // End of function.
        return params.release();
        break;
      case ',':
        // Note that next value is comma-separated.
        separator = FunctionParameters::COMMA_SEPARATED;
        in_++;
        break;
      case ' ':
        // The only purpose of spaces between identifiers is as a separator.
        // Note: separator defaults to SPACE_SEPARATED.
        in_++;
        break;
      default: {
        // TODO(sligocki): Should we parse Opacity=80 as a single value?
        const StringPiece allowed_chars("=");
        scoped_ptr<Value> val(ParseAny(allowed_chars));
        if (!val.get()) {
          ReportParsingError(kFunctionError,
                             "Cannot parse parameter in function");
          return NULL;
        }
        if (!Done() && *in_ != ' ' && *in_ != ',' && *in_ != ')') {
          ReportParsingError(kFunctionError, StringPrintf(
              "Function parameter contains unexpected char '%c'", *in_));
          return NULL;
        }
        params->AddSepValue(separator, val.release());
        // Unless otherwise indicated, next item is space-separated.
        separator = FunctionParameters::SPACE_SEPARATED;
        break;
      }
    }
    SkipSpace();
  }

  return NULL;
}

// Returns the 0-255 RGB value corresponding to Value v.  Only
// unusual thing is percentages are interpreted as percentages of
// 255.0.
unsigned char Parser::ValueToRGB(Value* v) {
  int toret = 0;
  if (v == NULL) {
    toret = 0;
  } else if (v->GetLexicalUnitType() == Value::NUMBER) {
    if (v->GetDimension() == Value::PERCENT) {
      toret = static_cast<int>(v->GetFloatValue()/100.0 * 255.0);
    } else {
      toret = v->GetIntegerValue();
    }
  } else {
    toret = 0;
  }

  // RGB values outside the device gamut should be clipped according to spec.
  if (toret > 255)
    toret = 255;
  if (toret < 0)
    toret = 0;
  return static_cast<unsigned char>(toret);
}

// parse RGB color 25, 32, 12 or 25%, 1%, 7%.
// stops without consuming final right-paren
Value* Parser::ParseRgbColor() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  unsigned char rgb[3];

  for (int i = 0; i < 3; i++) {
    scoped_ptr<Value> val(ParseNumber());
    if (!val.get() || val->GetLexicalUnitType() != Value::NUMBER ||
        (val->GetDimension() != Value::PERCENT &&
         val->GetDimension() != Value::NO_UNIT))
      break;
    rgb[i] = ValueToRGB(val.get());
    SkipSpace();
    // Make sure the correct syntax is followed.
    if (Done() || (*in_ != ',' && *in_ != ')') || (*in_ == ')' && i != 2))
      break;

    if (*in_ == ')')
      return new Value(HtmlColor(rgb[0], rgb[1], rgb[2]));

    DCHECK_EQ(',', *in_);
    in_++;
  }

  return NULL;
}

// parse url yellow.png or 'yellow.png'
// (doesn't consume subsequent right-paren).
Value* Parser::ParseUrl() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  UnicodeText s;
  if (*in_ == '\'') {
    s = ParseString<'\''>();
  } else if (*in_ == '"') {
    s = ParseString<'"'>();
  } else {
    while (in_ < end_) {
      if (IsSpace(*in_) || *in_ == ')') {
        break;
      } else if (*in_ == '\\') {
        s.push_back(ParseEscape());
      } else if (!IsAscii(*in_)) {
        Rune rune;
        int len = charntorune(&rune, in_, end_-in_);
        if (len && rune != Runeerror) {
          s.push_back(rune);
          in_ += len;
        } else {
          ReportParsingError(kUtf8Error, "UTF8 parsing error in URL");
          in_++;
        }
      } else {
        s.push_back(*in_);
        in_++;
      }
    }
  }
  SkipSpace();
  if (!Done() && *in_ == ')')
    return new Value(Value::URI, s);

  return NULL;
}

Value* Parser::ParseAnyExpectingColor(const StringPiece& allowed_chars) {
  Tracer trace(__func__, &in_);
  Value* toret = NULL;

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  const char* oldin = in_;
  HtmlColor c = ParseColor();
  if (c.IsDefined()) {
    toret = new Value(c);
  } else {
    in_ = oldin;  // no valid color.  rollback.
    toret = ParseAny(allowed_chars);
  }
  return toret;
}

// Parses a CSS value.  Could be just about anything.
Value* Parser::ParseAny(const StringPiece& allowed_chars) {
  Tracer trace(__func__, &in_);
  Value* toret = NULL;

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  const char* oldin = in_;
  switch (*in_) {
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
    case '.':
      toret = ParseNumber();
      break;
    case '(': case '[': {
      ReportParsingError(kValueError, StringPrintf(
          "Unsupported value starting with %c", *in_));
      char delim = *in_ == '(' ? ')' : ']';
      SkipPastDelimiter(delim);
      toret = NULL;  // we don't understand this construct.
      break;
    }
    case '"':
      toret = new Value(Value::STRING, ParseString<'"'>());
      break;
    case '\'':
      toret = new Value(Value::STRING, ParseString<'\''>());
      break;
    case '#': {
      HtmlColor color = ParseColor();
      if (color.IsDefined())
        toret = new Value(color);
      else
        toret = NULL;
      break;
    }
    case '+':
      toret = ParseNumber();
      break;
    case '-':
      // ambiguity between a negative number and an identifier starting with -.
      if (in_ < end_ - 1 &&
          ((*(in_ + 1) >= '0' && *(in_ + 1) <= '9') || *(in_ + 1) == '.')) {
        toret = ParseNumber();
        break;
      }
      // fail through
    default: {
      UnicodeText id = ParseIdent(allowed_chars);
      if (id.empty()) {
        toret = NULL;
      } else if (*in_ == '(') {
        in_++;
        if (id.utf8_length() == 3
            && memcasecmp("url", id.utf8_data(), 3) == 0) {
          toret = ParseUrl();
        } else if (id.utf8_length() == 3
                   && memcasecmp("rgb", id.utf8_data(), 3) == 0) {
          toret = ParseRgbColor();
        } else if (id.utf8_length() == 4
                   && memcasecmp("rect", id.utf8_data(), 4) == 0) {
          scoped_ptr<FunctionParameters> params(ParseFunction());
          if (params.get() != NULL && params->size() == 4) {
            toret = new Value(Value::RECT, params.release());
          } else {
            ReportParsingError(kFunctionError, "Could not parse parameters "
                               "for function rect");
          }
        } else {
          scoped_ptr<FunctionParameters> params(ParseFunction());
          if (params.get() != NULL) {
            toret = new Value(id, params.release());
          } else {
            ReportParsingError(kFunctionError, StringPrintf(
                "Could not parse function parameters for function %s",
                UnicodeTextToUTF8(id).c_str()));
          }
        }
        SkipSpace();
        if (*in_ != ')') {
          ReportParsingError(kFunctionError,
                             "Ignored chars at end of function.");
        }
        SkipPastDelimiter(')');
      } else {
        toret = new Value(Identifier(id));
      }
      break;
    }
  }
  // Deadlock prevention: always make progress even if nothing can be parsed.
  if (toret == NULL && in_ == oldin) {
    ReportParsingError(kValueError, "Ignoring chars in value.");
    ++in_;
  }
  return toret;
}

static bool IsPropExpectingColor(Property::Prop prop) {
  switch (prop) {
    case Property::BORDER_COLOR:
    case Property::BORDER_TOP_COLOR:
    case Property::BORDER_RIGHT_COLOR:
    case Property::BORDER_BOTTOM_COLOR:
    case Property::BORDER_LEFT_COLOR:
    case Property::BORDER:
    case Property::BORDER_TOP:
    case Property::BORDER_RIGHT:
    case Property::BORDER_BOTTOM:
    case Property::BORDER_LEFT:
    case Property::BACKGROUND_COLOR:
    case Property::BACKGROUND:
    case Property::COLOR:
    case Property::OUTLINE_COLOR:
    case Property::OUTLINE:
      return true;
    default:
      return false;
  }
}

// Parse values like "12pt Arial"
// If you make any change to this function, please also update
// ParseBackground, ParseFont and ParseFontFamily accordingly.
Values* Parser::ParseValues(Property::Prop prop) {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return new Values();
  DCHECK_LT(in_, end_);

  // If expecting_color is true, color values are expected.
  bool expecting_color = IsPropExpectingColor(prop);

  scoped_ptr<Values> values(new Values);
  // Note: We skip over all blocks and at-keywords and only parse "any"s.
  //   value : [ any | block | ATKEYWORD S* ]+;
  // TODO(sligocki): According to the spec, if we cannot parse one of the
  // values, we must ignore the whole declaration.
  while (SkipToNextAny()) {
    const StringPiece allowed_chars(":.");
    scoped_ptr<Value> v(expecting_color ?
                        ParseAnyExpectingColor(allowed_chars) :
                        ParseAny(allowed_chars));

if (v.get()) {
      values->push_back(v.release());
    } else {
      return NULL;
    }
  }
  if (values->size() > 0) {
    return values.release();
  } else {
    return NULL;
  }
}

// Parse background. It is a shortcut property for individual background
// properties.
//
// The output is a tuple in the following order:
//   "background-color background-image background-repeat background-attachment
//   background-position-x background-position-y"
// or NULL if invalid
//
// The x-y position parsing is somewhat complicated. The following spec is from
// CSS 2.1.
// http://www.w3.org/TR/CSS21/colors.html#propdef-background-position
//
// "If a background image has been specified, this property specifies its
// initial position. If only one value is specified, the second value is
// assumed to be 'center'. If at least one value is not a keyword, then the
// first value represents the horizontal position and the second represents the
// vertical position. Negative <percentage> and <length> values are allowed.
// <percentage> ...
// <length> ...
// top ...
// right ...
// bottom ...
// left ...
// center ..."
//
// In addition, we have some IE specific behavior:
// 1) you can specifiy more than two values, but once both x and y have
//    specified values, further values will be discarded.
// 2) if y is not specified and x has seen two or more values, the last value
//    counts. The same for y.
// 3) [length, left/right] is valid and the length becomes a value for y.
//    [top/bottom, length] is also valid and the length becomes a value for x.
// If you make any change to this function, please also update ParseValues,
// ParseFont and ParseFontFamily if applicable.
bool Parser::ExpandBackground(const Declaration& original_declaration,
                              Declarations* new_declarations) {
  const Values* vals = original_declaration.values();
  bool important = original_declaration.IsImportant();
  DCHECK(vals != NULL);

  Value background_color(Identifier::TRANSPARENT);
  Value background_image(Identifier::NONE);
  Value background_repeat(Identifier::REPEAT);
  Value background_attachment(Identifier::SCROLL);
  scoped_ptr<Value> background_position_x;
  scoped_ptr<Value> background_position_y;

  bool is_first = true;

  // The following flag is used to implement IE quirks #3. When the first
  // positional value is a length or CENTER, it is stored in
  // background-position-x, but the value may actually be used as
  // background-position-y if a keyword LEFT or RIGHT appears later.
  bool first_is_ambiguous = false;  // Value::NUMBER or Identifier::CENTER

  for (Values::const_iterator iter = vals->begin(); iter != vals->end();
       ++iter) {
    const Value* val = *iter;

    // Firefox allows only one value to be set per property, IE need not.
    switch (val->GetLexicalUnitType()) {
      case Value::COLOR:
        // background_color, etc. take ownership of val. We will clear vals
        // at the end to make sure we don't have double ownership.
        background_color = *val;
        break;
      case Value::URI:
        background_image = *val;
        break;
      case Value::NUMBER:
        if (!background_position_x.get()) {
          background_position_x.reset(new Value(*val));
          first_is_ambiguous = true;
        } else if (!background_position_y.get()) {
          background_position_y.reset(new Value(*val));
        }
        break;
      case Value::IDENT:
        switch (val->GetIdentifier().ident()) {
          case Identifier::CENTER:
            if (!background_position_x.get()) {
              background_position_x.reset(new Value(*val));
              first_is_ambiguous = true;
            } else if (!background_position_y.get()) {
              background_position_y.reset(new Value(*val));
            }
            break;
          case Identifier::LEFT:
          case Identifier::RIGHT:
            // This is IE-specific behavior.
            if (!background_position_x.get() || !background_position_y.get()) {
              if (background_position_x.get() && first_is_ambiguous)
                background_position_y.reset(background_position_x.release());
              background_position_x.reset(new Value(*val));
              first_is_ambiguous = false;
            }
            break;
          case Identifier::TOP:
          case Identifier::BOTTOM:
            if (!background_position_x.get() || !background_position_y.get())
              background_position_y.reset(new Value(*val));
            break;
          case Identifier::REPEAT:
          case Identifier::REPEAT_X:
          case Identifier::REPEAT_Y:
          case Identifier::NO_REPEAT:
            background_repeat = *val;
            break;
          case Identifier::SCROLL:
          case Identifier::FIXED:
            background_attachment = *val;
            break;
          case Identifier::TRANSPARENT:
            background_color = *val;
            break;
          case Identifier::NONE:
            background_image = *val;
            break;
          case Identifier::INHERIT:
            // Inherit must be the one and only value.
            if (!(iter == vals->begin() && vals->size() == 1))
              return false;
            // We copy the inherit value into each background_* value.
            background_color = *val;
            background_image = *val;
            background_repeat = *val;
            background_attachment = *val;
            background_position_x.reset(new Value(*val));
            background_position_y.reset(new Value(*val));
            break;
          default:
            return false;
        }
        break;
      default:
        return false;
    }
    is_first = false;
  }
  if (is_first) return false;

  new_declarations->push_back(new Declaration(Property::BACKGROUND_COLOR,
                                              background_color,
                                              important));
  new_declarations->push_back(new Declaration(Property::BACKGROUND_IMAGE,
                                              background_image,
                                              important));
  new_declarations->push_back(new Declaration(Property::BACKGROUND_REPEAT,
                                              background_repeat,
                                              important));
  new_declarations->push_back(new Declaration(Property::BACKGROUND_ATTACHMENT,
                                              background_attachment,
                                              important));

  // Fix up x and y position.
  if (!background_position_x.get() && !background_position_y.get()) {
    background_position_x.reset(new Value(0, Value::PERCENT));
    background_position_y.reset(new Value(0, Value::PERCENT));
  } else if (!background_position_x.get()) {
    background_position_x.reset(new Value(50, Value::PERCENT));
  } else if (!background_position_y.get()) {
    background_position_y.reset(new Value(50, Value::PERCENT));
  }
  new_declarations->push_back(new Declaration(Property::BACKGROUND_POSITION_X,
                                              *background_position_x,
                                              important));
  new_declarations->push_back(new Declaration(Property::BACKGROUND_POSITION_Y,
                                              *background_position_y,
                                              important));

  return true;
}

// Parses font-family. It is special in that it uses commas as delimiters. It
// also concatenates adjacent idents into one name. Strings can be also used
// and they are separate from others even without commas.
// E.g, Courier New, Sans -> "Courier New", "Sans"
//      Arial "MS Times" monospace -> "Arial", "MS Times", "monospace".
// If you make any change to this function, please also update ParseValues,
// ParseBackground and ParseFont if applicable.
bool Parser::ParseFontFamily(Values* values) {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return true;
  DCHECK_LT(in_, end_);

  UnicodeText family;
  while (SkipToNextAny()) {
    if (*in_ == ',') {
      if (!family.empty()) {
        values->push_back(new Value(Identifier(family)));
        family.clear();
      }
      in_++;
    } else {
      scoped_ptr<Value> v(ParseAny());
      if (!v.get()) return false;
      switch (v->GetLexicalUnitType()) {
        case Value::STRING:
          if (!family.empty()) {
            values->push_back(new Value(Identifier(family)));
            family.clear();
          }
          values->push_back(v.release());
          break;
        case Value::IDENT:
          if (!family.empty())
            family.push_back(static_cast<char32>(' '));
          family.append(v->GetIdentifierText());
          break;
        default:
          return false;
      }
    }
  }
  if (!family.empty())
    values->push_back(new Value(Identifier(family)));
  return true;
}

// Parse font. It is special in that it uses a special format (see spec):
//  [ [ <'font-style'> || <'font-variant'> || <'font-weight'> ]?
//     <'font-size'> [ / <'line-height'> ]? <'font-family'> ]
//  | caption | icon | menu | message-box | small-caption | status-bar | inherit
//
// The output is a tuple in the following order:
//   "font-style font-variant font-weight font-size line-height font-family*"
// or NULL if invalid
// IE pecularity: font-family is optional (hence the *).
// If you make any change to this function, please also update ParseValues,
// ParseBackground and ParseFontFamily if applicable.
Values* Parser::ParseFont() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  scoped_ptr<Values> values(new Values);

  if (!SkipToNextAny())
    return NULL;

  scoped_ptr<Value> v(ParseAny());
  if (!v.get()) return NULL;

  // For special one-valued font: notations, just return with that one value.
  // Note: these can be expanded by ExpandShorthandProperties
  if (v->GetLexicalUnitType() == Value::IDENT) {
    switch (v->GetIdentifier().ident()) {
      case Identifier::CAPTION:
      case Identifier::ICON:
      case Identifier::MENU:
      case Identifier::MESSAGE_BOX:
      case Identifier::SMALL_CAPTION:
      case Identifier::STATUS_BAR:
      case Identifier::INHERIT:
        // These special identifiers must be the only one in a declaration.
        // Fail if there are others.
        // TODO(sligocki): We should probably raise an error here.
        if (SkipToNextAny()) return NULL;
        // If everything is good, push these out.
        values->push_back(v.release());
        return values.release();
      default:
        break;
    }
  }

  scoped_ptr<Value> font_style(new Value(Identifier::NORMAL));
  scoped_ptr<Value> font_variant(new Value(Identifier::NORMAL));
  scoped_ptr<Value> font_weight(new Value(Identifier::NORMAL));
  scoped_ptr<Value> font_size(new Value(Identifier::MEDIUM));
  scoped_ptr<Value> line_height(new Value(Identifier::NORMAL));
  scoped_ptr<Value> font_family;

  // parse style, variant and weight
  while (true) {
    // Firefox allows only one value to be set per property, IE need not.
    if (v->GetLexicalUnitType() == Value::IDENT) {
      switch (v->GetIdentifier().ident()) {
        case Identifier::NORMAL:
          // no-op
          break;
        case Identifier::ITALIC:
        case Identifier::OBLIQUE:
          font_style.reset(v.release());
          break;
        case Identifier::SMALL_CAPS:
          font_variant.reset(v.release());
          break;
        case Identifier::BOLD:
        case Identifier::BOLDER:
        case Identifier::LIGHTER:
          font_weight.reset(v.release());
          break;
        default:
          goto check_fontsize;
      }
    } else if (v->GetLexicalUnitType() == Value::NUMBER &&
               v->GetDimension() == Value::NO_UNIT) {
      switch (v->GetIntegerValue()) {
        // Different browsers handle this quite differently. But there
        // is at least a test that is consistent between IE and
        // firefox: try <span style="font:120 serif"> and <span
        // style="font:100 serif">, the first one treats 120 as
        // font-size, and the second does not.
        case 100: case 200: case 300: case 400:
        case 500: case 600: case 700: case 800:
        case 900:
          font_weight.reset(v.release());
          break;
        default:
          goto check_fontsize;
      }
    } else {
      goto check_fontsize;
    }
    if (!SkipToNextAny())
      return NULL;
    v.reset(ParseAny());
    if (!v.get()) return NULL;
  }

 check_fontsize:
  // parse font-size
  switch (v->GetLexicalUnitType()) {
    case Value::IDENT:
      switch (v->GetIdentifier().ident()) {
        case Identifier::XX_SMALL:
        case Identifier::X_SMALL:
        case Identifier::SMALL:
        case Identifier::MEDIUM:
        case Identifier::LARGE:
        case Identifier::X_LARGE:
        case Identifier::XX_LARGE:
        case Identifier::LARGER:
        case Identifier::SMALLER:
          font_size.reset(v.release());
          break;
        default:
          return NULL;
      }
      break;
    case Value::NUMBER:
      font_size.reset(v.release());
      break;
    default:
      return NULL;
  }

  // parse line-height if '/' is seen, or use the default line-height
  if (SkipToNextAny() && *in_ == '/') {
    in_++;
    if (!SkipToNextAny()) return NULL;
    v.reset(ParseAny());
    if (!v.get()) return NULL;

    switch (v->GetLexicalUnitType()) {
      case Value::IDENT:
        if (v->GetIdentifier().ident() == Identifier::NORMAL)
          break;
        else
          return NULL;
      case Value::NUMBER:
        line_height.reset(v.release());
        break;
      default:
        return NULL;
    }
  }

  values->push_back(font_style.release());
  values->push_back(font_variant.release());
  values->push_back(font_weight.release());
  values->push_back(font_size.release());
  values->push_back(line_height.release());

  if (!ParseFontFamily(values.get()))  // empty is okay.
    return NULL;
  return values.release();
}

static void ExpandShorthandProperties(Declarations* declarations,
                                      const Declaration& declaration) {
  Property prop = declaration.property();
  const Values* vals = declaration.values();
  bool important = declaration.IsImportant();

  // Buffer to build up values used instead of vals above.
  scoped_ptr<Values> edit_vals;
  switch (prop.prop()) {
    case Property::FONT: {
      // Expand the value vector for special font: values.
      if (vals->size() == 1) {
        const Value* val = vals->at(0);
        switch (val->GetIdentifier().ident()) {
          case Identifier::CAPTION:
          case Identifier::ICON:
          case Identifier::MENU:
          case Identifier::MESSAGE_BOX:
          case Identifier::SMALL_CAPTION:
          case Identifier::STATUS_BAR:
            edit_vals.reset(new Values());
            // Reasonable defaults to use for special font: declarations.
            edit_vals->push_back(new Value(Identifier::NORMAL)); // font-style
            edit_vals->push_back(new Value(Identifier::NORMAL)); // font-variant
            edit_vals->push_back(new Value(Identifier::NORMAL)); // font-weight
            // In this case, the actual font size will depend on browser,
            // this is a common value found in IE and Firefox:
            edit_vals->push_back(new Value(32.0/3, Value::PX));  // font-size
            edit_vals->push_back(new Value(Identifier::NORMAL)); // line-height
            // We store the special font type as font-family:
            edit_vals->push_back(new Value(*val));  // font-family
            vals = edit_vals.get();  // Move pointer to new, built-up values.
            break;
          case Identifier::INHERIT:
            edit_vals.reset(new Values());
            // font: inherit means all properties inherit.
            edit_vals->push_back(new Value(*val));  // font-style
            edit_vals->push_back(new Value(*val));  // font-variant
            edit_vals->push_back(new Value(*val));  // font-weight
            edit_vals->push_back(new Value(*val));  // font-size
            edit_vals->push_back(new Value(*val));  // line-height
            edit_vals->push_back(new Value(*val));  // font-family
            vals = edit_vals.get();  // Move pointer to new, built-up values.
            break;
          default:
            break;
        }
      }
      // Only expand valid font: declarations (ones created by ParseFont, which
      // requires at least 5 values in a specific order).
      if (vals->size() < 5) {
        LOG(ERROR) << "font: values are not in the correct format.\n" << vals;
        break;
      }
      declarations->push_back(
          new Declaration(Property::FONT_STYLE, *vals->get(0), important));
      declarations->push_back(
          new Declaration(Property::FONT_VARIANT, *vals->get(1), important));
      declarations->push_back(
          new Declaration(Property::FONT_WEIGHT, *vals->get(2), important));
      declarations->push_back(
          new Declaration(Property::FONT_SIZE, *vals->get(3), important));
      declarations->push_back(
          new Declaration(Property::LINE_HEIGHT, *vals->get(4), important));
      if (vals->size() > 5) {
        Values* family_vals = new Values;
        for (int i = 5, n = vals->size(); i < n; ++i)
          family_vals->push_back(new Value(*vals->get(i)));
        declarations->push_back(
            new Declaration(Property::FONT_FAMILY, family_vals, important));
      }
    }
      break;
    default:
      // TODO(yian): other shorthand properties:
      // background-position
      // border-color border-style border-width
      // border-top border-right border-bottom border-left
      // border
      // margin padding
      // outline
      break;
  }
}

// Parse declarations like "background: white; color: #333; line-height: 1.3;"
Declarations* Parser::ParseRawDeclarations() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return new Declarations();
  DCHECK_LT(in_, end_);

  Declarations* declarations = new Declarations();
  while (in_ < end_) {
    // decl_start is saved so that we may pass through verbatim text
    // in case declaration could not be parsed correctly.
    const char* decl_start = in_;
    const uint64 start_errors_seen_mask = errors_seen_mask_;
    bool ignore_this_decl = false;
    switch (*in_) {
      case ';':
        // TODO(sligocki): Is there any way declarations might not be separated
        // by ';' in the current code? We don't explicitly check.
        in_++;
        break;
      case '}':
        return declarations;
      default: {
        UnicodeText id;
        // While not allowed by the CSS spec, there is a common hack
        // placing * before idents selectively allowing them to be parsed
        // on some IE versions.
        // See: http://en.wikipedia.org/wiki/CSS_filter#Star_hack
        if (*in_ == '*') {
          id.CopyUTF8("*", 1);
          in_++;
          UnicodeText rest = ParseIdent();
          if (rest.empty()) {
            ReportParsingError(kDeclarationError, "Ignoring * property");
            ignore_this_decl = true;
            break;
          }
          id.append(rest);
        } else {
          id = ParseIdent();
          if (id.empty()) {
            ReportParsingError(kDeclarationError, "Ignoring empty property");
            ignore_this_decl = true;
            break;
          }
        }
        Property prop(id);
        SkipSpace();
        if (Done() || *in_ != ':') {
          ReportParsingError(kDeclarationError,
                             StringPrintf("Ignoring property with no values %s",
                                          prop.prop_text().c_str()));
          ignore_this_decl = true;
          break;
        }
        DCHECK_EQ(':', *in_);
        in_++;

        scoped_ptr<Values> vals;
        switch (prop.prop()) {
          // TODO(sligocki): stop special-casing.
          case Property::FONT:
            vals.reset(ParseFont());
            break;
          case Property::FONT_FAMILY:
            vals.reset(new Values());
            if (!ParseFontFamily(vals.get()) || vals->empty()) {
              vals.reset(NULL);
            }
            break;
          default:
            vals.reset(ParseValues(prop.prop()));
            break;
        }

        if (vals.get() == NULL) {
          ReportParsingError(kDeclarationError, StringPrintf(
              "Failed to parse values for property %s",
              prop.prop_text().c_str()));
          ignore_this_decl = true;
          break;
        }

        // If an error has occurred while parsing vals, some content may have
        // been lost (invalid Unicode chars, etc.). Thus, in preservation-mode
        // we just want to drop this malformed declaration and pass it through
        // verbatim below.
        //
        // Note: This will not preserve values if an error occurred which was
        // already in start_errors_seen_mask. But the goal of preservation
        // mode is to have errors_seen_mask_ held at 0, because any higher
        // than that and we cannot trust the output to be fully preserved.
        // So, we are not worried about failing to preserve values when
        // errors_seen_mask_ is already non-0.
        if (preservation_mode_ && errors_seen_mask_ != start_errors_seen_mask) {
          ReportParsingError(kDeclarationError, StringPrintf(
              "Error while parsing values for property %s",
              prop.prop_text().c_str()));
          ignore_this_decl = true;
          break;
        }

        bool important = false;
        if (in_ < end_ && *in_ == '!') {
          in_++;
          SkipSpace();
          UnicodeText ident = ParseIdent();
          if (ident.utf8_length() == 9 &&
              !memcasecmp(ident.utf8_data(), "important", 9))
            important = true;
        }
        declarations->push_back(
            new Declaration(prop, vals.release(), important));
      }
    }
    SkipSpace();
    if (ignore_this_decl) {  // on bad syntax, we skip till the next declaration
      errors_seen_mask_ |= kDeclarationError;
      while (in_ < end_ && *in_ != ';' && *in_ != '}') {
        // IE (and IE only) ignores {} blocks in quirks mode.
        if (*in_ == '{' && !quirks_mode_) {
          SkipBlock();  // ignore
        } else {
          in_++;
          SkipSpace();
        }
      }
      if (preservation_mode_) {
        // Add pseudo-declaration of verbatim text because we failed to parse
        // this declaration correctly. This is saved so that it can be
        // serialized back out in case it was actually meaningful even though
        // we could not understand it.
        StringPiece bytes_in_original_buffer(decl_start, in_ - decl_start);
        declarations->push_back(new Declaration(bytes_in_original_buffer));
        // All errors that occurred sinse we started this declaration are
        // demoted to unparseable sections now that we've saved the dummy
        // element.
        unparseable_sections_seen_mask_ |= errors_seen_mask_;
        errors_seen_mask_ = start_errors_seen_mask;
      }
    }
  }
  return declarations;
}

Declarations* Parser::ExpandDeclarations(Declarations* orig_declarations) {
  scoped_ptr<Declarations> new_declarations(new Declarations);
  for (int j = 0; j < orig_declarations->size(); ++j) {
    // new_declarations takes ownership of declaration.
    Declaration* declaration = orig_declarations->at(j);
    orig_declarations->at(j) = NULL;
    // TODO(yian): We currently store both expanded properties and the original
    // property because only limited expansion is supported. In future, we
    // should discard the original property after expansion.
    new_declarations->push_back(declaration);
    ExpandShorthandProperties(new_declarations.get(), *declaration);
    // TODO(sligocki): Get ExpandBackground back into ExpandShorthandProperties.
    switch (declaration->property().prop()) {
      case Css::Property::BACKGROUND: {
        ExpandBackground(*declaration, new_declarations.get());
        break;
      }
      default:
        break;
    }
  }
  return new_declarations.release();
}

Declarations* Parser::ParseDeclarations() {
  scoped_ptr<Declarations> orig_declarations(ParseRawDeclarations());
  return ExpandDeclarations(orig_declarations.get());
}

// Starts from [ and parses to the closing ]
// in [ foo ~= bar ].
// Whitespace is not skipped at beginning or the end.
SimpleSelector* Parser::ParseAttributeSelector() {
  Tracer trace(__func__, &in_);

  DCHECK_LT(in_, end_);
  DCHECK_EQ('[', *in_);
  in_++;
  SkipSpace();

  UnicodeText attr = ParseIdent();
  SkipSpace();
  scoped_ptr<SimpleSelector> newcond;
  if (!attr.empty() && in_ < end_) {
    char oper = *in_;
    switch (*in_) {
      case '~':
      case '|':
      case '^':
      case '$':
      case '*':
        in_++;
        if (Done() || *in_ != '=')
          break;
        // fall through
      case '=': {
        in_++;
        UnicodeText value = ParseStringOrIdent();
        if (!value.empty())
          newcond.reset(SimpleSelector::NewBinaryAttribute(
              SimpleSelector::AttributeTypeFromOperator(oper),
              attr,
              value));
        break;
      }
      default:
        newcond.reset(SimpleSelector::NewExistAttribute(attr));
        break;
    }
  }
  SkipSpace();
  if (*in_ != ']') {
    ReportParsingError(kSelectorError, "Ignoring chars in attribute selector.");
  }
  if (SkipPastDelimiter(']'))
    return newcond.release();
  else
    return NULL;
}

SimpleSelector* Parser::ParseSimpleSelector() {
  Tracer trace(__func__, &in_);

  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  switch (*in_) {
    case '#': {
      in_++;
      UnicodeText id = ParseIdent();
      if (!id.empty())
        return SimpleSelector::NewId(id);
      break;
    }
    case '.': {
      in_++;
      UnicodeText classname = ParseIdent();
      if (!classname.empty())
        return SimpleSelector::NewClass(classname);
      break;
    }
    case ':': {
      UnicodeText sep;
      in_++;
      // CSS3 requires all pseudo-elements to use :: to distinguish them from
      // pseudo-classes. We save which separator was used in the Pseudoclass
      // object, so that the original value can be reconstructed.
      //
      // http://www.w3.org/TR/css3-selectors/#pseudo-elements
      if (*in_ == ':') {
        in_++;
        sep.CopyUTF8("::", 2);
      } else {
        sep.CopyUTF8(":", 1);
      }
      UnicodeText pseudoclass = ParseIdent();
      // FIXME(yian): skip constructs "(en)" in lang(en) for now.
      if (in_ < end_ && *in_ == '(') {
        ReportParsingError(kSelectorError,
                           "Cannot parse parameters for pseudoclass.");
        in_++;
        SkipSpace();
        ParseIdent();
        if (!SkipPastDelimiter(')'))
          break;
      }
      if (!pseudoclass.empty())
        return SimpleSelector::NewPseudoclass(pseudoclass, sep);
      break;
    }
    case '[': {
      SimpleSelector* newcond = ParseAttributeSelector();
      if (newcond)
        return newcond;
      break;
    }
    case '*':
      in_++;
      return SimpleSelector::NewUniversal();
      break;
    default: {
      UnicodeText ident = ParseIdent();
      if (!ident.empty())
        return SimpleSelector::NewElementType(ident);
      break;
    }
  }
  // We did not parse anything or we parsed something incorrectly.
  return NULL;
}

bool Parser::AtValidSimpleSelectorsTerminator() const {
  if (Done()) return true;
  switch (*in_) {
    case ' ': case '\t': case '\r': case '\n': case '\f':
    case ',': case '{': case '>': case '+':
      return true;
    case '/':
      if (in_ + 1 < end_ && *(in_ + 1) == '*')
        return true;
      break;
  }
  return false;
}

SimpleSelectors* Parser::ParseSimpleSelectors(bool expecting_combinator) {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  SimpleSelectors::Combinator combinator;
  if (!expecting_combinator)
    combinator = SimpleSelectors::NONE;
  else
    switch (*in_) {
      case '>':
        in_++;
        combinator = SimpleSelectors::CHILD;
        break;
      case '+':
        in_++;
        combinator = SimpleSelectors::SIBLING;
        break;
      default:
        combinator = SimpleSelectors::DESCENDANT;
        break;
    }

  scoped_ptr<SimpleSelectors> selectors(
      new SimpleSelectors(combinator));

  SkipSpace();
  if (Done()) return NULL;

  const char* oldin = in_;
  while (SimpleSelector* simpleselector = ParseSimpleSelector()) {
    selectors->push_back(simpleselector);
    oldin = in_;
  }

  if (selectors->size() > 0 &&  // at least one simple selector stored
      in_ == oldin &&           // the last NULL does not make progress
      AtValidSimpleSelectorsTerminator())  // stop at a valid terminator
    return selectors.release();

  return NULL;
}

Selectors* Parser::ParseSelectors() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  // Remember whether anything goes wrong, but continue parsing until the
  // declaration starts or the position comes to the end. Then discard the
  // selectors.
  bool success = true;

  scoped_ptr<Selectors> selectors(new Selectors());
  Selector* selector = new Selector();
  selectors->push_back(selector);

  // The first simple selector sequence in a chain of simple selector
  // sequences does not have a combinator.  ParseSimpleSelectors needs
  // to know this, so we set this to false here and after ',', and
  // true after we see a simple selector sequence.
  bool expecting_combinator = false;
  while (in_ < end_ && *in_ != '{') {
    switch (*in_) {
      case ',':
        if (selector->size() == 0) {
          success = false;
          ReportParsingError(kSelectorError,
                             "Could not parse ruleset: unexpected ,");
        } else {
          selector = new Selector();
          selectors->push_back(selector);
        }
        in_++;
        expecting_combinator = false;
        break;
      default: {
        const char* oldin = in_;
        SimpleSelectors* simple_selectors
          = ParseSimpleSelectors(expecting_combinator);
        if (!simple_selectors) {
          success = false;
          if (in_ == oldin) {
            ReportParsingError(kSelectorError, StringPrintf(
                "Could not parse selector: illegal char %c", *in_));
            in_++;
          }
        } else {
          selector->push_back(simple_selectors);
        }

        expecting_combinator = true;
        break;
      }
    }
    SkipSpace();
  }

  if (selector->size() == 0)
    success = false;

  if (success)
    return selectors.release();
  else
    return NULL;
}

Import* Parser::ParseAsSingleImport() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);
  if (*in_ != '@') return NULL;
  ++in_;

  UnicodeText ident = ParseIdent();

  // @import string|uri medium-list ? ;
  if (ident.utf8_length() != 6 ||
      memcasecmp(ident.utf8_data(), "import", 6) != 0) {
    return NULL;
  }

  Import* import = ParseImport();

  SkipSpace();
  if (Done()) return import;

  // There's something after the @import, which is expressly disallowed.
  delete import;
  return NULL;
}

UnicodeText Parser::ExtractCharset() {
  Tracer trace(__func__, &in_);

  UnicodeText result;
  if (*in_ == '@') {
    ++in_;
    UnicodeText ident = ParseIdent();
    if (ident.utf8_length() == 7 &&
        memcasecmp(ident.utf8_data(), "charset", 7) == 0) {
      result = ParseCharset();
    }
  }
  return result;
}

UnicodeText Parser::ParseCharset() {
  Tracer trace(__func__, &in_);

  UnicodeText result;
  SkipSpace();
  switch (*in_) {
    case '\'': {
      result = ParseString<'\''>();
      break;
    }
    case '"': {
      result = ParseString<'"'>();
      break;
    }
    default: {
      ReportParsingError(kCharsetError, "@charset lacks string.");
      break;
    }
  }
  SkipSpace();
  if (*in_ != ';') {
    ReportParsingError(kCharsetError,
                       "Ignoring chars at end of charset declaration.");
  }
  SkipPastDelimiter(';');
  return result;
}

Ruleset* Parser::ParseRuleset() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  // Remember whether anything goes wrong, but continue parsing until the
  // closing }. Then discard the whole ruleset if necessary. This allows the
  // parser to make progress anyway.
  bool success = true;
  const char* start_pos = in_;
  const uint64 start_errors_seen_mask = errors_seen_mask_;

  scoped_ptr<Ruleset> ruleset(new Ruleset());
  scoped_ptr<Selectors> selectors(ParseSelectors());

  if (Done()) {
    ReportParsingError(kSelectorError,
                       "Selectors without declarations at end of doc.");
    return NULL;
  }

  // In preservation_mode_ we want to use verbatim text whenever we got a
  // parsing error during selector parsing, so clear the partial parse here.
  if (preservation_mode_ && (start_errors_seen_mask != errors_seen_mask_)) {
    selectors.reset(NULL);
  }

  if (selectors.get() == NULL) {
    ReportParsingError(kSelectorError, "Failed to parse selector");
    if (preservation_mode_) {
      selectors.reset(new Selectors(StringPiece(start_pos, in_ - start_pos)));
      ruleset->set_selectors(selectors.release());
      // All errors that occurred sinse we started this declaration are
      // demoted to unparseable sections now that we've saved the dummy
      // element.
      unparseable_sections_seen_mask_ |= errors_seen_mask_;
      errors_seen_mask_ = start_errors_seen_mask;
    } else {
      // http://www.w3.org/TR/CSS21/syndata.html#rule-sets
      // When a user agent can't parse the selector (i.e., it is not
      // valid CSS 2.1), it must ignore the declaration block as
      // well.
      success = false;
    }
  } else {
    ruleset->set_selectors(selectors.release());
  }

  DCHECK_EQ('{', *in_);
  in_++;
  ruleset->set_declarations(ParseRawDeclarations());

  SkipSpace();
  if (*in_ != '}') {
    // TODO(sligocki): Can this ever be hit? Add a test that does.
    ReportParsingError(kRulesetError, "Ignored chars at end of ruleset.");
  }
  SkipPastDelimiter('}');

  if (success)
    return ruleset.release();
  else
    return NULL;
}

void Parser::ParseMediumList(std::vector<UnicodeText>* media) {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return;
  DCHECK_LT(in_, end_);

  while (in_ < end_) {
    switch (*in_) {
      case ';':
      case '{':
        return;
      case ',':
        in_++;
        break;
      default:
        scoped_ptr<Value> v(ParseAny());
        if (v.get() && v->GetLexicalUnitType() == Value::IDENT) {
          media->push_back(v->GetIdentifierText());
        } else {
          ReportParsingError(kMediaError, "Failed to parse media");
        }
        break;
    }
    SkipSpace();
  }
}

// Start after @import is parsed.
Import* Parser::ParseImport() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return NULL;
  DCHECK_LT(in_, end_);

  scoped_ptr<Value> v(ParseAny());
  if (!v.get() || (v->GetLexicalUnitType() != Value::STRING &&
                   v->GetLexicalUnitType() != Value::URI))
    return NULL;

  Import* import = new Import();
  import->link = v->GetStringValue();

  ParseMediumList(&import->media);
  if (in_ < end_ && *in_ == ';') in_++;
  return import;
}

void Parser::ParseAtRule(Stylesheet* stylesheet) {
  Tracer trace(__func__, &in_);

  SkipSpace();
  DCHECK_LT(in_, end_);
  DCHECK_EQ('@', *in_);

  // The starting point is saved so that we may pass through verbatim text
  // in case the @-rule cannot be parsed correctly.
  const char* at_rule_start = in_;
  const uint64 start_errors_seen_mask = errors_seen_mask_;
  in_++;

  UnicodeText ident = ParseIdent();

  // @import string|uri medium-list ? ;
  if (ident.utf8_length() == 6 &&
      memcasecmp(ident.utf8_data(), "import", 6) == 0) {
    scoped_ptr<Import> import(ParseImport());
    if (import.get() && stylesheet) {
      stylesheet->mutable_imports().push_back(import.release());
    } else {
      ReportParsingError(kImportError, "Failed to parse import");
      SkipPastDelimiter(';');
    }

  // @charset string ;
  } else if (ident.utf8_length() == 7 &&
             memcasecmp(ident.utf8_data(), "charset", 7) == 0) {
    UnicodeText s = ParseCharset();
    stylesheet->mutable_charsets().push_back(s);

  // @media medium-list { ruleset-list }
  } else if (ident.utf8_length() == 5 &&
             memcasecmp(ident.utf8_data(), "media", 5) == 0) {
    std::vector<UnicodeText> media;
    ParseMediumList(&media);
    if (Done()) {
      ReportParsingError(kMediaError, "Unexpected EOF in @media statement.");
      return;
    } else if (*in_ == ';') {
      // @media tags ending in ';' are no-ops, we simply ignore them.
      // Skip over ending ';'
      in_++;
      return;
    } else if (*in_ != '{') {
      ReportParsingError(kMediaError, "Malformed @media statement.");
      return;
    }
    DCHECK_EQ('{', *in_);
    in_++;
    SkipSpace();
    while (in_ < end_ && *in_ != '}') {
      const char* oldin = in_;
      // TODO(sligocki): Do we need to be able to parse at-rules here.
      scoped_ptr<Ruleset> ruleset(ParseRuleset());
      if (!ruleset.get() && in_ == oldin) {
        ReportParsingError(kSelectorError, StringPrintf(
            "Could not parse ruleset: illegal char %c", *in_));
        in_++;
      }
      if (ruleset.get()) {
        ruleset->set_media(media);
        stylesheet->mutable_rulesets().push_back(ruleset.release());
      }
      SkipSpace();
    }
    if (in_ < end_) {
      DCHECK_EQ('}', *in_);
      in_++;
    }

  // Unexpected @-rule.
  } else {
    string ident_string(ident.utf8_data(), ident.utf8_length());
    ReportParsingError(kAtRuleError, StringPrintf(
        "Cannot parse unknown @-statement: %s", ident_string.c_str()));
    SkipToAtRuleEnd();

    if (preservation_mode_) {
      // Add a place-holder with verbatim text because we failed to parse
      // this @-rule correctly. This is saved so that it can be
      // serialized back out in case it was actually meaningful even though
      // we could not understand it.
      StringPiece bytes_in_original_buffer(at_rule_start, in_ - at_rule_start);
      stylesheet->mutable_rulesets().push_back(
          new Ruleset(new UnparsedRegion(bytes_in_original_buffer)));
      // All errors that occurred sinse we started this declaration are
      // demoted to unparseable sections now that we've saved the dummy
      // element.
      unparseable_sections_seen_mask_ |= errors_seen_mask_;
      errors_seen_mask_ = start_errors_seen_mask;
    }
  }
}

// From http://www.w3.org/TR/CSS2/syndata.html#parsing-errors:
//
//   At-rules with unknown at-keywords. User agents must ignore an invalid
//   at-keyword together with everything following it, up to the end of the
//   block that contains the invalid at-keyword, or up to and including the
//   next semicolon (;), or up to and including the next block ({...}),
//   whichever comes first.
void Parser::SkipToAtRuleEnd() {
  Tracer trace(__func__, &in_);

  while (in_ < end_) {
    switch (*in_) {
      // "up to the end of the block that contains the invalid at-keyword"
      case '}':
        // Note: Do not advance in_, so that caller will see closing '}'.
        return;
      // "up to and including the next semicolon (;)"
      case ';':
        ++in_;
        return;
      // "up to and including the next block ({...})"
      case '{':
        SkipBlock();
        return;
      // Skip over all other chars.
      default:
        ++in_;
        break;
    }
  }
}

void Parser::SkipBlock() {
  Tracer trace(__func__, &in_);

  ReportParsingError(kBlockError, "Ignoring {} block.");

  SkipSpace();
  DCHECK_LT(in_, end_);
  DCHECK_EQ('{', *in_);
  int depth = 0;
  while (in_ < end_) {
    switch (*in_) {
      case '{':
        in_++;
        depth++;
        break;
      case '@':
        in_++;
        ParseIdent();
        break;
      case ';':
        in_++;
        break;
      case '}':
        in_++;
        depth--;
        if (depth == 0)
          return;
        break;
      default:
        // Ignore whatever there is to parse.
        scoped_ptr<Value> v(ParseAny());
        break;
    }
    SkipSpace();
  }
}

Stylesheet* Parser::ParseRawStylesheet() {
  Tracer trace(__func__, &in_);

  SkipSpace();
  if (Done()) return new Stylesheet();
  DCHECK_LT(in_, end_);

  Stylesheet* stylesheet = new Stylesheet();
  while (in_ < end_) {
    switch (*in_) {
      // HTML-style comments are not allowed in CSS.
      // In fact, "<!--" and "-->" are ignored when parsing CSS.
      // Probably a legacy from when browsers didn't support <style> tags.
      case '<':
        in_++;
        if (end_ - in_ >= 3 && memcmp(in_, "!--", 3) == 0) {
          in_ += 3;
        } else {
          ReportParsingError(kHtmlCommentError, "< without following !--");
        }
        break;
      case '-':
        in_++;
        if (end_ - in_ >= 2 && memcmp(in_, "->", 2) == 0) {
          in_ += 2;
        } else {
          ReportParsingError(kHtmlCommentError, "- without following ->");
        }
        break;
      case '@':
        ParseAtRule(stylesheet);
        break;
      default: {
        const char* oldin = in_;
        scoped_ptr<Ruleset> ruleset(ParseRuleset());
        if (!ruleset.get() && oldin == in_) {
          ReportParsingError(kSelectorError, StringPrintf(
              "Could not parse ruleset: illegal char %c", *in_));
          in_++;
        }
        if (ruleset.get())
          stylesheet->mutable_rulesets().push_back(ruleset.release());
        break;
      }
    }
    SkipSpace();
  }

  DCHECK(Done()) << "Finished parsing before end of document.";

  return stylesheet;
}

Stylesheet* Parser::ParseStylesheet() {
  Tracer trace(__func__, &in_);

  Stylesheet* stylesheet = ParseRawStylesheet();

  Rulesets& rulesets = stylesheet->mutable_rulesets();
  for (int i = 0; i < rulesets.size(); ++i) {
    if (rulesets[i]->type() == Css::Ruleset::RULESET) {
      Declarations& orig_declarations = rulesets[i]->mutable_declarations();
      rulesets[i]->set_declarations(ExpandDeclarations(&orig_declarations));
    }
  }

  return stylesheet;
}

//
// Some destructors that need STLDeleteElements() from stl_util-inl.h
//

Declarations::~Declarations() { STLDeleteElements(this); }
Rulesets::~Rulesets() { STLDeleteElements(this); }
Charsets::~Charsets() {}
Imports::~Imports() { STLDeleteElements(this); }

}  // namespace
