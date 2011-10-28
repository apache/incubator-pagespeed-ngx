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

// Author: matterbury@google.com (Matt Atterbury)

#include "net/instaweb/rewriter/public/css_inline_import_to_link_filter.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/htmlparse/public/doctype.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/htmlparse/public/html_node.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

namespace {

// names for Statistics variables.
const char kCssImportsToLinks[] = "css_imports_to_links";

// If style elements contents is more than this number of bytes we won't even
// check to see if it's an @import, because URLs are generally considered to
// be at most 2083 bytes (an IE limitation).
const size_t kMaxCssToSave = 4096;

void ConvertUnicodeVectorToStringPieceVector(
    const std::vector<UnicodeText>& in_vector,
    StringPieceVector* out_vector) {
  std::vector<UnicodeText>::const_iterator iter;
  for (iter = in_vector.begin(); iter != in_vector.end(); ++iter) {
    out_vector->push_back(StringPiece(iter->utf8_data(), iter->utf8_length()));
  }
}

void VectorizeMediaAttribute(const StringPiece& input_media,
                             StringPieceVector* output_vector) {
  // Split on commas, trim whitespace from each element found.
  SplitStringPieceToVector(
      input_media, ",", output_vector, false);
  for (std::vector<StringPiece>::iterator iter = output_vector->begin();
       iter != output_vector->end(); ++iter) {
    TrimWhitespace(&(*iter));
  }
  return;
}

bool CompareMediaVectors(const StringPieceVector& style_media,
                         const StringPieceVector& import_media) {
  // No import media is ok since we'll just use whatever the style has.
  if (import_media.empty()) return true;

  // Otherwise, sort both lists and compare elements, ignoring empty elements.
  StringPieceVector style_copy(style_media);
  StringPieceVector import_copy(import_media);
  std::sort(style_copy.begin(), style_copy.end());
  std::sort(import_copy.begin(), import_copy.end());
  std::vector<StringPiece>::iterator style_iter = style_copy.begin();
  std::vector<StringPiece>::iterator import_iter = import_copy.begin();
  while (style_iter != style_copy.end() && import_iter != import_copy.end()) {
    if (style_iter->empty()) {
      ++style_iter;
    } else if (import_iter->empty()) {
      ++import_iter;
    } else if (*style_iter == *import_iter) {
      ++style_iter;
      ++import_iter;
    } else {
      return false;
    }
  }
  while (style_iter != style_copy.end() && style_iter->empty()) {
    ++style_iter;
  }
  while (import_iter != import_copy.end() && import_iter->empty()) {
    ++import_iter;
  }
  return (style_iter == style_copy.end() && import_iter == import_copy.end());
}

GoogleString StringifyMediaVector(const StringPieceVector& import_media) {
  if (import_media.empty()) return "";
  StringPieceVector::const_iterator iter = import_media.begin();
  GoogleString result(iter->data(), iter->length());
  for (++iter; iter != import_media.end(); ++iter) {
    result = StrCat(result, ",", *iter);
  }
  return result;
}

}  // namespace

CssInlineImportToLinkFilter::CssInlineImportToLinkFilter(RewriteDriver* driver,
                                                         Statistics* statistics)
    : driver_(driver),
      counter_(statistics->GetVariable(kCssImportsToLinks)) {
  ResetState();
}

CssInlineImportToLinkFilter::~CssInlineImportToLinkFilter() {}

void CssInlineImportToLinkFilter::Initialize(Statistics* statistics) {
  if (statistics != NULL) {
    statistics->AddVariable(kCssImportsToLinks);
  }
}

void CssInlineImportToLinkFilter::StartDocument() {
  ResetState();
}

void CssInlineImportToLinkFilter::EndDocument() {
  ResetState();
}

void CssInlineImportToLinkFilter::StartElement(HtmlElement* element) {
  DCHECK(style_element_ == NULL);  // HTML Parser guarantees this.
  if (style_element_ == NULL && element->keyword() == HtmlName::kStyle) {
    // The contents are ok to rewrite iff its type is text/css or it has none.
    // See http://www.w3.org/TR/html5/semantics.html#the-style-element
    const char* type = element->AttributeValue(HtmlName::kType);
    if (type == NULL || strcmp(type, kContentTypeCss.mime_type()) == 0) {
      style_element_ = element;
      style_characters_ = NULL;
    }
  }
}

