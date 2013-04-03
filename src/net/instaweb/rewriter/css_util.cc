/*
 * Copyright 2011 Google Inc.
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

// Author: nforman@google.com (Naomi Forman)

#include "net/instaweb/rewriter/public/css_util.h"

#include <vector>

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/media.h"
#include "webutil/css/parser.h"
#include "webutil/css/property.h"
#include "webutil/css/selector.h"
#include "webutil/css/value.h"

namespace net_instaweb {

namespace css_util {

// Extract the numerical value from a values vector.
// TODO(nforman): Allow specification what what style of numbers we can handle.
int GetValueDimension(const Css::Values* values) {
  for (Css::Values::const_iterator value_iter = values->begin();
       value_iter != values->end(); ++value_iter) {
    Css::Value* value = *value_iter;
    if ((value->GetLexicalUnitType() == Css::Value::NUMBER)
        && (value->GetDimension() == Css::Value::PX)) {
      return value->GetIntegerValue();
    }
  }
  return kNoValue;
}

DimensionState GetDimensions(Css::Declarations* decls,
                             int* width, int* height) {
  bool has_width = false;
  bool has_height = false;
  *width = kNoValue;
  *height = kNoValue;
  for (Css::Declarations::iterator decl_iter = decls->begin();
       decl_iter != decls->end() && (!has_width || !has_height); ++decl_iter) {
    Css::Declaration* decl = *decl_iter;
    switch (decl->prop()) {
      case Css::Property::WIDTH: {
        *width = GetValueDimension(decl->values());
        has_width = true;
        break;
      }
      case Css::Property::HEIGHT: {
        *height = GetValueDimension(decl->values());
        has_height = true;
        break;
      }
      default:
        break;
    }
  }
  if (has_width && has_height && *width != kNoValue && *height != kNoValue) {
    return kHasBothDimensions;
  } else if ((has_width && *width == kNoValue) ||
             (has_height && *height == kNoValue)) {
    return kNotParsable;
  } else if (has_width) {
    return kHasWidthOnly;
  } else if (has_height) {
    return kHasHeightOnly;
  }
  return kNoDimensions;
}

StyleExtractor::StyleExtractor(HtmlElement* element)
    : decls_(GetDeclsFromElement(element)),
      width_px_(kNoValue),
      height_px_(kNoValue) {
  if (decls_.get() != NULL) {
    state_ = GetDimensions(decls_.get(), &width_px_, &height_px_);
  } else {
    state_ = kNoDimensions;
  }
}

StyleExtractor::~StyleExtractor() {}

// Return a Declarations* from the style attribute of an element.  If
// there is no style, return NULL.
Css::Declarations* StyleExtractor::GetDeclsFromElement(HtmlElement* element) {
  HtmlElement::Attribute* style = element->FindAttribute(HtmlName::kStyle);
  if ((style != NULL) && (style->DecodedValueOrNull() != NULL)) {
    Css::Parser parser(style->DecodedValueOrNull());
    return parser.ParseDeclarations();
  }
  return NULL;
}

void VectorizeMediaAttribute(const StringPiece& input_media,
                             StringVector* output_vector) {
  // Split on commas, trim whitespace from each element found, delete empties.
  // Note that we hand trim because SplitStringPieceToVector() trims elements
  // of zero length but not those comprising one or more whitespace chars.
  StringPieceVector media_vector;
  SplitStringPieceToVector(input_media, ",", &media_vector, false);
  std::vector<StringPiece>::iterator it;
  for (it = media_vector.begin(); it != media_vector.end(); ++it) {
    TrimWhitespace(&(*it));
    if (StringCaseEqual(*it, kAllMedia)) {
      // Special case: an element of value 'all'.
      output_vector->clear();
      break;
    } else if (!it->empty()) {
      it->CopyToString(StringVectorAdd(output_vector));
    }
  }

  return;
}

GoogleString StringifyMediaVector(const StringVector& input_media) {
  GoogleString result;
  // Special case: inverse of the special rule in the vectorize function.
  if (input_media.empty()) {
    result = kAllMedia;
  } else {
    AppendJoinCollection(&result, input_media, ",");
  }
  return result;
}

bool IsComplexMediaQuery(const Css::MediaQuery& query) {
  return (query.qualifier() != Css::MediaQuery::NO_QUALIFIER ||
          !query.expressions().empty());
}

bool ConvertMediaQueriesToStringVector(const Css::MediaQueries& in_vector,
                                       StringVector* out_vector) {
  out_vector->clear();
  Css::MediaQueries::const_iterator iter;
  for (iter = in_vector.begin(); iter != in_vector.end(); ++iter) {
    // Reject complex media queries immediately.
    if (IsComplexMediaQuery(**iter)) {
      out_vector->clear();
      return false;
    } else {
      const UnicodeText& media_type = (*iter)->media_type();
      StringPiece element(media_type.utf8_data(), media_type.utf8_length());
      TrimWhitespace(&element);
      if (!element.empty()) {
        element.CopyToString(StringVectorAdd(out_vector));
      }
    }
  }
  return true;
}

void ConvertStringVectorToMediaQueries(const StringVector& in_vector,
                                       Css::MediaQueries* out_vector) {
  out_vector->Clear();
  std::vector<GoogleString>::const_iterator iter;
  for (iter = in_vector.begin(); iter != in_vector.end(); ++iter) {
    StringPiece element(*iter);
    TrimWhitespace(&element);
    if (!element.empty()) {
      Css::MediaQuery* query = new Css::MediaQuery;
      query->set_media_type(UTF8ToUnicodeText(element.data(), element.size()));
      out_vector->push_back(query);
    }
  }
}

void ClearVectorIfContainsMediaAll(StringVector* media) {
  StringVector::const_iterator iter;
  for (iter = media->begin(); iter != media->end(); ++iter) {
    if (StringCaseEqual(*iter, kAllMedia)) {
      media->clear();
      break;
    }
  }
}

namespace {

// Does the given data start with the given word followed by whitespace, '(', or
// end of string?  If so, strip the token and spaces and return true.  Otherwise
// return false and leave data alone.
bool StartsWithWord(const StringPiece& word, StringPiece* data) {
  // Make a local copy, so we only shorten on success.
  StringPiece local(*data);
  if (!local.starts_with(word)) {
    return false;
  }
  local.remove_prefix(word.size());
  if (TrimLeadingWhitespace(&local) ||
      local.empty() ||
      local[0] == '(') {
    *data = local;
    return true;
  }
  return false;
}

}  // namespace

bool CanMediaAffectScreen(const StringPiece& media) {
  // TODO(jmaessen): re-implement via CSS parser once it has an entry point for
  // media parsing.
  if (media.empty()) {
    // Media type "" appears to be either screen or all depending on spec
    // version, and affects the screen either way.
    return true;
  }
  StringPieceVector media_vector;
  SplitStringPieceToVector(media, ",", &media_vector, true);
  for (int i = 0, n = media_vector.size(); i < n; ++i) {
    StringPiece current(media_vector[i]);
    TrimLeadingWhitespace(&current);
    // Recognize a CSS3 media query.  We are generous in our recognition here:
    // we'll take anything that contains "screen" or "all" as a token.  Compare
    // with http://www.w3.org/TR/css3-mediaqueries/ which is relatively strict.
    // Note that we rely on the fact that the media itself must come first, so
    // we stop once we've seen that or a left paren.  Also, we don't require
    // whitespace before (.
    // First, we strip a leading "only" if it exists.  This is a no-op in CSS3
    // (but causes CSS2 to not use this rule).
    StartsWithWord("only", &current);
    bool initial_not = StartsWithWord("not", &current);
    if (StartsWithWord("screen", &current) ||
        StartsWithWord("all", &current) ||
        current.empty() ||
        current[0] == '(') {
      // Affects screen, unless there was an initial not.
      if (!initial_not) {
        return true;
      }
    } else if (initial_not) {
      // Something like "not print" that affects screen.
      return true;
    }
  }
  return false;
}

GoogleString JsDetectableSelector(const Css::Selector& selector) {
  // Create a temporary selector representing the desired result that shares
  // structure with the given selector.  We do this because a SimpleSelector
  // isn't copyable without about another page of code.  We're only creating
  // this AST fragment locally and throwing it away.
  Css::Selector trimmed;
  for (int i = 0, n = selector.size(); i < n; ++i) {
    Css::SimpleSelectors* simple_selectors = selector[i];
    scoped_ptr<Css::SimpleSelectors> trimmed_selectors(
        new Css::SimpleSelectors(simple_selectors->combinator()));
    for (int j = 0, m = simple_selectors->size(); j < m; ++j) {
      Css::SimpleSelector* simple_selector = (*simple_selectors)[j];
      // For now we simply discard all pseudoclass attributes.
      // TODO(jmaessen): Only discard pseudoclass attributes that
      // refer to UI elements or dynamic pseudo-classes; see
      // http://www.w3.org/TR/selectors/#pseudo-classes
      if (simple_selector->type() != Css::SimpleSelector::PSEUDOCLASS) {
        trimmed_selectors->push_back(simple_selector);
      }
    }
    if (trimmed_selectors->empty()) {
      // If there's no simple selector at this point, our combinators may have
      // gotten messed up.  Conservatively truncate the Css selector.  This
      // should be difficult in practice, as it requires rules like "p > :hover
      // > a" whose exact interpretation are ambiguous.  We'll truncate such a
      // rule to "p".  Note that rules like "p :hover a" should end up sensibly
      // as "p a".
      break;
    }
    trimmed.push_back(trimmed_selectors.release());
  }
  GoogleString result(trimmed.ToString());
  for (int i = 0, n = trimmed.size(); i < n; ++i) {
    // Remove the SimpleSelector objects without cleaning them up, since we
    // don't own them.
    trimmed[i]->clear();
  }
  return result;
}

}  // namespace css_util

}  // namespace net_instaweb
