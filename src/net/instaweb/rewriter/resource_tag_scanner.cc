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

// Authors: jmarantz@google.com (Joshua Marantz)
//          jefftk@google.com (Jeff Kaufman)

#include "net/instaweb/rewriter/public/resource_tag_scanner.h"

#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/http/public/semantic_type.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {
namespace resource_tag_scanner {

const char kIcon[] = "icon";  // favicons

// See http://developer.apple.com/library/ios/#DOCUMENTATION/
//   AppleApplications/Reference/SafariWebContent/ConfiguringWebApplications/
//   ConfiguringWebApplications.html
const char kAppleTouchIcon[] = "apple-touch-icon";
const char kAppleTouchIconPrecomposed[] = "apple-touch-icon-precomposed";
const char kAppleTouchStartupImage[] = "apple-touch-startup-image";

// Below are the values of the "rel" attribute of LINK tag which are relevant to
// DNS prefetch.
const char kRelPrefetch[] = "prefetch";
const char kRelDnsPrefetch[] = "dns-prefetch";

const char kAttrValImage[] = "image";  // <input type="image" src=...>

namespace {

bool IsAttributeValid(HtmlElement::Attribute* attr) {
  return attr != NULL && !attr->decoding_error();
}

semantic_type::Category CategorizeAttributeBySpec(
    const HtmlElement* element, HtmlName::Keyword attribute_name) {
  switch (element->keyword()) {
    case HtmlName::kLink: {
      // See http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#linkTypes
      if (attribute_name != HtmlName::kHref) {
        return semantic_type::kUndefined;
      }
      const HtmlElement::Attribute* rel_attr =
          element->FindAttribute(HtmlName::kRel);
      if (rel_attr == NULL) {
        return semantic_type::kHyperlink;
      }
      if (CssTagScanner::IsStylesheetOrAlternate(
              rel_attr->DecodedValueOrNull())) {
        return semantic_type::kStylesheet;
      }
      // Ignore keywords we don't recognize, to deal with cases like
      // "shortcut icon" where shortcut is simply ignored.
      StringPieceVector values;
      SplitStringPieceToVector(rel_attr->DecodedValueOrNull(),
                               " ", &values, true /* skip empty */);
      // Image takes precedence over prefetch, so look for images first.
      for (int i = 0, n = values.size(); i < n; ++i) {
        if (StringCaseEqual(values[i], kIcon) ||
            StringCaseEqual(values[i], kAppleTouchIcon) ||
            StringCaseEqual(values[i], kAppleTouchIconPrecomposed) ||
            StringCaseEqual(values[i], kAppleTouchStartupImage)) {
          return semantic_type::kImage;
        }
      }
      for (int i = 0, n = values.size(); i < n; ++i) {
        if (StringCaseEqual(values[i], kRelPrefetch) ||
            StringCaseEqual(values[i], kRelDnsPrefetch)) {
          return semantic_type::kPrefetch;
        }
      }
      return semantic_type::kUndefined;
    }
    case HtmlName::kScript:
      return (attribute_name == HtmlName::kSrc ?
              semantic_type::kScript : semantic_type::kUndefined);
    case HtmlName::kImg:
      if (attribute_name == HtmlName::kSrc) {
        return semantic_type::kImage;
      }
      if (attribute_name == HtmlName::kLongdesc) {
        return semantic_type::kHyperlink;
      }
      return semantic_type::kUndefined;
    case HtmlName::kBody:
      if (attribute_name == HtmlName::kBackground) {
        return semantic_type::kImage;
      }
      if (attribute_name == HtmlName::kCite) {
        return semantic_type::kHyperlink;
      }
      return semantic_type::kUndefined;
    case HtmlName::kTd:
    case HtmlName::kTh:
    case HtmlName::kTable:
    case HtmlName::kTbody:
    case HtmlName::kTfoot:
    case HtmlName::kThead:
      return (attribute_name == HtmlName::kBackground ?
              semantic_type::kImage : semantic_type::kUndefined);
    case HtmlName::kInput:
      if (attribute_name == HtmlName::kFormaction) {
        return semantic_type::kHyperlink;
      }
      if (StringCaseEqual(element->AttributeValue(HtmlName::kType),
                          kAttrValImage) &&
          attribute_name == HtmlName::kSrc) {
        return semantic_type::kImage;
      }
      return semantic_type::kUndefined;
    case HtmlName::kCommand:
      return (attribute_name == HtmlName::kIcon ?
              semantic_type::kImage : semantic_type::kUndefined);
    case HtmlName::kA:
    case HtmlName::kArea:
      return (attribute_name == HtmlName::kHref ?
              semantic_type::kHyperlink : semantic_type::kUndefined);
    case HtmlName::kForm:
      return (attribute_name == HtmlName::kAction ?
              semantic_type::kHyperlink : semantic_type::kUndefined);
    case HtmlName::kVideo:
      if (attribute_name == HtmlName::kSrc) {
        return semantic_type::kOtherResource;
      }
      if (attribute_name == HtmlName::kPoster) {
        return semantic_type::kImage;
      }
      return semantic_type::kUndefined;
    case HtmlName::kAudio:
    case HtmlName::kSource:
    case HtmlName::kTrack:
    case HtmlName::kEmbed:
      return (attribute_name == HtmlName::kSrc ?
              semantic_type::kOtherResource : semantic_type::kUndefined);
    case HtmlName::kFrame:
    case HtmlName::kIframe:
      if (attribute_name == HtmlName::kSrc) {
        return semantic_type::kOtherResource;
      }
      if (attribute_name == HtmlName::kLongdesc) {
        return semantic_type::kHyperlink;
      }
      return semantic_type::kUndefined;
    case HtmlName::kHtml:
      return (attribute_name == HtmlName::kManifest ?
              semantic_type::kOtherResource : semantic_type::kUndefined);
    case HtmlName::kBlockquote:
    case HtmlName::kQ:
    case HtmlName::kIns:
    case HtmlName::kDel:
      return (attribute_name == HtmlName::kCite ?
              semantic_type::kHyperlink : semantic_type::kUndefined);
    case HtmlName::kButton:
      return (attribute_name == HtmlName::kFormaction ?
              semantic_type::kHyperlink : semantic_type::kUndefined);
    default:
      break;
  }
  return semantic_type::kUndefined;
}

}  // namespace

semantic_type::Category CategorizeAttribute(
    const HtmlElement* element,
    const HtmlElement::Attribute* attribute,
    const RewriteOptions* options) {

  if (attribute == NULL) {
    return semantic_type::kUndefined;
  }

  // Handle spec-defined attributes.
  semantic_type::Category spec_category =
      CategorizeAttributeBySpec(element, attribute->keyword());
  if (spec_category != semantic_type::kUndefined) {
    return spec_category;
  }

  // Handle user-defined attributes.
  for (int i = 0, n = options->num_url_valued_attributes(); i < n; ++i) {
    StringPiece element_i;
    StringPiece attribute_i;
    semantic_type::Category category_i;
    options->UrlValuedAttribute(i, &element_i, &attribute_i, &category_i);
    if (StringCaseEqual(element->name_str(), element_i) &&
        StringCaseEqual(attribute->name_str(), attribute_i)) {
      return category_i;
    }
  }
  return semantic_type::kUndefined;
}

void ScanElement(HtmlElement* element,
                 const RewriteOptions* options,
                 UrlCategoryVector* attributes) {
  HtmlElement::AttributeList* attrs = element->mutable_attributes();
  for (HtmlElement::AttributeIterator i = attrs->begin();
       i != attrs->end(); ++i) {
    UrlCategoryPair url_category_pair;
    url_category_pair.url = i.Get();
    if (IsAttributeValid(url_category_pair.url)) {
      url_category_pair.category = CategorizeAttribute(
          element, url_category_pair.url, options);
      if (url_category_pair.category != semantic_type::kUndefined) {
        attributes->push_back(url_category_pair);
      }
    }
  }
}

}  // namespace resource_tag_scanner
}  // namespace net_instaweb
