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

// Copyright 2007 Google Inc. All Rights Reserved.
// Author: yian@google.com (Yi-An Huang)

#include "webutil/css/tostring.h"

#include <string>
#include <vector>

#include "base/stringprintf.h"
#include "strings/join.h"
#include "strings/strutil.h"
#include "webutil/css/parser.h"
#include "webutil/css/string.h"
#include "webutil/css/string_util.h"

class UnicodeText;

namespace Css {

namespace {

// Is this char safe to be emitted un-escaped in an unquoted URL?
bool IsUrlSafe(char c) {
  // From http://www.w3.org/TR/css3-syntax/#tokenization :
  //   urlchar  ::=  [#x9#x21#x23-#x26#x27-#x7E] | nonascii | escape
  // This appears to be a typo and should be:
  //   urlchar  ::=  [#x9#x21#x23-#x26#x28-#x7E] | nonascii | escape
  // Specifically, allowed chars are TAB (#x9) + all printable ASCII chars
  // except for SPACE (#x20), " (#x22) and ' (#x27) + all non-ascii and
  // backslash-escaped chars.
  if (c >= 0x21 && c <= 0x7e) {
    switch (c) {
      // SPACE, " and ' specifically disallowed.
      case ' ': case '"': case '\'':
      // Backslash clearly needs to be escaped.
      case '\\':
      // Parentheses generally need to be matched correctly, so we escape
      // them too, just to be safe.
      case '(': case ')': case '{': case '}': case '[': case ']':
        return false;
      default:
        // All other printable chars are allowed.
        return true;
    }
  } else if (!IsAscii(c)) {
    // Non-ASCII chars are allowed.
    return true;
  }
  // Everything else is not allowed.
  // Note: TAB (\t, #x9) is technically safe in unquoted URLs, but we escape it.
  return false;
}

// Is this char safe to be emitted un-escaped in a string?
bool IsStringSafe(char c) {
  // From http://www.w3.org/TR/css3-syntax/#tokenization :
  //   string     ::=  '"' (stringchar | "'")* '"' | "'" (stringchar | '"')* "'"
  //   stringchar ::=  urlchar | #x20 | '\' nl

  // We could allow ' or " through unescaped depending on what delimiter this
  // string used, but for now we just escape both of them.
  // '\' nl is ignored in strings, so the only difference between strings and
  // URLs is that SPACE is allowed unescaped in strings.
  return (c == ' ' || IsUrlSafe(c));
}

// Is this char safe to be emitted un-escaped in an identifier?
// Note: This is not technically valid for the first ident char, which cannot
// be a digit. (Likewise if the first char is a hyphen, the second cannot be
// a digit.)
bool IsIdentSafe(char c) {
  // From http://www.w3.org/TR/css3-syntax/#tokenization :
  //   ident    ::=  '-'? nmstart nmchar*
  //   nmstart  ::=  [a-zA-Z] | '_' | nonascii | escape
  //   nmchar   ::=  [a-zA-Z0-9] | '-' | '_' | nonascii | escape
  return ((c >= 'A' && c <= 'Z') ||
          (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') ||
          c == '-' || c == '_' ||
          !IsAscii(c));
}

// Escape an ASCII char and append it to dest.
void AppendEscapedAsciiChar(char c, string* dest) {
  // From http://www.w3.org/TR/CSS21/syndata.html#tokenization :
  //   escape  {unicode}|\\[^\n\r\f0-9a-f]

  DCHECK(IsAscii(c)) << "AppendEscapedAsciiChar called on non-ASCII char " << c;
  switch (c) {
    // Note: CSS does not use standard \n, \r and \f escapes and they cannot
    // be specified by simply preceding them with a backslash.
    // Generic hex escapes are used instead.
    // See: http://www.w3.org/TR/CSS2/syndata.html#strings
    //
    // Note: Hex escapes in CSS must end in space.
    // See: http://www.w3.org/TR/CSS2/syndata.html#characters
    case '\n':
      *dest += "\\A ";
      break;
    case '\r':
      *dest += "\\D ";
      break;
    case '\f':
      *dest += "\\C ";
      break;
    // \t is not specifically disallowed by the spec, but we escape it anyway
    // because it seems safer to be conservative and tabs should be pretty
    // uncommon in CSS.
    case '\t':
      *dest += "\\9 ";
      break;
    default:
      // All other ASCII chars can just be escaped with a backslash.
      // TODO(sligocki): Actually [0-9a-fA-F] also cannot be escaped this way
      // because of ambiguity with Unicode escapes. Fix.
      dest->push_back('\\');
      dest->push_back(c);
      break;
  }
}

}  // namespace

string EscapeString(StringPiece src) {
  string dest;
  dest.reserve(src.size());  // Minimum possible expansion

  for (int i = 0, n = src.size(); i < n; ++i) {
    if (IsStringSafe(src[i])) {
      // Safe to be appended un-escaped.
      dest.push_back(src[i]);
    } else {
      AppendEscapedAsciiChar(src[i], &dest);
    }
  }

  return dest;
}

string EscapeString(const UnicodeText& src) {
  return EscapeString(StringPiece(src.utf8_data(), src.utf8_length()));
}

string EscapeUrl(StringPiece src) {
  string dest;
  dest.reserve(src.size());  // Minimum possible expansion

  for (int i = 0, n = src.size(); i < n; ++i) {
    if (IsUrlSafe(src[i])) {
      // Safe to be appended un-escaped.
      dest.push_back(src[i]);
    } else {
      AppendEscapedAsciiChar(src[i], &dest);
    }
  }

  return dest;
}

string EscapeUrl(const UnicodeText& src) {
  return EscapeUrl(StringPiece(src.utf8_data(), src.utf8_length()));
}

string EscapeIdentifier(StringPiece src) {
  string dest;
  dest.reserve(src.size());  // Minimum possible expansion

  // TODO(sligocki): Identifiers cannot start with a digit ([0-9]). We need
  // to escape it if src starts with a digit.
  for (int i = 0, n = src.size(); i < n; ++i) {
    if (IsIdentSafe(src[i])) {
      // Safe to be appended un-escaped.
      dest.push_back(src[i]);
    } else {
      AppendEscapedAsciiChar(src[i], &dest);
    }
  }

  return dest;
}

string EscapeIdentifier(const UnicodeText& text) {
  // TODO(sligocki): Should we Unicode escape all non-ASCII symbols?
  return EscapeIdentifier(StringPiece(text.utf8_data(), text.utf8_length()));
}

template <typename Container>
static string JoinElementStrings(const Container& c, const char* delim) {
  std::vector<string> vals;
  vals.reserve(c.size());
  for (typename Container::const_iterator it = c.begin(); it != c.end(); ++it)
    vals.push_back((*it)->ToString());
  string result;
  JoinStrings(vals, delim, &result);
  return result;
}

static string StylesheetTypeString(Stylesheet::StylesheetType type) {
  switch (type) {
    case Stylesheet::AUTHOR: return string("AUTHOR");
    case Stylesheet::USER:   return string("USER");
    case Stylesheet::SYSTEM: return string("SYSTEM");
    default:
      LOG(FATAL) << "Invalid type";
  }
}

string Value::ToString() const {
  switch (GetLexicalUnitType()) {
    case NUMBER:
      return StringPrintf("%g%s",
                          GetFloatValue(),
                          GetDimensionUnitText().c_str());
    case URI:
      return StringPrintf("url(%s)",
                          Css::EscapeUrl(GetStringValue()).c_str());
    case FUNCTION:
      return StringPrintf("%s(%s)",
                          Css::EscapeIdentifier(GetFunctionName()).c_str(),
                          GetParametersWithSeparators()->ToString().c_str());
    case RECT:
      return StringPrintf("rect(%s)",
                          GetParametersWithSeparators()->ToString().c_str());
    case COLOR:
      if (GetColorValue().IsDefined())
        return GetColorValue().ToString();
      else
        return "bad";
    case STRING:
      return StringPrintf("\"%s\"",
                          Css::EscapeString(GetStringValue()).c_str());
    case IDENT:
      return Css::EscapeIdentifier(GetIdentifierText());
    case UNKNOWN:
      return "UNKNOWN";
    case DEFAULT:
      return "";
    default:
      LOG(FATAL) << "Invalid type";
  }
}

string Values::ToString() const {
  return JoinElementStrings(*this, " ");
}

string FunctionParameters::ToString() const {
  string ret;
  if (size() >= 1) {
    ret += value(0)->ToString();
  }
  for (int i = 1, n = size(); i < n; ++i) {
    switch (separator(i)) {
      case FunctionParameters::COMMA_SEPARATED:
        ret += ", ";
        break;
      case FunctionParameters::SPACE_SEPARATED:
        ret += " ";
        break;
    }
    ret += value(i)->ToString();
  }
  return ret;
}

string SimpleSelector::ToString() const {
  switch (type()) {
    case ELEMENT_TYPE:
      return Css::EscapeIdentifier(element_text());
    case UNIVERSAL:
      return "*";
    case EXIST_ATTRIBUTE:
      return StringPrintf("[%s]",
                          Css::EscapeIdentifier(attribute()).c_str());
    case EXACT_ATTRIBUTE:
      // TODO(sligocki): Maybe print value out as identifier if that's smaller.
      // The value here (and below) can be either a string or identifier.
      // We currently always print it as a string because that is easier and
      // more failsafe (note for example that [height="1"] would need to be
      // converted to [height=\49 ] to remain an identifier :/).
      return StringPrintf("[%s=\"%s\"]",
                          Css::EscapeIdentifier(attribute()).c_str(),
                          Css::EscapeString(value()).c_str());
    case ONE_OF_ATTRIBUTE:
      return StringPrintf("[%s~=\"%s\"]",
                          Css::EscapeIdentifier(attribute()).c_str(),
                          Css::EscapeString(value()).c_str());
    case BEGIN_HYPHEN_ATTRIBUTE:
      return StringPrintf("[%s|=\"%s\"]",
                          Css::EscapeIdentifier(attribute()).c_str(),
                          Css::EscapeString(value()).c_str());
    case SUBSTRING_ATTRIBUTE:
      return StringPrintf("[%s*=\"%s\"]",
                          Css::EscapeIdentifier(attribute()).c_str(),
                          Css::EscapeString(value()).c_str());
    case BEGIN_WITH_ATTRIBUTE:
      return StringPrintf("[%s^=\"%s\"]",
                          Css::EscapeIdentifier(attribute()).c_str(),
                          Css::EscapeString(value()).c_str());
    case END_WITH_ATTRIBUTE:
      return StringPrintf("[%s$=\"%s\"]",
                          Css::EscapeIdentifier(attribute()).c_str(),
                          Css::EscapeString(value()).c_str());
    case CLASS:
      return StringPrintf(".%s",
                          Css::EscapeIdentifier(value()).c_str());
    case ID:
      return StringPrintf("#%s",
                          Css::EscapeIdentifier(value()).c_str());
    case PSEUDOCLASS:
      return StringPrintf("%s%s",
                          // pseudoclass_separator() is either ":" or "::".
                          UnicodeTextToUTF8(pseudoclass_separator()).c_str(),
                          Css::EscapeIdentifier(pseudoclass()).c_str());
    case LANG:
      return StringPrintf(":lang(%s)",
                          Css::EscapeIdentifier(lang()).c_str());
    default:
      LOG(FATAL) << "Invalid type";
  }
}

string SimpleSelectors::ToString() const {
  string prefix;
  switch (combinator()) {
    case CHILD:
      prefix = "> ";
      break;
    case SIBLING:
      prefix = "+ ";
      break;
    default:
      break;
  }
  return prefix + JoinElementStrings(*this, "");
}

string Selector::ToString() const {
  return JoinElementStrings(*this, " ");
}

string Selectors::ToString() const {
  if (is_dummy()) {
    string result = "/* Unparsed selectors: */ ";
    bytes_in_original_buffer().AppendToString(&result);
    return result;
  } else {
    return JoinElementStrings(*this, ", ");
  }
}

string Declaration::ToString() const {
  string result = prop_text() + ": ";
  switch (prop()) {
    case Property::UNPARSEABLE:
      result = "/* Unparsed declaration: */ ";
      bytes_in_original_buffer().AppendToString(&result);
      return result;
      break;
    case Property::FONT_FAMILY:
      result += JoinElementStrings(*values(), ",");
      break;
    case Property::FONT:
      if (values()->size() == 1) {
        // Special one-value font: notations like "font: menu".
        result += values()->ToString();
      } else if (values()->size() < 5) {
        result += "bad";
      } else {
        string tmp;
        tmp = values()->get(0)->ToString();
        if (tmp != "normal") result += tmp + " ";
        tmp = values()->get(1)->ToString();
        if (tmp != "normal") result += tmp + " ";
        tmp = values()->get(2)->ToString();
        if (tmp != "normal") result += tmp + " ";
        result += values()->get(3)->ToString();
        tmp = values()->get(4)->ToString();
        if (tmp != "normal") result += "/" + tmp;
        for (int i = 5, n = values()->size(); i < n; ++i)
          result += (i == 5 ? " " : ",") + values()->get(i)->ToString();
      }
      break;
    default:
      result += values()->ToString();
      break;
  }
  if (IsImportant())
    result += " !important";
  return result;
}

string Declarations::ToString() const {
  return JoinElementStrings(*this, "; ");
}

string UnparsedRegion::ToString() const {
  string result = "/* Unparsed region: */ ";
  bytes_in_original_buffer().AppendToString(&result);
  return result;
}

string MediaExpression::ToString() const {
  string result = "(";
  result += Css::EscapeIdentifier(name());
  if (has_value()) {
    result += ": ";
    // Note: While this is not a string, it is a mixture of text that should
    // be escaped in roughly the same way.
    result += Css::EscapeString(value());
  }
  result += ")";
  return result;
}

string MediaQuery::ToString() const {
  string result;
  switch (qualifier()) {
    case Css::MediaQuery::ONLY:
      result += "only ";
      break;
    case Css::MediaQuery::NOT:
      result += "not ";
      break;
    case Css::MediaQuery::NO_QUALIFIER:
      break;
  }

  result += Css::EscapeIdentifier(media_type());
  if (!media_type().empty() && !expressions().empty()) {
    result += " and ";
  }
  result += JoinElementStrings(expressions(), " and ");
  return result;
}

string MediaQueries::ToString() const {
  return JoinElementStrings(*this, ", ");
}

string Ruleset::ToString() const {
  string result;
  if (!media_queries().empty())
    result += StringPrintf("@media %s { ", media_queries().ToString().c_str());
  switch (type()) {
    case RULESET:
      result += selectors().ToString() + " {" + declarations().ToString() + "}";
      break;
    case UNPARSED_REGION:
      result = unparsed_region()->ToString();
      break;
  }
  if (!media_queries().empty())
    result += " }";
  return result;
}

string Charsets::ToString() const {
  string result;
  for (const_iterator iter = begin(); iter != end(); ++iter) {
    result += StringPrintf("@charset \"%s\";",
                           Css::EscapeString(*iter).c_str());
  }
  return result;
}

string Import::ToString() const {
  return StringPrintf("@import url(\"%s\") %s;",
                      Css::EscapeUrl(link()).c_str(),
                      media_queries().ToString().c_str());
}

string Stylesheet::ToString() const {
  string result;
  result += "/* " + StylesheetTypeString(type()) + " */\n";
  result += charsets().ToString() + "\n";
  result += JoinElementStrings(imports(), "\n") + "\n";
  result += JoinElementStrings(rulesets(), "\n") + "\n";
  return result;
}

}  // namespace Css
