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
#include "webutil/css/identifier.h"
#include "webutil/css/media.h"
#include "webutil/css/parser.h"
#include "webutil/css/property.h"
#include "webutil/css/selector.h"
#include "webutil/css/tostring.h"
#include "webutil/css/value.h"
#include "webutil/html/htmlcolor.h"

namespace net_instaweb {

bool CssMinify::Stylesheet(const Css::Stylesheet& stylesheet,
                           Writer* writer,
                           MessageHandler* handler) {
  // Get an object to encapsulate writing.
  CssMinify minifier(writer, handler);
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
    StringPiece url(import->link().utf8_data(), import->link().utf8_length());
    GoogleUrl gurl(base, url);
    if (gurl.IsWebValid() && gurl.Spec() != url) {
      url = gurl.Spec();
      import->set_link(UTF8ToUnicodeText(url.data(), url.length()));
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

  // Absolutify URLs in unparseable selectors and declarations.
  Css::Rulesets& rulesets = stylesheet->mutable_rulesets();
  for (Css::Rulesets::iterator ruleset_iter = rulesets.begin();
       ruleset_iter != rulesets.end(); ++ruleset_iter) {
    Css::Ruleset* ruleset = *ruleset_iter;
    // Check any unparseable sections for any URLs and absolutify as required.
    if (handle_unparseable_sections) {
      switch (ruleset->type()) {
        case Css::Ruleset::RULESET: {
          Css::Selectors& selectors(ruleset->mutable_selectors());
          if (selectors.is_dummy()) {
            StringPiece original_bytes = selectors.bytes_in_original_buffer();
            GoogleString rewritten_bytes;
            StringWriter writer(&rewritten_bytes);
            if (CssTagScanner::TransformUrls(original_bytes, &writer,
                                             &transformer, handler)) {
              selectors.set_bytes_in_original_buffer(rewritten_bytes);
              result = true;
            }
          }
          break;
        }
        case Css::Ruleset::UNPARSED_REGION: {
          Css::UnparsedRegion* unparsed = ruleset->mutable_unparsed_region();
          StringPiece original_bytes = unparsed->bytes_in_original_buffer();
          GoogleString rewritten_bytes;
          StringWriter writer(&rewritten_bytes);
          if (CssTagScanner::TransformUrls(original_bytes, &writer,
                                           &transformer, handler)) {
            unparsed->set_bytes_in_original_buffer(rewritten_bytes);
            result = true;
          }
          break;
        }
      }
    }
    if (ruleset->type() == Css::Ruleset::RULESET) {
      Css::Declarations& decls = ruleset->mutable_declarations();
      for (Css::Declarations::iterator decl_iter = decls.begin();
           decl_iter != decls.end(); ++decl_iter) {
        Css::Declaration* decl = *decl_iter;
        if (decl->prop() == Css::Property::UNPARSEABLE) {
          if (handle_unparseable_sections) {
            StringPiece original_bytes = decl->bytes_in_original_buffer();
            GoogleString rewritten_bytes;
            StringWriter writer(&rewritten_bytes);
            if (CssTagScanner::TransformUrls(original_bytes, &writer,
                                             &transformer, handler)) {
              result = true;
              decl->set_bytes_in_original_buffer(rewritten_bytes);
            }
          }
        } else if (handle_parseable_sections) {
          // [cribbed from css_image_rewriter.cc]
          // Rewrite all URLs.
          // Note: We must rewrite all URLs. Not just ones from declarations
          // we expect to have URLs.
          Css::Values* values = decl->mutable_values();
          for (size_t value_index = 0; value_index < values->size();
               ++value_index) {
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
        }
      }
    }
  }

  return result;
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
  Write(Css::EscapeUrl(string_url));
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
    for (++iter; iter != end && Equals((*first)->media_queries(),
                                       (*iter)->media_queries()); ++iter) {
      Write(sep);
      MinifyRulesetIgnoringMedia(**iter);
    }
    MinifyRulesetMediaEnd(**first);
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
    Write(Css::EscapeString(*iter));
    Write("\";");
  }
}

void CssMinify::Minify(const Css::Import& import) {
  Write("@import url(");
  WriteURL(import.link());
  Write(") ");
  JoinMinify(import.media_queries(), ",");
  Write(";");
}

void CssMinify::Minify(const Css::MediaQuery& media_query) {
  switch (media_query.qualifier()) {
    case Css::MediaQuery::ONLY:
      Write("only ");
      break;
    case Css::MediaQuery::NOT:
      Write("not ");
      break;
    case Css::MediaQuery::NO_QUALIFIER:
      break;
  }

  Write(Css::EscapeIdentifier(media_query.media_type()));
  if (!media_query.media_type().empty() && !media_query.expressions().empty()) {
    Write(" and ");
  }
  JoinMinify(media_query.expressions(), " and ");
}

void CssMinify::Minify(const Css::MediaExpression& expression) {
  Write("(");
  Write(Css::EscapeIdentifier(expression.name()));
  if (expression.has_value()) {
    Write(":");
    // Note: expression.value() is not a string, but it is not an identifier
    // either. For example, we don't want to escape space in 20 < width < 300.
    Write(Css::EscapeString(expression.value()));
  }
  Write(")");
}

void CssMinify::MinifyRulesetIgnoringMedia(const Css::Ruleset& ruleset) {
  switch (ruleset.type()) {
    case Css::Ruleset::RULESET:
      if (ruleset.selectors().is_dummy()) {
        Write(ruleset.selectors().bytes_in_original_buffer());
      } else {
        JoinMinify(ruleset.selectors(), ",");
      }
      Write("{");
      JoinMinify(ruleset.declarations(), ";");
      Write("}");
      break;
    case Css::Ruleset::UNPARSED_REGION:
      Minify(*ruleset.unparsed_region());
      break;
  }
}

void CssMinify::MinifyRulesetMediaStart(const Css::Ruleset& ruleset) {
  if (!ruleset.media_queries().empty()) {
    Write("@media ");
    JoinMinify(ruleset.media_queries(), ",");
    Write("{");
  }
}

void CssMinify::MinifyRulesetMediaEnd(const Css::Ruleset& ruleset) {
  if (!ruleset.media_queries().empty()) {
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
  // SimpleSelector::ToString is already basically minified (and is escaped).
  Write(sselector.ToString());
}

namespace {

bool IsValueNormalIdentifier(const Css::Value& value) {
  return (value.GetLexicalUnitType() == Css::Value::IDENT &&
          value.GetIdentifier().ident() == Css::Identifier::NORMAL);
}

}  // namespace

void CssMinify::MinifyFont(const Css::Values& font_values) {
  CHECK_LE(5U, font_values.size());

  // font-style: defaults to normal
  if (!IsValueNormalIdentifier(*font_values.get(0))) {
    Minify(*font_values.get(0));
    Write(" ");
  }
  // font-variant: defaults to normal
  if (!IsValueNormalIdentifier(*font_values.get(1))) {
    Minify(*font_values.get(1));
    Write(" ");
  }
  // font-weight: defaults to normal
  if (!IsValueNormalIdentifier(*font_values.get(2))) {
    Minify(*font_values.get(2));
    Write(" ");
  }
  // font-size is required
  Minify(*font_values.get(3));
  // line-height: defaults to normal
  if (!IsValueNormalIdentifier(*font_values.get(4))) {
    Write("/");
    Minify(*font_values.get(4));
  }
  // font-family:
  for (int i = 5, n = font_values.size(); i < n; ++i) {
    Write(i == 5 ? " " : ",");
    Minify(*font_values.get(i));
  }
}

void CssMinify::Minify(const Css::Declaration& declaration) {
  if (declaration.prop() == Css::Property::UNPARSEABLE) {
    Write(declaration.bytes_in_original_buffer());
  } else {
    Write(Css::EscapeIdentifier(declaration.prop_text()));
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
          MinifyFont(*declaration.values());
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
      GoogleString buffer;
      StringPiece number_string;
      if (!value.bytes_in_original_buffer().empty()) {
        // All parsed values should have verbatim bytes set and we use them
        // to ensure we keep the original precision.
        number_string = value.bytes_in_original_buffer();
      } else {
        // Values added or modified outside of the parsing code need
        // to be converted to strings by us.
        buffer = StringPrintf("%.16g", value.GetFloatValue());
        number_string = buffer;
      }
      if (number_string.starts_with("0.")) {
        // Optimization: Strip "0.25" -> ".25".
        Write(number_string.substr(1));
      } else if (number_string.starts_with("-0.")) {
        // Optimization: Strip "-0.25" -> "-.25".
        Write("-");
        Write(number_string.substr(2));
      } else {
        // Otherwise just print the original string.
        Write(number_string);
      }

      GoogleString unit = value.GetDimensionUnitText();
      // Unit can be either "%" or an identifier.
      if (unit != "%") {
        unit = Css::EscapeIdentifier(unit);
      }
      Write(unit);
      break;
    }
    case Css::Value::URI:
      Write("url(");
      WriteURL(value.GetStringValue());
      Write(")");
      break;
    case Css::Value::FUNCTION:
      Write(Css::EscapeIdentifier(value.GetFunctionName()));
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
      if (!value.bytes_in_original_buffer().empty()) {
        // All parsed strings should have verbatim bytes set.
        // Note: bytes_in_original_buffer() contains quote chars.
        Write(value.bytes_in_original_buffer());
      } else {
        // Strings added or modified outside of the parsing code will need
        // to be serialized by us.
        Write("\"");
        Write(Css::EscapeString(value.GetStringValue()));
        Write("\"");
      }
      break;
    case Css::Value::IDENT:
      Write(Css::EscapeIdentifier(value.GetIdentifierText()));
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

void CssMinify::Minify(const Css::UnparsedRegion& unparsed_region) {
  Write(unparsed_region.bytes_in_original_buffer());
}


bool CssMinify::Equals(const Css::MediaQueries& a,
                       const Css::MediaQueries& b) const {
  if (a.size() != b.size()) {
    return false;
  }
  for (int i = 0, n = a.size(); i < n; ++i) {
    if (!Equals(*a.at(i), *b.at(i))) {
      return false;
    }
  }
  return true;
}

bool CssMinify::Equals(const Css::MediaQuery& a,
                       const Css::MediaQuery& b) const {
  if (a.qualifier() != b.qualifier() ||
      a.media_type() != b.media_type() ||
      a.expressions().size() != b.expressions().size()) {
    return false;
  }
  for (int i = 0, n = a.expressions().size(); i < n; ++i) {
    if (!Equals(a.expression(i), b.expression(i))) {
      return false;
    }
  }
  return true;
}

bool CssMinify::Equals(const Css::MediaExpression& a,
                       const Css::MediaExpression& b) const {
  if (a.name() != b.name() ||
      a.has_value() != b.has_value()) {
    return false;
  }
  if (a.has_value() && a.value() != b.value()) {
    return false;
  }
  return true;
}

}  // namespace net_instaweb
