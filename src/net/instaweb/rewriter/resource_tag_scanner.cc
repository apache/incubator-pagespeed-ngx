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
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/htmlparse/public/html_element.h"
#include "net/instaweb/htmlparse/public/html_name.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
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

const char kAttrValImage[] = "image";  // <input type="image" src=...>

HtmlElement::Attribute* ScanElement(
    HtmlElement* element,
    RewriteDriver* driver,  // Can be NULL.
    ContentType::Category* category) {
  HtmlName::Keyword keyword = element->keyword();
  HtmlElement::Attribute* attr = NULL;
  *category = ContentType::kUndefined;
  switch (keyword) {
    case HtmlName::kLink: {
      // See http://www.whatwg.org/specs/web-apps/current-work/multipage/
      // links.html#linkTypes
      attr = element->FindAttribute(HtmlName::kHref);
      *category = ContentType::kOtherNonShardable;
      HtmlElement::Attribute* rel_attr = element->FindAttribute(HtmlName::kRel);
      if (rel_attr != NULL) {
        if (StringCaseEqual(rel_attr->DecodedValueOrNull(),
                            CssTagScanner::kStylesheet)) {
          *category = ContentType::kStylesheet;
        } else if (StringCaseEqual(rel_attr->DecodedValueOrNull(),
                                   kIcon) ||
                   StringCaseEqual(rel_attr->DecodedValueOrNull(),
                                   kAppleTouchIcon) ||
                   StringCaseEqual(rel_attr->DecodedValueOrNull(),
                                   kAppleTouchIconPrecomposed) ||
                   StringCaseEqual(rel_attr->DecodedValueOrNull(),
                                   kAppleTouchStartupImage)) {
          *category = ContentType::kImage;
        }
      }
      break;
    }
    case HtmlName::kScript:
      attr = element->FindAttribute(HtmlName::kSrc);
      *category = ContentType::kScript;
      break;
    case HtmlName::kImg:
      attr = element->FindAttribute(HtmlName::kSrc);
      *category = ContentType::kImage;
      break;
    case HtmlName::kBody:
    case HtmlName::kTd:
    case HtmlName::kTh:
    case HtmlName::kTable:
    case HtmlName::kTbody:
    case HtmlName::kTfoot:
    case HtmlName::kThead:
      attr = element->FindAttribute(HtmlName::kBackground);
      *category = ContentType::kImage;
      break;
    case HtmlName::kInput:
      if (StringCaseEqual(element->AttributeValue(HtmlName::kType),
                          kAttrValImage)) {
        attr = element->FindAttribute(HtmlName::kSrc);
        *category = ContentType::kImage;
      } else {
        *category = ContentType::kOtherNonShardable;
      }
      break;
    case HtmlName::kCommand:
      attr = element->FindAttribute(HtmlName::kIcon);
      *category = ContentType::kImage;
      break;
    case HtmlName::kA:
    case HtmlName::kArea:
      attr = element->FindAttribute(HtmlName::kHref);
      *category = ContentType::kOtherNonShardable;
      break;
    case HtmlName::kForm:
      attr = element->FindAttribute(HtmlName::kAction);
      *category = ContentType::kOtherNonShardable;
      break;
    case HtmlName::kAudio:
    case HtmlName::kVideo:
    case HtmlName::kSource:
    case HtmlName::kTrack:
      attr = element->FindAttribute(HtmlName::kSrc);
      *category = ContentType::kOtherShardable;
      break;
    case HtmlName::kHtml:
      attr = element->FindAttribute(HtmlName::kManifest);
      *category = ContentType::kOtherShardable;
      break;
    case HtmlName::kEmbed:
    case HtmlName::kFrame:
    case HtmlName::kIframe:
      attr = element->FindAttribute(HtmlName::kSrc);
      *category = ContentType::kOtherNonShardable;
      break;
    case HtmlName::kBlockquote:
    case HtmlName::kQ:
    case HtmlName::kIns:
    case HtmlName::kDel:
      attr = element->FindAttribute(HtmlName::kCite);
      *category = ContentType::kOtherNonShardable;
      break;
    case HtmlName::kButton:
      attr = element->FindAttribute(HtmlName::kFormaction);
      *category = ContentType::kOtherNonShardable;
      break;
    default:
      break;
  }
  if (attr == NULL || attr->decoding_error() ||
      *category == ContentType::kUndefined) {
    attr = NULL;
    *category = ContentType::kUndefined;
  }
  return attr;
}

}  // namespace resource_tag_scanner
}  // namespace net_instaweb
