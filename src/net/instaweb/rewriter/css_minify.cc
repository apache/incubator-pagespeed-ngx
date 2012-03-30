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

#include <cstddef>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/writer.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"
#include "webutil/css/property.h"
#include "webutil/css/selector.h"
#include "webutil/css/value.h"
#include "webutil/html/htmlcolor.h"

namespace net_instaweb {

namespace {

GoogleString CSSEscapeString(const UnicodeText& src) {
  return CssMinify::EscapeString(
      StringPiece(src.utf8_data(), src.utf8_length()), false);
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

bool CssMinify::Stylesheet(const Css::Stylesheet& stylesheet,
                           const GoogleUrl& base_url,
                           Writer* writer,
                           MessageHandler* handler) {
  // Get an object to encapsulate writing.
  CssMinify minifier(base_url, writer, handler);
  minifier.Minify(stylesheet);
  return minifier.ok_;
}

bool CssMinify::Declarations(const Css::Declarations& declarations,
                             Writer* writer,
                             MessageHandler* handler) {
  // Get an object to encapsulate writing.
  CssMinify minifier(writer, handler);
  minifier.JoinMinify(declarations, ";");
  return minifier.ok_;
}

bool CssMinify::AbsolutifyImports(Css::Stylesheet* stylesheet,
                                  const GoogleUrl& base) {
  bool result = false;
  const Css::Imports& imports = stylesheet->imports();
  Css::Imports::const_iterator iter;
  for (iter = imports.begin(); iter != imports.end(); ++iter) {
    Css::Import* import = *iter;
    StringPiece url(import->link.utf8_data(), import->link.utf8_length());
    GoogleUrl gurl(base, url);
    if (gurl.is_valid() && gurl.Spec() != url) {
      url = gurl.Spec();
      import->link.CopyUTF8(url.data(), url.length());
      result = true;
    }
  }
  return result;
}

bool CssMinify::AbsolutifyUrls(Css::Stylesheet* stylesheet,
                               const GoogleUrl& base,
                               bool handle_parseable_sections,
                               bool handle_unparseable_sections,
                               RewriteDriver* driver,
                               MessageHandler* handler) {
  RewriteDomainTransformer transformer(&base, &base, driver);
  transformer.set_trim_urls(false);
  bool result = false;
  Css::Rulesets& rulesets = stylesheet->mutable_rulesets();
  for (Css::Rulesets::iterator ruleset_iter = rulesets.begin();
       ruleset_iter != rulesets.end(); ++ruleset_iter) {
    Css::Ruleset* ruleset = *ruleset_iter;
    Css::Declarations& decls = ruleset->mutable_declarations();
    for (Css::Declarations::iterator decl_iter = decls.begin();
         decl_iter != decls.end(); ++decl_iter) {
      Css::Declaration* decl = *decl_iter;
      switch (decl->prop()) {
        case Css::Property::UNPARSEABLE:
          if (handle_unparseable_sections) {
            StringPiece original_bytes = decl->bytes_in_original_buffer();
            GoogleString rewritten_bytes;
            StringWriter writer(&rewritten_bytes);
            if (CssTagScanner::TransformUrls(original_bytes, &writer,
                                             &transformer, handler)) {
              result = true;
              decl->set_bytes_in_original_buffer(rewritten_bytes);
            }
            break;
          }
        case Css::Property::BACKGROUND:
        case Css::Property::BACKGROUND_IMAGE:
        case Css::Property::CONTENT:  // In CSS2 but not CSS2.1
        case Css::Property::CURSOR:
        case Css::Property::LIST_STYLE:
        case Css::Property::LIST_STYLE_IMAGE:
          if (handle_parseable_sections) {
            // [cribbed from css_image_rewriter_async.cc]
            // Rewrite all URLs. Technically, background-image should only
            // have a single value which is a URL, but background could have
            // more values.
            Css::Values* values = decl->mutable_values();
            for (size_t value_index = 0; value_index < values->size();
                 value_index++) {
              Css::Value* value = values->at(value_index);
              if (value->GetLexicalUnitType() == Css::Value::URI) {
                result = true;
                GoogleString in = UnicodeTextToUTF8(value->GetStringValue());
                GoogleString out;
                transformer.Transform(in, &out);
                if (in != out) {
                  delete (*values)[value_index];
                  (*values)[value_index] =
                      new Css::Value(Css::Value::URI,
                                     UTF8ToUnicodeText(out.data(), out.size()));
                }
              }
            }
            break;
          }
        default:
          break;
      }
    }
  }

  return result;
}

// Escape [() \t\r\n\\'"].  Also escape , for non-URLs.  Escaping , in
// URLs causes IE8 to interpret the backslash as a forward slash.
//
GoogleString CssMinify::EscapeString(const StringPiece& src, bool in_url) {
  GoogleString dest;
  dest.reserve(src.size());  // Minimum possible expansion

  const char* src_end = src.data() + src.size();

  for (const char* p = src.data(); p < src_end; p++) {
    switch (*p) {
      // Note: CSS does not use standard \n, \r and \t escapes.
      // Generic hex escapes are used instead.
      // See: http://www.w3.org/TR/CSS2/syndata.html#strings
      //
      // Note: Hex escapes in CSS must end in space.
      // See: http://www.w3.org/TR/CSS2/syndata.html#characters
      case '\n':
        dest += "\\A ";
        break;
      case '\r':
        dest += "\\D ";
        break;
      case '\t':
        dest += "\\9 ";
        break;
      case ',':
        if (in_url) {
          dest.push_back(*p);
          break;
        }
        // fall through -- we are escaping commas for non-URLs.
      case '\"': case '\'': case '\\': case '(': case ')':
        dest.push_back('\\');
        dest.push_back(*p);
          break;
      default:
        dest.push_back(*p);
        break;
    }
  }

  return dest;
}

CssMinify::CssMinify(const GoogleUrl& base_url,
                     Writer* writer, MessageHandler* handler)
    : base_url_(base_url.UncheckedSpec()),
      writer_(writer), handler_(handler), ok_(true) {
  DCHECK(base_url_.is_valid());
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

void CssMinify::WriteURL(const UnicodeText& url) {
  StringPiece string_url(url.utf8_data(), url.utf8_length());
  GoogleUrl abs_url;
  if (!string_url.empty() && base_url_.is_valid()) {
    abs_url.Reset(base_url_, string_url);
    if (abs_url.is_valid()) {
      string_url = abs_url.Spec();
    }
  }
  Write(EscapeString(string_url, true));
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

template<>
void CssMinify::JoinMinifyIter<Css::Rulesets::const_iterator>(
    const Css::Rulesets::const_iterator& begin,
    const Css::Rulesets::const_iterator& end,
    const StringPiece& sep) {
  // Go through the list of rulesets finding the contiguous subsets with the
  // same set of media (f.ex [a b b b a a] -> [a] [b b b] [a a]). For each
  // such subset, emit the start of the @media rule (if required), then emit
  // each ruleset without an @media rule, separating them by the given 'sep',
  // then emit the end of the @media rule (if required).
  for (Css::Rulesets::const_iterator iter = begin; iter != end; ) {
    Css::Rulesets::const_iterator first = iter;
    MinifyRulesetMediaStart(**first);
    MinifyRulesetIgnoringMedia(**first);
    for (++iter; iter != end && (*first)->media() == (*iter)->media(); ++iter) {
      Write(sep);
      MinifyRulesetIgnoringMedia(**iter);
    }
    MinifyRulesetMediaEnd(**first);
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
  WriteURL(import.link);
  Write(") ");
  JoinMediaMinify(import.media, ",");
  Write(";");
}

void CssMinify::MinifyRulesetIgnoringMedia(const Css::Ruleset& ruleset) {
  if (ruleset.selectors().is_dummy()) {
    Write(ruleset.selectors().bytes_in_original_buffer());
  } else {
    JoinMinify(ruleset.selectors(), ",");
  }
  Write("{");
  JoinMinify(ruleset.declarations(), ";");
  Write("}");
}

void CssMinify::MinifyRulesetMediaStart(const Css::Ruleset& ruleset) {
  if (!ruleset.media().empty()) {
    Write("@media ");
    JoinMediaMinify(ruleset.media(), ",");
    Write("{");
  }
}

void CssMinify::MinifyRulesetMediaEnd(const Css::Ruleset& ruleset) {
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
  Write(EscapeString(sselector.ToString(), false));
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
  if (declaration.prop() == Css::Property::UNPARSEABLE) {
    Write(declaration.bytes_in_original_buffer());
  } else {
    Write(EscapeString(declaration.prop_text(), false));
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
}

void CssMinify::Minify(const Css::Value& value) {
  switch (value.GetLexicalUnitType()) {
    case Css::Value::NUMBER: {
      // TODO(sligocki): Minify number
      // TODO(sligocki): Check that exponential notation is appropriate.
      // TODO(sligocki): Distinguish integers from float and print differently.
      // We use .16 to get most precission without getting rounding artifacts.
      GoogleString unit = EscapeString(value.GetDimensionUnitText(), false);
      Write(StringPrintf("%.16g%s", value.GetFloatValue(), unit.c_str()));
      break;
    }
    case Css::Value::URI:
      Write("url(");
      WriteURL(value.GetStringValue());
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