void CssInlineImportToLinkFilter::EndElement(HtmlElement* element) {
  if (style_element_ == element) {
    InlineImportToLinkStyle();
    ResetState();
  }
}

void CssInlineImportToLinkFilter::Characters(HtmlCharactersNode* characters) {
  if (style_element_ != NULL) {
    DCHECK(style_characters_ == NULL);  // HTML Parser guarantees this.
    if (characters->contents().size() > kMaxCssToSave) {
      driver_->InfoHere("Inline element not rewritten because its size "
                        "is above threshold %ld", (long) kMaxCssToSave);
      ResetState();
    } else {
      style_characters_ = characters;
    }
  }
}

void CssInlineImportToLinkFilter::Flush() {
  // If we were flushed in a style element, we cannot rewrite it.
  if (style_element_ != NULL) {
    ResetState();
  }
}

void CssInlineImportToLinkFilter::ResetState() {
  style_element_ = NULL;
  style_characters_ = NULL;
}

// Change the <style>...</style> element into a <link/> element.
void CssInlineImportToLinkFilter::InlineImportToLinkStyle() {
  // Conditions for rewriting a style element to a link element:
  // * The element isn't empty.
  // * The element is rewritable.
  // * Its contents are a single valid @import statement.
  // * It actually imports something (the url isn't empty).
  // * It doesn't already have an href or rel attribute, since we add these.
  // * The @import's media, if any, are the same as style's, if any.
  if (style_characters_ != NULL && driver_->IsRewritable(style_element_)) {
    Css::Parser parser(style_characters_->contents());
    scoped_ptr<Css::Import> import(parser.ParseAsSingleImport());
    if (import.get() != NULL &&
        style_element_->FindAttribute(HtmlName::kHref) == NULL &&
        style_element_->FindAttribute(HtmlName::kRel) == NULL) {
      StringPiece url(import->link.utf8_data(), import->link.utf8_length());
      if (!url.empty()) {
        bool import_media_ok = false;
        const HtmlElement::Attribute* media_attribute =
            style_element_->FindAttribute(HtmlName::kMedia);
        // Cater for simple cases first for performance reasons.
        if (import->media.empty()) {
          import_media_ok = true;
        } else if (media_attribute != NULL &&
                   import->media.size() == 1 &&
                   media_attribute->value() == StringPiece(
                       import->media[0].utf8_data(),
                       import->media[0].utf8_length())) {
          import_media_ok = true;
        } else {
          // If the style has media then the @import may specify no media or the
          // same media; if the style has no media use the @import's, if any.
          StringPieceVector import_media;
          ConvertUnicodeVectorToStringPieceVector(import->media, &import_media);

          if (media_attribute != NULL) {
            StringPieceVector style_media;
            VectorizeMediaAttribute(media_attribute->value(), &style_media);
            import_media_ok = CompareMediaVectors(style_media, import_media);
          } else {
            import_media_ok = true;
            // Set the media in the style so it's copied to the link below.
            GoogleString media_text = StringifyMediaVector(import_media);
            driver_->AddAttribute(style_element_, HtmlName::kMedia, media_text);
          }
        }

        if (import_media_ok) {
          // Create new link element to replace the style element with.
          HtmlElement* link_element =
              driver_->NewElement(style_element_->parent(), HtmlName::kLink);
          if (driver_->doctype().IsXhtml()) {
            link_element->set_close_style(HtmlElement::BRIEF_CLOSE);
          }
          driver_->AddAttribute(link_element, HtmlName::kRel,
                                CssTagScanner::kStylesheet);
          driver_->AddAttribute(link_element, HtmlName::kHref, url);
          // Add all style atrributes to link.
          for (int i = 0; i < style_element_->attribute_size(); ++i) {
            const HtmlElement::Attribute& attr = style_element_->attribute(i);
            link_element->AddAttribute(attr);
          }
          // Add link to DOM.
          driver_->InsertElementAfterElement(style_element_, link_element);

          // Remove style element from DOM.
          if (!driver_->DeleteElement(style_element_)) {
            driver_->ErrorHere("Failed to delete inline style element");
          }

          counter_->Add(1);
        }
      }
    }
  }
}

}  // namespace net_instaweb
