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

#include <string>
#include <vector>

#include "base/stringprintf.h"
#include "strings/join.h"
#include "strings/strutil.h"
#include "webutil/css/parser.h"
#include "webutil/css/string.h"

namespace Css {

// Escape [(), \t\r\n\\'"]
// Based on CEscape/CEscapeString from strings/strutil.cc.
static string CSSEscapeString(const StringPiece& src) {
  const int dest_length = src.size() * 2 + 1;  // Maximum possible expansion
  scoped_array<char> dest(new char[dest_length]);

  const char* src_end = src.data() + src.size();
  int used = 0;

  for (const char* p = src.data(); p < src_end; p++) {
    switch (*p) {
      // Note: CSS does not use standard \n, \r and \t escapes.
      // Generic hex escapes are used instead.
      case '\n':
        dest[used++] = '\\'; dest[used++] = 'A'; dest[used++] = ' '; break;
      case '\r':
        dest[used++] = '\\'; dest[used++] = 'D'; dest[used++] = ' '; break;
      case '\t':
        dest[used++] = '\\'; dest[used++] = '9'; dest[used++] = ' '; break;
      case '\"': case '\'': case '\\': case ',': case '(': case ')':
          dest[used++] = '\\';
          dest[used++] = *p;
          break;
      default: dest[used++] = *p; break;
    }
  }

  return string(dest.get(), used);
}

static string CSSEscapeString(const UnicodeText& src) {
  return CSSEscapeString(StringPiece(src.utf8_data(), src.utf8_length()));
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

static string JoinMediaStrings(const std::vector<UnicodeText>& media,
                               const char* delim) {
  std::vector<string> vals;
  vals.reserve(media.size());
  for (std::vector<UnicodeText>::const_iterator
       it = media.begin(); it != media.end(); ++it)
    vals.push_back(CSSEscapeString(*it));
  string result;
  JoinStrings(vals, delim, &result);
  return result;
}

static string StylesheetTypeString(Stylesheet::StylesheetType type) {
  switch (type) {
    CONSIDER_IN_CLASS(Stylesheet, AUTHOR);
    CONSIDER_IN_CLASS(Stylesheet, USER);
    CONSIDER_IN_CLASS(Stylesheet, SYSTEM);
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
                          CSSEscapeString(GetStringValue()).c_str());
    case FUNCTION:
      return StringPrintf("%s(%s)",
                          CSSEscapeString(GetFunctionName()).c_str(),
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
                          CSSEscapeString(GetStringValue()).c_str());
    case IDENT:
      return CSSEscapeString(GetIdentifierText());
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
      return UnicodeTextToUTF8(element_text());
    case UNIVERSAL:
      return "*";
    case EXIST_ATTRIBUTE:
      return StringPrintf("[%s]",
                          CSSEscapeString(attribute()).c_str());
    case EXACT_ATTRIBUTE:
      return StringPrintf("[%s=%s]",
                          CSSEscapeString(attribute()).c_str(),
                          CSSEscapeString(value()).c_str());
    case ONE_OF_ATTRIBUTE:
      return StringPrintf("[%s~=%s]",
                          CSSEscapeString(attribute()).c_str(),
                          CSSEscapeString(value()).c_str());
    case BEGIN_HYPHEN_ATTRIBUTE:
      return StringPrintf("[%s|=%s]",
                          CSSEscapeString(attribute()).c_str(),
                          CSSEscapeString(value()).c_str());
    case SUBSTRING_ATTRIBUTE:
      return StringPrintf("[%s*=%s]",
                          CSSEscapeString(attribute()).c_str(),
                          CSSEscapeString(value()).c_str());
    case BEGIN_WITH_ATTRIBUTE:
      return StringPrintf("[%s^=%s]",
                          CSSEscapeString(attribute()).c_str(),
                          CSSEscapeString(value()).c_str());
    case END_WITH_ATTRIBUTE:
      return StringPrintf("[%s$=%s]",
                          CSSEscapeString(attribute()).c_str(),
                          CSSEscapeString(value()).c_str());
    case CLASS:
      return StringPrintf(".%s",
                          CSSEscapeString(value()).c_str());
    case ID:
      return StringPrintf("#%s",
                          CSSEscapeString(value()).c_str());
    case PSEUDOCLASS:
      return StringPrintf("%s%s",
                          // pseudoclass_separator() is either ":" or "::".
                          UnicodeTextToUTF8(pseudoclass_separator()).c_str(),
                          CSSEscapeString(pseudoclass()).c_str());
    case LANG:
      return StringPrintf(":lang(%s)",
                          CSSEscapeString(lang()).c_str());
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
  return JoinElementStrings(*this, ", ");
}

string Declaration::ToString() const {
  string result = prop_text() + ": ";
  switch (prop()) {
    case Property::UNPARSEABLE:
      result = "/* Unparsed declaration: */ ";
      text_in_original_buffer().AppendToString(&result);
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

string Ruleset::ToString() const {
  string result;
  if (!media().empty())
    result += StringPrintf("@media %s { ",
                           JoinMediaStrings(media(), ",").c_str());
  result += selectors().ToString() + " {" + declarations().ToString() + "}";
  if (!media().empty())
    result += " }";
  return result;
}

string Charsets::ToString() const {
  string result;
  for (const_iterator iter = begin(); iter != end(); ++iter) {
    result += StringPrintf("@charset \"%s\";", CSSEscapeString(*iter).c_str());
  }
  return result;
}

string Import::ToString() const {
  return StringPrintf("@import url(\"%s\") %s;",
                      CSSEscapeString(link).c_str(),
                      JoinMediaStrings(media, ",").c_str());
}

string Stylesheet::ToString() const {
  string result;
  result += "/* " + StylesheetTypeString(type()) + " */\n";
  result += charsets().ToString() + "\n";
  result += JoinElementStrings(imports(), "\n") + "\n";
  result += JoinElementStrings(rulesets(), "\n") + "\n";
  return result;
}

}  // namespace
