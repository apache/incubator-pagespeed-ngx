/*
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_minify.h"

#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/writer.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"
#include "webutil/css/property.h"
#include "webutil/css/selector.h"
#include "webutil/css/value.h"
#include "webutil/html/htmlcolor.h"

namespace net_instaweb {

namespace {

// Escape [(), \t\r\n\\'"]
GoogleString CSSEscapeString(const StringPiece& src) {
  const int dest_length = src.size() * 2 + 1;  // Maximum possible expansion
  scoped_array<char> dest(new char[dest_length]);

  const char* src_end = src.data() + src.size();
  int used = 0;

  for (const char* p = src.data(); p < src_end; p++) {
    switch (*p) {
      case '\n': dest[used++] = '\\'; dest[used++] = 'n';  break;
      case '\r': dest[used++] = '\\'; dest[used++] = 'r';  break;
      case '\t': dest[used++] = '\\'; dest[used++] = 't';  break;
      case '\"': case '\'': case '\\': case ',': case '(': case ')':
          dest[used++] = '\\';
          dest[used++] = *p;
          break;
      default: dest[used++] = *p; break;
    }
  }

  return GoogleString(dest.get(), used);
}

GoogleString CSSEscapeString(const UnicodeText& src) {
  return CSSEscapeString(StringPiece(src.utf8_data(), src.utf8_length()));
}

}  // namespace

bool CssMinify::Stylesheet(const Css::Stylesheet& stylesheet,
                           Writer* writer,
                           MessageHandler* handler) {
  // Get an object to encapsulate writing.
  CssMinify minifier(writer, handler);
  minifier.Minify(stylesheet);
  return minifier.ok_;
}

CssMinify::CssMinify(Writer* writer, MessageHandler* handler)
    : writer_(writer), handler_(handler), ok_(true) {
}

CssMinify::~CssMinify() {
}

// Write if we have not encountered write error yet.
void CssMinify::Write(const StringPiece& str) {
  if (ok_) {
    ok_ &= writer_->Write(str, handler_);
  }
}

// Write out minified version of each element of vector using supplied function
// seperated by sep.
template<typename Container>
void CssMinify::JoinMinify(const Container& container, const StringPiece& sep) {
  JoinMinifyIter(container.begin(), container.end(), sep);
}

template<typename Iterator>
void CssMinify::JoinMinifyIter(const Iterator& begin, const Iterator& end,
                               const StringPiece& sep) {
  for (Iterator iter = begin; iter != end; ++iter) {
    if (iter != begin) {
      Write(sep);
    }
    Minify(**iter);
  }
}

template<typename Container>
void CssMinify::JoinMediaMinify(const Container& container,
                                const StringPiece& sep) {
  for (typename Container::const_iterator iter = container.begin();
       iter != container.end(); ++iter) {
    if (iter != container.begin()) {
      Write(sep);
    }
    Write(CSSEscapeString(*iter));
  }
}


// Write the minified versions of each type. Most of these are called via
// templated instantiations of JoinMinify (or JoinMinifyIter) so that we can
// abstract the idea of minifying all sub-elements of a vector and joining them
// together.
//   Adapted from webutil/css/tostring.cc

void CssMinify::Minify(const Css::Stylesheet& stylesheet) {
  // We might want to add in unnecessary newlines between rules and imports
  // so that some readability is preserved.
  Minify(stylesheet.charsets());
  JoinMinify(stylesheet.imports(), "");
  JoinMinify(stylesheet.rulesets(), "");
}

void CssMinify::Minify(const Css::Charsets& charsets) {
  for (Css::Charsets::const_iterator iter = charsets.begin();
       iter != charsets.end(); ++iter) {
    Write("@charset \"");
    Write(CSSEscapeString(*iter));
    Write("\";");
  }
}

void CssMinify::Minify(const Css::Import& import) {
  Write("@import url(");
  // TODO(sligocki): Make a URL printer method that absolutifies and prints.
  Write(CSSEscapeString(import.link));
  Write(") ");
  JoinMediaMinify(import.media, ",");
  Write(";");
}

void CssMinify::Minify(const Css::Ruleset& ruleset) {
  if (!ruleset.media().empty()) {
    Write("@media ");
    JoinMediaMinify(ruleset.media(), ",");
    Write("{");
  }

  JoinMinify(ruleset.selectors(), ",");
  Write("{");
  JoinMinify(ruleset.declarations(), ";");
  Write("}");

  if (!ruleset.media().empty()) {
    Write("}");
  }
}

void CssMinify::Minify(const Css::Selector& selector) {
  // Note Css::Selector == std::vector<Css::SimpleSelectors*>
  Css::Selector::const_iterator iter = selector.begin();
  if (iter != selector.end()) {
    bool isfirst = true;
    Minify(**iter, isfirst);
    ++iter;
    JoinMinifyIter(iter, selector.end(), "");
  }
}

void CssMinify::Minify(const Css::SimpleSelectors& sselectors, bool isfirst) {
  if (sselectors.combinator() == Css::SimpleSelectors::CHILD) {
    Write(">");
  } else if (sselectors.combinator() == Css::SimpleSelectors::SIBLING) {
    Write("+");
  } else if (!isfirst) {
    Write(" ");
  }
  // Note Css::SimpleSelectors == std::vector<Css::SimpleSelector*>
  JoinMinify(sselectors, "");
}

void CssMinify::Minify(const Css::SimpleSelector& sselector) {
  // SimpleSelector::ToString is already basically minified.
  Write(sselector.ToString());
}

namespace {

// TODO(sligocki): Either make this an accessible function in
// webutil/css/tostring or specialize it for minifier.
//
// Note that currently the style is terrible and it will crash the program if
// we have >= 5 args.
GoogleString FontToString(const Css::Values& font_values) {
  CHECK_LE(5U, font_values.size());
  GoogleString tmp, result;

  // font-style: defaults to normal
  tmp = font_values.get(0)->ToString();
  if (tmp != "normal") result += tmp + " ";
  // font-variant: defaults to normal
  tmp = font_values.get(1)->ToString();
  if (tmp != "normal") result += tmp + " ";
  // font-weight: defaults to normal
  tmp = font_values.get(2)->ToString();
  if (tmp != "normal") result += tmp + " ";
  // font-size is required
  result += font_values.get(3)->ToString();
  // line-height: defaults to normal
  tmp = font_values.get(4)->ToString();
  if (tmp != "normal") result += "/" + tmp;
  // font-family:
  for (int i = 5, n = font_values.size(); i < n; ++i)
    result += (i == 5 ? " " : ",") + font_values.get(i)->ToString();

  return result;
}

}  // namespace

void CssMinify::Minify(const Css::Declaration& declaration) {
  Write(declaration.prop_text());
  Write(":");
  switch (declaration.prop()) {
    case Css::Property::FONT_FAMILY:
      JoinMinify(*declaration.values(), ",");
      break;
    case Css::Property::FONT:
      // font: menu special case.
      if (declaration.values()->size() == 1) {
        JoinMinify(*declaration.values(), " ");
      // Normal font notation.
      } else if (declaration.values()->size() >= 5) {
        Write(FontToString(*declaration.values()));
      } else {
        handler_->Message(kError, "Unexpected number of values in "
                          "font declaration: %d",
                          static_cast<int>(declaration.values()->size()));
        ok_ = false;
      }
      break;
    default:
      JoinMinify(*declaration.values(), " ");
      break;
  }
  if (declaration.IsImportant()) {
    Write("!important");
  }
}

void CssMinify::Minify(const Css::Value& value) {
  switch (value.GetLexicalUnitType()) {
    case Css::Value::NUMBER:
      // TODO(sligocki): Minify number
      // TODO(sligocki): Check that exponential notation is appropriate.
      // TODO(sligocki): Distinguish integers from float and print differently.
      // We use .16 to get most precission without getting rounding artifacts.
      Write(StringPrintf("%.16g%s",
                         value.GetFloatValue(),
                         value.GetDimensionUnitText().c_str()));
      break;
    case Css::Value::URI:
      // TODO(sligocki): Make a URL printer method that absolutifies and prints.
      Write("url(");
      Write(CSSEscapeString(value.GetStringValue()));
      Write(")");
      break;
    case Css::Value::FUNCTION:
      Write(CSSEscapeString(value.GetFunctionName()));
      Write("(");
      Minify(*value.GetParametersWithSeparators());
      Write(")");
      break;
    case Css::Value::RECT:
      Write("rect(");
      Minify(*value.GetParametersWithSeparators());
      Write(")");
      break;
    case Css::Value::COLOR:
      // TODO(sligocki): Can we assert, or might this happen in the wild?
      CHECK(value.GetColorValue().IsDefined());
      Write(HtmlColorUtils::MaybeConvertToCssShorthand(
          value.GetColorValue()));
      break;
    case Css::Value::STRING:
      Write("\"");
      Write(CSSEscapeString(value.GetStringValue()));
      Write("\"");
      break;
    case Css::Value::IDENT:
      Write(CSSEscapeString(value.GetIdentifierText()));
      break;
    case Css::Value::UNKNOWN:
      handler_->Message(kError, "Unknown attribute");
      ok_ = false;
      break;
    case Css::Value::DEFAULT:
      break;
  }
}

void CssMinify::Minify(const Css::FunctionParameters& parameters) {
  if (parameters.size() >= 1) {
    Minify(*parameters.value(0));
  }
  for (int i = 1, n = parameters.size(); i < n; ++i) {
    switch (parameters.separator(i)) {
      case Css::FunctionParameters::COMMA_SEPARATED:
        Write(",");
        break;
      case Css::FunctionParameters::SPACE_SEPARATED:
        Write(" ");
        break;
    }
    Minify(*parameters.value(i));
  }
}

}  // namespace net_instaweb
